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

void FaultSimulator::add_new_registers_vector(Emulator& emu, const Instruction& instruction, void* user_data)
{
    UNUSED(emu);
    UNUSED(instruction);

    auto [inst_ptr, collection] = *((std::tuple<FaultSimulator*, std::vector<std::vector<Register>>*>*)user_data);

    auto& c = collection->back();

    for (const auto& range : inst_ptr->m_ctx.ignore_memory_ranges)
    {
        if (instruction.address >= range.offset && instruction.address < range.offset + range.size)
        {
            c.clear();
            break;
        }
    }
    for (const auto& range : inst_ptr->m_ctx.ignore_time_ranges)
    {
        if (emu.get_time() >= range.start && emu.get_time() < range.end)
        {
            c.clear();
            break;
        }
    }

    if (!c.empty())
    {
        std::sort(c.begin(), c.end());
        c.erase(std::unique(c.begin(), c.end()), c.end());
    }

    collection->push_back({});
}

static void collect_read_registers(Emulator& emu, Register reg, u32 value, void* user_data)
{
    UNUSED(emu);
    UNUSED(value);
    auto collection = (std::vector<std::vector<Register>>*)user_data;
    collection->back().push_back(reg);
}

static void revert_transient_fault(Emulator& emu, const Instruction& instruction, void* user_data)
{
    UNUSED(instruction);
    auto fault = (RegisterFault*)user_data;
    if (emu.get_time() == fault->time)
    {
        emu.write_register(fault->reg, fault->original_value);
    }
}

void FaultSimulator::simulate_register_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 fault_model_index, u32 remaining_cycles, const FaultCombination& current_chain)
{
    auto& emu         = thread_ctx.emu;
    auto active_model = dynamic_cast<const RegisterFaultModel*>(m_fault_models[fault_model_index]);

    thread_ctx.snapshots[fault_model_index]->reset();
    thread_ctx.snapshots[fault_model_index]->backup();

    std::vector<std::vector<Register>> read_registers;
    read_registers.push_back({});
    {
        if (m_ctx.exploitability_model != nullptr)
        {
            thread_ctx.exploitability_model = m_ctx.exploitability_model->clone();
        }
        std::tuple<FaultSimulator*, std::vector<std::vector<Register>>*> hook_user_data(this, &read_registers);
        auto hook1 = emu.instruction_executed_hook.add(&FaultSimulator::add_new_registers_vector, &hook_user_data);
        auto hook2 = emu.before_register_read_hook.add(&collect_read_registers, &read_registers);
        emu.emulate(remaining_cycles);
        emu.instruction_executed_hook.remove(hook1);
        emu.before_register_read_hook.remove(hook2);
    }
    read_registers.pop_back();

    u32 instruction_count = read_registers.size();
    u32 last_index        = 0;
    u32 current_index     = -1;

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

        if (current_index >= instruction_count)
        {
            break;
        }

        if (read_registers[current_index].empty())
        {
            continue;
        }

        if (m_print_progress && fault_model_index == 0)
        {
            double new_progress = std::round(100.0 * (current_index + 1.0) / instruction_count);
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

        if (last_index < current_index)
        {
            auto ret = emu.emulate(current_index - last_index);
            remaining_cycles -= emu.get_emulated_time();
            last_index = current_index;
            if (ret != ReturnCode::MAX_INSTRUCTIONS_REACHED)
            {
                throw std::runtime_error("error which should never occur (" + to_string(ret) + ") ");
                return;
            }
        }

        if (remaining_cycles == 0)
        {
            return;
        }

        thread_ctx.snapshots[fault_model_index]->backup();

        auto now = emu.get_time();

        for (auto reg : read_registers[current_index])
        {
            bool already_processed = false;
            for (const auto& other : current_chain.register_faults)
            {
                if (other.reg == reg)
                {
                    if (other.model->is_permanent() || other.time == now)
                    {
                        already_processed = true;
                        break;
                    }
                }
            }
            if (already_processed)
            {
                continue;
            }

            thread_ctx.snapshots[fault_model_index]->restore();
            auto original_value = emu.read_register(reg);

            for (const auto& other : current_chain.register_faults)
            {
                if (other.model->get_type() == RegisterFaultModel::FaultType::UNTIL_OVERWRITE && other.reg == reg && other.manipulated_value == original_value)
                {
                    already_processed = true;
                    break;
                }
            }
            if (already_processed)
            {
                continue;
            }

            u32 iteration = -1;
            while (true)
            {
                iteration++;

                RegisterFault fault(active_model, now, iteration);
                fault.reg            = reg;
                fault.original_value = original_value;

                if (iteration >= active_model->get_number_of_iterations(fault))
                {
                    break;
                }

                if (!active_model->is_applicable(fault))
                {
                    continue;
                }

                active_model->apply(fault);

                if (fault.original_value == fault.manipulated_value)
                {
                    continue;
                }

                FaultCombination new_chain(current_chain);
                new_chain.register_faults.push_back(fault);

                if (is_fault_redundant(new_chain))
                {
                    continue;
                }

                thread_ctx.snapshots[fault_model_index]->restore();

                emu.write_register(fault.reg, fault.manipulated_value);

                if (emu.read_register(fault.reg) == fault.original_value)
                {
                    continue;
                }

                u32 recursive_hook = 0;
                if (active_model->get_type() == RegisterFaultModel::FaultType::TRANSIENT)
                {
                    recursive_hook = emu.instruction_executed_hook.add(&revert_transient_fault, &fault);
                }

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
                else if (fault_model_index < m_fault_models.size() - 1 && remaining_cycles > 0)
                {
                    thread_ctx.snapshots[fault_model_index]->restore();

                    emu.write_register(fault.reg, fault.manipulated_value);

                    simulate_fault(thread_ctx, recursion_data, fault_model_index + 1, remaining_cycles, new_chain);
                }

                if (recursive_hook != 0)
                {
                    emu.instruction_executed_hook.remove(recursive_hook);
                    recursive_hook = 0;
                }
            }
        }
    }
}

