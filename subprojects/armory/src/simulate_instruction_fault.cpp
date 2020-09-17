#include "armory/fault_simulator.h"
#include "m-ulator/emulator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

// #########################################

using namespace armory;

// used as a hook in get_instruction_order
void FaultSimulator::instruction_collector(Emulator& emu, const Instruction& instruction, void* user_data)
{
    if (!emu.is_running())
    {
        return;
    }

    UNUSED(emu);
    auto [inst_ptr, addresses_to_fault] = *((std::tuple<FaultSimulator*, std::vector<std::tuple<u32, u32>>*>*)user_data);

    // check if the current address is in an ignore range
    for (const auto& range : inst_ptr->m_ctx.ignore_memory_ranges)
    {
        if (instruction.address >= range.offset && instruction.address < range.offset + range.size)
        {
            return;
        }
    }
    for (const auto& range : inst_ptr->m_ctx.ignore_time_ranges)
    {
        if (emu.get_time() >= range.start && emu.get_time() < range.end)
        {
            return;
        }
    }

    addresses_to_fault->emplace_back(instruction.address, instruction.size);
}

std::vector<std::tuple<u32, u32>> FaultSimulator::get_instruction_order(ThreadContext& thread_ctx, u32 remaining_cycles)
{
    using namespace std::placeholders;

    std::vector<std::tuple<u32, u32>> addresses_to_fault;

    // strategy: just run the emulator from the current address and collect all executed instructions
    std::tuple<FaultSimulator*, std::vector<std::tuple<u32, u32>>*> hook_user_data(this, &addresses_to_fault);
    u32 hook = thread_ctx.emu.add_instruction_decoded_hook(&FaultSimulator::instruction_collector, &hook_user_data);

    if (m_ctx.exploitability_model != nullptr)
    {
        thread_ctx.exploitability_model = m_ctx.exploitability_model->clone();
    }

    thread_ctx.emu.emulate(remaining_cycles);

    thread_ctx.emu.remove_hook(hook);

    return addresses_to_fault;
}

void FaultSimulator::simulate_instruction_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 fault_model_index, u32 remaining_cycles, const FaultCombination& current_chain)
{
    auto& emu         = thread_ctx.emu;
    auto active_model = dynamic_cast<const InstructionFaultModel*>(m_fault_models[fault_model_index]);

    thread_ctx.snapshots[fault_model_index]->reset();
    thread_ctx.snapshots[fault_model_index]->backup();

    auto addresses_to_fault = get_instruction_order(thread_ctx, remaining_cycles);
    u32 num_fault_addresses = addresses_to_fault.size();

    u32 last_index    = 0;
    u32 current_index = -1;

    u8 original_instruction[4];

    while (true)
    {
        if (fault_model_index == 0)
        {
            std::lock_guard<std::mutex> lock(m_progress_mutex);
            current_index = m_thread_progress;
            m_thread_progress++;
        }
        else
        {
            current_index++;
        }

        if (current_index >= num_fault_addresses)
        {
            break;
        }

        if (m_print_progress && fault_model_index == 0)
        {
            double new_progress = std::round(100.0 * (current_index + 1.0) / num_fault_addresses);
            std::lock_guard<std::mutex> lock(m_progress_mutex);
            if (new_progress != m_progress)
            {
                m_progress = new_progress;
                print_progress();
            }
        }

        thread_ctx.snapshots[fault_model_index]->restore();

        if (m_ctx.exploitability_model != nullptr)
        {
            thread_ctx.exploitability_model = m_ctx.exploitability_model->clone();
        }

        while (last_index <= current_index)
        {
            auto [next_address, unused] = addresses_to_fault.at(last_index);
            UNUSED(unused);

            auto ret = emu.emulate(next_address, remaining_cycles);
            if (ret == ReturnCode::MAX_INSTRUCTIONS_REACHED)
            {
                return;
            }
            if (ret != ReturnCode::END_ADDRESS_REACHED)
            {
                throw std::runtime_error("error which should never occur (" + to_string(ret) + ")");
                return;
            }
            remaining_cycles -= emu.get_emulated_time();
            last_index++;
        }

        if (remaining_cycles == 0)
        {
            return;
        }
        thread_ctx.snapshots[fault_model_index]->backup();

        auto [fault_address, fault_instr_size] = addresses_to_fault.at(current_index);

        bool already_processed = false;

        for (const auto& other : current_chain.instruction_faults)
        {
            if (other.model->is_permanent() && other.address == fault_address)
            {
                already_processed = true;
                break;
            }
        }
        if (already_processed)
            continue;

        emu.read_memory(fault_address, original_instruction, fault_instr_size);

        auto now = emu.get_time();

        u32 iteration = -1;
        while (true)
        {
            iteration++;

            InstructionFault fault(active_model, now, iteration);
            fault.address    = fault_address;
            fault.instr_size = fault_instr_size;
            std::memcpy(fault.original_instruction, original_instruction, fault.instr_size);

            if (iteration >= active_model->get_number_of_iterations(fault))
            {
                break;
            }

            if (!active_model->is_applicable(fault))
            {
                continue;
            }

            active_model->apply(fault);

            if (std::memcmp(fault.original_instruction, fault.manipulated_instruction, fault.instr_size) == 0)
            {
                continue;
            }

            FaultCombination new_chain(current_chain);
            new_chain.instruction_faults.push_back(fault);

            if (is_fault_redundant(new_chain))
            {
                continue;
            }

            thread_ctx.snapshots[fault_model_index]->restore();

            emu.write_memory(fault.address, fault.manipulated_instruction, fault.instr_size);

            thread_ctx.end_reached = false;
            if (m_ctx.exploitability_model != nullptr)
            {
                thread_ctx.exploitability_model = m_ctx.exploitability_model->clone();
            }

            auto ret = emu.emulate(1);
            if (ret != ReturnCode::STOP_EMULATION_CALLED && ret != ReturnCode::MAX_INSTRUCTIONS_REACHED)
            {
                emu.write_memory(fault.address, fault.original_instruction, fault.instr_size);
                continue;
            }

            if (active_model->get_type() == InstructionFaultModel::FaultType::TRANSIENT)
            {
                emu.write_memory(fault.address, fault.original_instruction, fault.instr_size);
            }

            if (!thread_ctx.end_reached && remaining_cycles > 1)
            {
                emu.emulate(remaining_cycles - 1);
            }

            if (thread_ctx.end_reached && thread_ctx.decision == ExploitabilityModel::Decision::EXPLOITABLE)
            {
                thread_ctx.new_faults.push_back(new_chain);
            }
            else if (fault_model_index < m_fault_models.size() - 1 && remaining_cycles > 2)
            {
                thread_ctx.snapshots[fault_model_index]->restore();

                emu.write_memory(fault.address, fault.manipulated_instruction, fault.instr_size);

                if (emu.emulate(1) != ReturnCode::MAX_INSTRUCTIONS_REACHED)
                {
                    emu.write_memory(fault.address, fault.original_instruction, fault.instr_size);
                    continue;
                }

                if (active_model->get_type() == InstructionFaultModel::FaultType::TRANSIENT)
                {
                    emu.write_memory(fault.address, fault.original_instruction, fault.instr_size);
                }

                simulate_fault(thread_ctx, recursion_data, fault_model_index + 1, remaining_cycles - 1, new_chain);
            }

            emu.write_memory(fault.address, fault.original_instruction, fault.instr_size);
        }
    }
}

