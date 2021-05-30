#include "armory/fault_tracer.h"

#include "armory/termcolor.h"

#include <cstring>
#include <iomanip>
#include <iostream>

using namespace armory;

FaultTracer::FaultTracer(const Context& ctx)
{
    m_ctx               = ctx;
    m_verification_mode = true;
}

bool FaultTracer::trace(const Emulator& emulator, FaultCombination& faults, bool start_after_first_fault, bool log_cpu_state)
{
    m_verification_mode       = false;
    m_start_after_first_fault = start_after_first_fault;
    m_log_cpu_state           = log_cpu_state;

    auto exploitable = verify(emulator, faults);

    std::cout << "--------------------------------" << std::endl;
    if (!m_first_fault_reached)
    {
        std::cout << "WARNING: no fault was injected" << std::endl;
    }
    if (exploitable)
    {
        std::cout << "Result: fault is exploitable!" << std::endl;
    }
    else
    {
        std::cout << "Result: fault is " << termcolor::red << "not" << termcolor::reset << " exploitable!" << std::endl;
    }

    m_verification_mode = true;

    return exploitable;
}

bool FaultTracer::verify(const Emulator& emulator, FaultCombination& faults)
{
    using namespace std::placeholders;

    m_disassembler = std::make_unique<Disassembler>(emulator.get_architecture());

    m_first_fault_reached      = false;
    m_faulted_this_instruction = false;

    Emulator emu(emulator);

    std::tuple<FaultTracer*, FaultCombination*> hook_user_data(this, &faults);

    emu.before_fetch_hook.add(&FaultTracer::detect_end_of_execution, this);

    if (!faults.instruction_faults.empty())
    {
        emu.before_fetch_hook.add(&FaultTracer::handle_instruction_faults, (void*)&hook_user_data);
    }

    if (!faults.register_faults.empty())
    {
        emu.before_fetch_hook.add(&FaultTracer::handle_register_faults, (void*)&hook_user_data);
    }

    for (const auto& f : faults.register_faults)
    {
        if (f.model->get_type() == RegisterFaultModel::FaultType::PERMANENT)
        {
            emu.after_register_write_hook.add(&FaultTracer::handle_permanent_register_fault_overwrite, (void*)&hook_user_data);
            break;
        }
    }

    emu.instruction_decoded_hook.add(&FaultTracer::log_instructions, this);
    if (m_log_cpu_state)
    {
        emu.instruction_executed_hook.add(&FaultTracer::log_cpu_state, this);
    }

    m_end_reached = false;

    if (m_ctx.exploitability_model != nullptr)
    {
        m_exploitability_model = m_ctx.exploitability_model->clone();
    }

    auto ret = emu.emulate(m_ctx.emulation_timeout);

    if (!m_verification_mode)
    {
        std::cout << "end of emulation: ";
        switch (ret)
        {
            case ReturnCode::MAX_INSTRUCTIONS_REACHED:
                std::cout << "timeout";
                break;
            case ReturnCode::END_ADDRESS_REACHED:
                std::cout << "end address reached";
                break;
            case ReturnCode::STOP_EMULATION_CALLED:
                std::cout << "stopped by user";
                break;
            default:
                std::cout << ret;
        }
        std::cout << std::endl;
    }

    if (m_end_reached && m_decision != ExploitabilityModel::Decision::EXPLOITABLE)
    {
        m_end_reached = false;
    }

    return m_end_reached;
}

///////////////////////////////////////////
/////////////  PRIVATE  ///////////////////
///////////////////////////////////////////

void FaultTracer::fault_injected()
{
    if (!m_first_fault_reached)
    {
        if (!m_verification_mode && m_start_after_first_fault)
        {
            std::cout << "..." << std::endl;
        }
        m_first_fault_reached = true;
    }
}

void FaultTracer::log_instructions(Emulator& emu, const Instruction& instruction, void* hook_context)
{
    auto inst_ptr = (FaultTracer*)(hook_context);

    if (!inst_ptr->m_verification_mode && (inst_ptr->m_first_fault_reached || !inst_ptr->m_start_after_first_fault))
    {
        inst_ptr->m_disassembler->disassemble(instruction);

        std::cout << std::dec << std::setw(5) << std::setfill(' ') << emu.get_time() << " | ";
        std::cout << std::hex << instruction.address << ": ";
        if (inst_ptr->m_faulted_this_instruction)
            std::cout << termcolor::bright_red;
        std::cout << std::setw(2 * instruction.size) << std::setfill('0') << instruction.encoding;
        std::cout << "  " << inst_ptr->m_disassembler->get_string();
        if (inst_ptr->m_faulted_this_instruction)
            std::cout << termcolor::reset;
        std::cout << std::endl;
    }
}

