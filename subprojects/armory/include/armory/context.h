#pragma once

#include "m-ulator/emulator.h"
#include "armory/types.h"

#include <string>
#include <vector>
#include <memory>

namespace armory
{
    struct Context;

    using namespace mulator;

    struct ExploitabilityModel
    {
        ExploitabilityModel() = default;
        virtual ~ExploitabilityModel() = default;

        enum class Decision
        {
            EXPLOITABLE,
            NOT_EXPLOITABLE,
            CONTINUE_SIMULATION,
        };

        virtual std::unique_ptr<ExploitabilityModel> clone() = 0;

        virtual Decision evaluate(const Emulator& emu, const Context& ctx, u32 reached_end_address) = 0;
    };

    struct TimeRange
    {
        u32 start;
        u32 end;
    };

    struct MemoryRange
    {
        u32 offset;
        u32 size;
    };

    struct Context
    {
        /*
         * Defines addresses on which fault simulation is stopped.
         * Use addresses where you want to check/know whether a fault was exploitable.
         */
        std::vector<u32> halting_points;

        /*
         * The model is checked whenever an "end address" is hit.
         * If it returns FAULT_EXPLOITABLE, the fault is regarded as exploitable and the next iteration is tested.
         * If it returns NOT_EXPLOITABLE, the fault is regarded as not exploitable and the next iteration is tested.
         * If it returns CONTINUE_SIMULATION, the fault is regarded as not exploitable and the ongoing simulation is continued for the same fault.
         * If the function object is left empty, every fault for which an end address is reached is regarded as FAULT_EXPLOITABLE.
         */
        ExploitabilityModel* exploitability_model = nullptr;

        /*
         * Give a timeout in number of executed instructions for binary emulation.
         * The timeout is applied for every combination of faults individually.
         */
        u32 emulation_timeout = 0;

        /*
         * If you want to not inject faults in a time tange, define it here.
         * This does not affect permanent faults!
         */
        std::vector<TimeRange> ignore_time_ranges;

        /*
         * If you want to not inject faults in an address range, define it here
         */
        std::vector<MemoryRange> ignore_memory_ranges;
    };
} // namespace armory