void FaultSimulator::simulate_permanent_instruction_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 fault_model_index, u32 remaining_cycles, const FaultCombination& current_chain)
{
    auto& emu         = thread_ctx.emu;
    auto active_model = dynamic_cast<const InstructionFaultModel*>(m_fault_models[fault_model_index]);

    thread_ctx.snapshots[fault_model_index]->reset();
    thread_ctx.snapshots[fault_model_index]->backup();

    u32 num_fault_addresses = m_all_instructions.size();

    u32 current_index = recursion_data - 1;

    u8 original_instruction[4];

    auto time = emu.get_time();

    while (true)
    {
        if (fault_model_index == 0)
        {
            std::lock_guard<std::mutex> lock(m_progress_mutex);
            current_index = m_thread_progress;
            m_thread_progress++;
        }
        else
        {
            current_index++;
        }

        if (current_index >= num_fault_addresses)
        {
            break;
        }

        if (m_print_progress && fault_model_index == 0)
        {
            double new_progress = std::round(100.0 * (current_index + 1.0) / num_fault_addresses);
            std::lock_guard<std::mutex> lock(m_progress_mutex);
            if (new_progress != m_progress)
            {
                m_progress = new_progress;
                print_progress();
            }
        }

        auto [fault_address, fault_instr_size] = m_all_instructions.at(current_index);

        bool already_processed = false;
        for (const auto& other : current_chain.instruction_faults)
        {
            if (other.address == fault_address)
            {
                already_processed = true;
                break;
            }
        }
        if (already_processed)
            continue;

        emu.read_memory(fault_address, original_instruction, fault_instr_size);

        u32 iteration = -1;
        while (true)
        {
            iteration++;

            InstructionFault fault(active_model, time, iteration);
            fault.address    = fault_address;
            fault.instr_size = fault_instr_size;

            std::memcpy(fault.original_instruction, original_instruction, fault.instr_size);

            if (iteration >= active_model->get_number_of_iterations(fault))
            {
                break;
            }

            if (!active_model->is_applicable(fault))
            {
                continue;
            }

            active_model->apply(fault);

            if (std::memcmp(fault.original_instruction, fault.manipulated_instruction, fault.instr_size) == 0)
            {
                continue;
            }

            {
                auto [success, instr] = thread_ctx.decoder.decode_instruction(fault.address, fault.manipulated_instruction, fault.instr_size, emu.in_IT_block(), emu.last_in_IT_block());
                UNUSED(instr);
                if (success != ReturnCode::OK)
                {
                    continue;
                }
            }

            FaultCombination new_chain(current_chain);
            new_chain.instruction_faults.push_back(fault);

            if (is_fault_redundant(new_chain))
            {
                continue;
            }

            thread_ctx.snapshots[fault_model_index]->restore();

            emu.write_memory(fault.address, fault.manipulated_instruction, fault.instr_size);

            if (fault_model_index == m_fault_models.size() - 1)
            {
                thread_ctx.end_reached = false;
                if (m_ctx.exploitability_model != nullptr)
                {
                    thread_ctx.exploitability_model = m_ctx.exploitability_model->clone();
                }

                emu.emulate(remaining_cycles);

                if (thread_ctx.end_reached && thread_ctx.decision == ExploitabilityModel::Decision::EXPLOITABLE)
                {
                    thread_ctx.new_faults.push_back(new_chain);
                }
            }
            else
            {
                u32 next_recursion_data = 0;
                if (m_fault_models[fault_model_index + 1] == active_model)
                {
                    next_recursion_data = current_index + 1;
                }
                simulate_fault(thread_ctx, next_recursion_data, fault_model_index + 1, remaining_cycles, new_chain);
            }

            emu.write_memory(fault.address, fault.original_instruction, fault.instr_size);
        }
    }
}