void FaultTracer::log_cpu_state(Emulator& emu, const Instruction& instruction, void* hook_context)
{
    UNUSED(emu);
    UNUSED(instruction);
    auto inst_ptr = (FaultTracer*)(hook_context);
    if (!inst_ptr->m_verification_mode && (inst_ptr->m_first_fault_reached || !inst_ptr->m_start_after_first_fault))
    {
        for (u32 i = 0; i <= 15; ++i)
        {
            std::cout << "        " << (Register)i << " = " << std::hex << std::setw(8) << std::setfill('0') << emu.read_register((Register)i) << std::endl;
        }
        std::cout << std::endl;
    }
}

void FaultTracer::detect_end_of_execution(Emulator& emu, u32 address, u32 instr_size, void* hook_context)
{
    UNUSED(instr_size);
    auto inst_ptr = (FaultTracer*)(hook_context);
    for (auto end_address : inst_ptr->m_ctx.halting_points)
    {
        if (end_address == address)
        {
            if (inst_ptr->m_exploitability_model == nullptr)
            {
                inst_ptr->m_decision = ExploitabilityModel::Decision::EXPLOITABLE;
            }
            else
            {
                inst_ptr->m_decision = inst_ptr->m_exploitability_model->evaluate(emu, inst_ptr->m_ctx, address);
            }
            if (inst_ptr->m_decision != ExploitabilityModel::Decision::CONTINUE_SIMULATION)
            {
                inst_ptr->m_end_reached = true;
                emu.stop_emulation();
            }
            return;
        }
    }
}

void FaultTracer::handle_instruction_faults(Emulator& emu, u32 address, u32 instr_size, void* hook_context)
{
    UNUSED(instr_size);
    auto [inst_ptr, faults] = *((std::tuple<FaultTracer*, FaultCombination*>*)hook_context);

    inst_ptr->m_faulted_this_instruction = false;

    for (auto& fault : faults->instruction_faults)
    {
        if (fault.model->get_type() == InstructionFaultModel::FaultType::TRANSIENT)
        {
            if (fault.time == emu.get_time() - 1)
            {
                emu.write_memory(fault.address, fault.original_instruction, fault.instr_size);
            }
        }
    }

    for (auto& fault : faults->instruction_faults)
    {
        if (fault.time == emu.get_time())
        {
            emu.read_memory(fault.address, fault.original_instruction, fault.instr_size);
            fault.model->apply(fault);
            emu.write_memory(fault.address, fault.manipulated_instruction, fault.instr_size);
            if (!fault.model->is_permanent())
            {
                inst_ptr->fault_injected();
                inst_ptr->m_faulted_this_instruction = true;
            }
        }

        if (fault.model->is_permanent() && fault.address == address)
        {
            inst_ptr->fault_injected();
            inst_ptr->m_faulted_this_instruction = true;
        }
    }
}

void FaultTracer::handle_register_faults(Emulator& emu, u32 address, u32 instr_size, void* hook_context)
{
    UNUSED(address);
    UNUSED(instr_size);
    auto [inst_ptr, faults] = *((std::tuple<FaultTracer*, FaultCombination*>*)hook_context);

    for (const auto& fault : faults->register_faults)
    {
        if (fault.model->get_type() == RegisterFaultModel::FaultType::TRANSIENT)
        {
            if (emu.get_time() == fault.time + 1)
            {
                emu.write_register(fault.reg, fault.original_value);
                if (!inst_ptr->m_verification_mode)
                {
                    std::cout << "        revert " << fault.reg << " back to " << std::hex << std::setw(8) << std::setfill('0') << fault.original_value << std::endl;
                }
            }
        }
    }

    for (auto& fault : faults->register_faults)
    {
        if (fault.time == emu.get_time())
        {
            fault.original_value = emu.read_register(fault.reg);
            fault.model->apply(fault);
            emu.write_register(fault.reg, fault.manipulated_value);
            inst_ptr->fault_injected();
            if (!inst_ptr->m_verification_mode)
            {
                std::cout << "        " << fault.reg << " : " << std::hex << std::setw(8) << std::setfill('0') << fault.original_value;
                std::cout << " -> " << termcolor::bright_red << std::setw(8) << std::setfill('0') << fault.manipulated_value << termcolor::reset << std::endl;
            }
        }
    }
}

void FaultTracer::handle_permanent_register_fault_overwrite(Emulator& emu, Register reg, u32 value, void* hook_context)
{
    auto [inst_ptr, faults] = *((std::tuple<FaultTracer*, FaultCombination*>*)hook_context);
    for (auto& fault : faults->register_faults)
    {
        if (fault.model->is_permanent() && fault.reg == reg)
        {
            fault.original_value = value;
            fault.model->apply(fault);
            emu.write_register(fault.reg, fault.manipulated_value);
            inst_ptr->fault_injected();
            if (!inst_ptr->m_verification_mode)
            {
                std::cout << "        " << fault.reg << " : " << std::hex << std::setw(8) << std::setfill('0') << fault.original_value;
                std::cout << " -> " << termcolor::bright_red << std::setw(8) << std::setfill('0') << fault.manipulated_value << termcolor::reset << std::endl;
            }
        }
    }
}
