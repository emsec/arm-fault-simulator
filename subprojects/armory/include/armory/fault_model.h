#pragma once

#include "armory/context.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace armory
{
    /*
     * Base class.
     * See InstructionFaultModel or RegisterFaultModel for implementations.
     *
     * In general a FaultModel specifies the effect and duration of a fault.
     */
    class FaultModel
    {
    public:
        FaultModel(const std::string& name);
        virtual ~FaultModel() = default;

        std::string get_name() const;

        virtual bool is_permanent() const = 0;

    private:
        std::string m_name;
    };

    /*
     * Base class.
     * See InstructionFault or RegisterFault for implementations.
     *
     * In general a Fault holds the effect of the application of a specific iteration of a FaultModel.
     */
    struct Fault
    {
        Fault(u32 time, u32 fault_model_iteration);
        virtual ~Fault() = 0;
        u32 time;
        u32 fault_model_iteration;
    };

    template<class T>
    inline void hash_combine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

}    // namespace armory
