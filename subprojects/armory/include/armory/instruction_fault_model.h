#pragma once

#include "armory/fault_model.h"

#include <functional>

namespace armory
{
    struct InstructionFault;

    class InstructionFaultModel : public FaultModel
    {
    public:
        using IterationCounter = std::function<u32(const InstructionFault& fault)>;
        using Tester           = std::function<bool(const InstructionFault& fault)>;
        using Injector         = std::function<void(InstructionFault& fault)>;

        enum class FaultType
        {
            PERMANENT,    // the fault is injected at the beginning of emulation and never removed
            TRANSIENT,    // the fault is injected for a single instruction execution only
        };

        /*
         * Constructs a new instruction fault model.
         *
         * @param[in] name - the name of the model
         * @param[in] type - the type of the faults to be injected
         * @param[in] counter - a function that returns, based on a concrete fault, how many iterations of the injector are performed
         * @param[in] tester - a function that returns true, if a concrete fault shall be executed
         * @param[in] injector - a function that sets the "manipulated_instruction" value in the fault, based on the "original_instruction" value
         */
        InstructionFaultModel(const std::string& name, const FaultType& type, const IterationCounter& counter, const Tester& tester, const Injector& injector);

        bool is_permanent() const override;

        FaultType get_type() const;

        u32 get_number_of_iterations(const InstructionFault& fault) const;

        bool is_applicable(const InstructionFault& fault) const;

        void apply(InstructionFault& fault) const;

    private:
        FaultType m_type;
        IterationCounter m_counter;
        Tester m_tester;
        Injector m_injector;
    };

    struct InstructionFault : public Fault
    {
        InstructionFault(const InstructionFaultModel* model, u32 time, u32 fault_model_iteration);
        ~InstructionFault() = default;

        const InstructionFaultModel* model;
        u32 address;
        u32 instr_size;
        u8 original_instruction[4];
        u8 manipulated_instruction[4];

        bool operator==(const InstructionFault& other) const;
        bool operator!=(const InstructionFault& other) const;
        bool operator<(const InstructionFault& other) const;
        bool operator>(const InstructionFault& other) const;
    };

}    // namespace armory

template<>
struct std::hash<armory::InstructionFault>
{
    std::size_t operator()(const armory::InstructionFault& x) const noexcept
    {
        size_t seed = 0;
        armory::hash_combine(seed, x.address);
        armory::hash_combine(seed, x.model);
        armory::hash_combine(seed, x.time);
        armory::hash_combine(seed, x.fault_model_iteration);
        return seed;
    }
};
