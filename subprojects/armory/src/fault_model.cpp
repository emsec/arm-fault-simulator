#include "armory/fault_model.h"

namespace armory
{

    FaultModel::FaultModel(const std::string& name)
    {
        m_name = name;
    }

    std::string FaultModel::get_name() const
    {
        return m_name;
    }

    Fault::Fault(u32 _time, u32 _fault_model_iteration)
    {
        this->time = _time;
        this->fault_model_iteration = _fault_model_iteration;
    }

    Fault::~Fault() {}

} // namespace armory
