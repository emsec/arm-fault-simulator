#pragma once

#include "armory/instruction_fault_model.h"
#include "armory/register_fault_model.h"

namespace armory
{
    class FaultCombination
    {
    public:
        FaultCombination();
        FaultCombination(const FaultCombination& other);
        FaultCombination& operator=(const FaultCombination& other);

        /*
         * Adds a fault to this combination.
         */
        void add(const InstructionFault& fault);
        void add(const RegisterFault& fault);

        /*
         * Checks whether this FaultCombination includes another one.
         * If both combinations lead to exploitable faults and one includes the other, only the smaller one needs to be kept.
         */
        bool includes(const FaultCombination& other) const;

        /*
         * Returns a sorted vector of all faults in the combination.
         * The sorted vector is also cached, i.e., first call after adding faults is slower than subsequent calls
         */
        const std::vector<const Fault*>& get_sorted_faults() const;

        bool operator==(const FaultCombination& other) const;
        bool operator!=(const FaultCombination& other) const;
        bool operator<(const FaultCombination& other) const;
        bool operator>(const FaultCombination& other) const;

        u32 size() const;

        /*
         * Direct member access to the vectors of specific faults.
         */
        std::vector<InstructionFault> instruction_faults;
        std::vector<RegisterFault> register_faults;

    private:
        mutable std::vector<const Fault*> m_sorted;
        mutable u32 m_sorted_size;
    };

}    // namespace armory

template<>
struct std::hash<armory::FaultCombination>
{
    std::size_t operator()(const armory::FaultCombination& x) const noexcept
    {
        size_t seed = 0;
        for (auto fault : x.get_sorted_faults())
        {
            if (auto ptr = dynamic_cast<const armory::InstructionFault*>(fault); ptr != nullptr)
            {
                armory::hash_combine(seed, *ptr);
            }
            else if (auto ptr = dynamic_cast<const armory::RegisterFault*>(fault); ptr != nullptr)
            {
                armory::hash_combine(seed, *ptr);
            }
        }
        return seed;
    }
};
