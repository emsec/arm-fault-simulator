#pragma once

#include "armory/fault_model.h"

namespace armory
{
    struct RegisterFault;

    class RegisterFaultModel : public FaultModel
    {
    public:
        using IterationCounter = std::function<u32(const RegisterFault& fault)>;
        using Tester           = std::function<bool(const RegisterFault& fault)>;
        using Injector         = std::function<void(RegisterFault& fault)>;

        enum class FaultType
        {
            PERMANENT,          // the register is faulted at the program beginning and whenever it changes
            UNTIL_OVERWRITE,    // the register is faulted once
            TRANSIENT           // the register is faulted once and the original value is restored after instruction execution
        };

        /*
         * Constructs a new register fault model.
         *
         * @param[in] name - the name of the model
         * @param[in] type - the type of the faults to be injected
         * @param[in] counter - a function that returns, based on a concrete fault, how many iterations of the injector are performed
         * @param[in] tester - a function that returns true, if a concrete fault shall be executed
         * @param[in] injector - a function that sets the "manipulated_value" in the fault, based on the "original_value"
         */
        RegisterFaultModel(const std::string& name, const FaultType& type, const IterationCounter& counter, const Tester& tester, const Injector& injector);

        bool is_permanent() const override;

        FaultType get_type() const;

        u32 get_number_of_iterations(const RegisterFault& fault) const;

        bool is_applicable(const RegisterFault& fault) const;

        void apply(RegisterFault& fault) const;

    private:
        FaultType m_type;
        IterationCounter m_counter;
        Tester m_tester;
        Injector m_injector;
    };

    struct RegisterFault : public Fault
    {
        RegisterFault(const RegisterFaultModel* model, u32 time, u32 fault_model_iteration);
        ~RegisterFault() = default;

        const RegisterFaultModel* model;
        Register reg;
        u32 original_value;
        u32 manipulated_value;

        bool operator==(const RegisterFault& other) const;
        bool operator!=(const RegisterFault& other) const;
        bool operator<(const RegisterFault& other) const;
        bool operator>(const RegisterFault& other) const;
    };

}    // namespace armory

template<>
struct std::hash<armory::RegisterFault>
{
    std::size_t operator()(const armory::RegisterFault& x) const noexcept
    {
        size_t seed = 0;
        armory::hash_combine(seed, x.reg);
        armory::hash_combine(seed, x.model);
        armory::hash_combine(seed, x.time);
        armory::hash_combine(seed, x.fault_model_iteration);
        return seed;
    }
};
