#include "armory/register_fault_model.h"

namespace armory
{
    RegisterFaultModel::RegisterFaultModel(const std::string& name, const FaultType& type, const IterationCounter& counter, const Tester& tester, const Injector& injector) : FaultModel(name)
    {
        m_type     = type;
        m_counter  = counter;
        m_tester   = tester;
        m_injector = injector;
    }

    bool RegisterFaultModel::is_permanent() const
    {
        return m_type == FaultType::PERMANENT;
    }

    RegisterFaultModel::FaultType RegisterFaultModel::get_type() const
    {
        return m_type;
    }

    u32 RegisterFaultModel::get_number_of_iterations(const RegisterFault& fault) const
    {
        return m_counter(fault);
    }

    bool RegisterFaultModel::is_applicable(const RegisterFault& fault) const
    {
        return m_tester(fault);
    }

    void RegisterFaultModel::apply(RegisterFault& fault) const
    {
        m_injector(fault);
    }

    // ########################################################
    // ########################################################
    // ########################################################

    RegisterFault::RegisterFault(const RegisterFaultModel* _model, u32 _time, u32 _fault_model_iteration) : Fault(_time, _fault_model_iteration)
    {
        this->model = _model;
    }

    bool RegisterFault::operator<(const RegisterFault& other) const
    {
        if (time != other.time)
            return time < other.time;
        if (reg != other.reg)
            return reg < other.reg;
        if (fault_model_iteration != other.fault_model_iteration)
            return fault_model_iteration < other.fault_model_iteration;
        return false;
    }

    bool RegisterFault::operator>(const RegisterFault& other) const
    {
        return other < (*this);
    }

    bool RegisterFault::operator==(const RegisterFault& other) const
    {
        return model == other.model && time == other.time && reg == other.reg && fault_model_iteration == other.fault_model_iteration;
    }

    bool RegisterFault::operator!=(const RegisterFault& other) const
    {
        return !(*this == other);
    }

}    // namespace armory
