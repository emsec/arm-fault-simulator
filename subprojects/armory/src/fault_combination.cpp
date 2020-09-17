#include "armory/fault_combination.h"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace armory
{
    FaultCombination::FaultCombination()
    {
        m_sorted_size = 0;
    }

    FaultCombination::FaultCombination(const FaultCombination& other) : instruction_faults(other.instruction_faults), register_faults(other.register_faults)
    {
        m_sorted_size = 0;
    }
    FaultCombination& FaultCombination::operator=(const FaultCombination& other)
    {
        instruction_faults = other.instruction_faults;
        register_faults    = other.register_faults;
        m_sorted_size      = 0;
        return *this;
    }

    void FaultCombination::add(const InstructionFault& fault)
    {
        instruction_faults.push_back(fault);
    }

    void FaultCombination::add(const RegisterFault& fault)
    {
        register_faults.push_back(fault);
    }

    static bool is_less_than(const Fault* a, const Fault* b)
    {
        if (auto p_i_a = dynamic_cast<const InstructionFault*>(a))
        {
            if (auto p_i_b = dynamic_cast<const InstructionFault*>(b))
            {
                if ((*p_i_a) < (*p_i_b))
                {
                    return true;
                }
                if ((*p_i_b) < (*p_i_a))
                {
                    return false;
                }
            }
        }
        else if (auto p_r_a = dynamic_cast<const RegisterFault*>(a))
        {
            if (auto p_r_b = dynamic_cast<const RegisterFault*>(b))
            {
                if ((*p_r_a) < (*p_r_b))
                {
                    return true;
                }
                if ((*p_r_b) < (*p_r_a))
                {
                    return false;
                }
            }
        }
        else
        {
            if (a->time != b->time)
                return a->time < b->time;
            if (a->fault_model_iteration != b->fault_model_iteration)
                return a->fault_model_iteration < b->fault_model_iteration;
        }
        return false;
    }

    const std::vector<const Fault*>& FaultCombination::get_sorted_faults() const
    {
        if (m_sorted_size != size())
        {
            m_sorted_size = size();

            m_sorted.clear();
            for (auto& x : instruction_faults)
            {
                m_sorted.push_back(&x);
            }
            for (auto& x : register_faults)
            {
                m_sorted.push_back(&x);
            }

            std::sort(m_sorted.begin(), m_sorted.end(), is_less_than);
        }
        return m_sorted;
    }

    template<typename T>
    static bool custom_includes(const std::vector<T>& a, const std::vector<T>& b)
    {
        size_t i   = 0;
        size_t end = b.size();

        if (i == end)
        {
            return true;
        }

        for (auto& x : a)
        {
            if (x == b[i])
            {
                ++i;
                if (i == end)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool FaultCombination::includes(const FaultCombination& other) const
    {
        if (instruction_faults.size() < other.instruction_faults.size() || register_faults.size() < other.register_faults.size())
        {
            return false;
        }
        return custom_includes(instruction_faults, other.instruction_faults) && custom_includes(register_faults, other.register_faults);
        // return std::includes(instruction_faults.begin(), instruction_faults.end(), other.instruction_faults.begin(), other.instruction_faults.end()) && std::includes(register_faults.begin(), register_faults.end(), other.register_faults.begin(), other.register_faults.end());
    }

    bool FaultCombination::operator==(const FaultCombination& other) const
    {
        if (instruction_faults.size() != other.instruction_faults.size() || register_faults.size() != other.register_faults.size())
        {
            return false;
        }

        return instruction_faults == other.instruction_faults && register_faults == other.register_faults;
    }

    bool FaultCombination::operator!=(const FaultCombination& other) const
    {
        return !(*this == other);
    }

    bool FaultCombination::operator<(const FaultCombination& other) const
    {
        u32 this_size  = size();
        u32 other_size = other.size();
        if (this_size < other_size)
        {
            return true;
        }
        if (this_size > other_size)
        {
            return false;
        }
        auto& this_sorted  = get_sorted_faults();
        auto& other_sorted = other.get_sorted_faults();

        for (u32 i = 0; i < this_size; ++i)
        {
            if (is_less_than(this_sorted[i], other_sorted[i]))
            {
                return true;
            }
            if (is_less_than(other_sorted[i], this_sorted[i]))
            {
                return false;
            }
        }
        return false;
    }

    bool FaultCombination::operator>(const FaultCombination& other) const
    {
        u32 this_size  = size();
        u32 other_size = other.size();
        if (this_size > other_size)
        {
            return true;
        }
        if (this_size < other_size)
        {
            return false;
        }
        auto& this_sorted  = get_sorted_faults();
        auto& other_sorted = other.get_sorted_faults();

        for (u32 i = 0; i < this_size; ++i)
        {
            if (is_less_than(this_sorted[i], other_sorted[i]))
            {
                return false;
            }
            if (is_less_than(other_sorted[i], this_sorted[i]))
            {
                return true;
            }
        }
        return false;
    }

    u32 FaultCombination::size() const
    {
        return instruction_faults.size() + register_faults.size();
    }

}    // namespace armory