void FaultSimulator::handle_permanent_register_fault_overwrite(Emulator& emu, Register reg, u32 value, void* hook_context)
{
    auto [inst_ptr, reg_fault] = *((std::tuple<FaultSimulator*, RegisterFault*>*)hook_context);

    if (reg_fault->reg == reg)
    {
        auto addr = emu.read_register(Register::PC) - 4;

        for (const auto& range : inst_ptr->m_ctx.ignore_memory_ranges)
        {
            if (addr >= range.offset && addr < range.offset + range.size)
            {
                return;
            }
        }

        reg_fault->original_value = value;
        reg_fault->model->apply(*reg_fault);
        emu.write_register(reg_fault->reg, reg_fault->manipulated_value);
    }
}

void FaultSimulator::simulate_permanent_register_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 fault_model_index, u32 remaining_cycles, const FaultCombination& current_chain)
{
    auto& emu         = thread_ctx.emu;
    auto active_model = dynamic_cast<const RegisterFaultModel*>(m_fault_models[fault_model_index]);

    thread_ctx.snapshots[fault_model_index]->reset();
    thread_ctx.snapshots[fault_model_index]->backup();

    std::vector<Register> read_registers = {
        Register::R0,
        Register::R1,
        Register::R2,
        Register::R3,
        Register::R4,
        Register::R5,
        Register::R6,
        Register::R7,
        Register::R8,
        Register::R9,
        Register::R10,
        Register::R11,
        Register::R12,
        Register::SP,
        Register::LR,
        Register::PC,
        Register::PSR,
    };

    u32 register_count = read_registers.size();
    u32 current_index  = recursion_data - 1;

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

        if (current_index >= register_count)
        {
            break;
        }

        if (m_print_progress && fault_model_index == 0)
        {
            double new_progress = std::round(100.0 * (current_index + 1.0) / register_count);
            std::lock_guard<std::mutex> lock(m_progress_mutex);
            if (new_progress != m_progress)
            {
                m_progress = new_progress;
                print_progress();
            }
        }

        auto current_reg = read_registers[current_index];

        bool already_processed = false;
        for (const auto& other : current_chain.register_faults)
        {
            if (other.reg == current_reg)
            {
                already_processed = true;
                break;
            }
        }
        if (already_processed)
            continue;

        u32 iteration = -1;

        thread_ctx.snapshots[fault_model_index]->restore();

        auto original_value = emu.read_register(current_reg);

        while (true)
        {
            iteration++;

            RegisterFault fault(active_model, time, iteration);
            fault.reg            = current_reg;
            fault.original_value = original_value;

            if (iteration >= active_model->get_number_of_iterations(fault))
            {
                break;
            }

            if (!active_model->is_applicable(fault))
            {
                continue;
            }

            active_model->apply(fault);

            FaultCombination new_chain(current_chain);
            new_chain.register_faults.push_back(fault);

            if (is_fault_redundant(new_chain))
            {
                continue;
            }

            thread_ctx.snapshots[fault_model_index]->restore();

            emu.write_register(fault.reg, fault.manipulated_value);

            std::tuple<FaultSimulator*, RegisterFault*> hook_user_data(this, &fault);
            auto recursive_hook = emu.after_register_write_hook.add(&FaultSimulator::handle_permanent_register_fault_overwrite, &hook_user_data);

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

            emu.after_register_write_hook.remove(recursive_hook);
        }
    }
}
