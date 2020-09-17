#include "armory/instruction_fault_model.h"

namespace armory
{
    InstructionFaultModel::InstructionFaultModel(const std::string& name, const FaultType& type, const IterationCounter& counter, const Tester& tester, const Injector& injector) : FaultModel(name)
    {
        m_type     = type;
        m_counter  = counter;
        m_tester   = tester;
        m_injector = injector;
    }

    bool InstructionFaultModel::is_permanent() const
    {
        return m_type == FaultType::PERMANENT;
    }

    InstructionFaultModel::FaultType InstructionFaultModel::get_type() const
    {
        return m_type;
    }

    u32 InstructionFaultModel::get_number_of_iterations(const InstructionFault& fault) const
    {
        return m_counter(fault);
    }

    bool InstructionFaultModel::is_applicable(const InstructionFault& fault) const
    {
        return m_tester(fault);
    }

    void InstructionFaultModel::apply(InstructionFault& fault) const
    {
        m_injector(fault);
    }

    // ########################################################
    // ########################################################
    // ########################################################

    InstructionFault::InstructionFault(const InstructionFaultModel* _model, u32 _time, u32 _fault_model_iteration) : Fault(_time, _fault_model_iteration)
    {
        this->model = _model;
    }

    bool InstructionFault::operator<(const InstructionFault& other) const
    {
        if (time != other.time)
            return time < other.time;
        if (address != other.address)
            return address < other.address;
        if (fault_model_iteration != other.fault_model_iteration)
            return fault_model_iteration < other.fault_model_iteration;
        return false;
    }

    bool InstructionFault::operator>(const InstructionFault& other) const
    {
        return other < (*this);
    }

    bool InstructionFault::operator==(const InstructionFault& other) const
    {
        return model == other.model && address == other.address && time == other.time && fault_model_iteration == other.fault_model_iteration;
    }

    bool InstructionFault::operator!=(const InstructionFault& other) const
    {
        return !(*this == other);
    }

}    // namespace armory
