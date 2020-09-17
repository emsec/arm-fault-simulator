#pragma once

#include "armory/fault_combination.h"
#include "armory/context.h"
#include "m-ulator/disassembler.h"
#include "m-ulator/emulator.h"

#include <functional>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace armory
{
    using namespace mulator;

    class FaultTracer
    {
    public:

        /*
         * Creates a new fault tracer.
         * The context should be the same as was used in the fault simulator.
         */
        FaultTracer(const Context& ctx);
        ~FaultTracer() = default;


        /*
         * Start the fault tracing and print the trace to stdout.
         * The given emulator is taken as the base state, i.e., you can initialize an emulator, add your own hooks, and emulate arbitrary instructions before starting fault injection.
         *
         * @param[in] emulator - the base emulator
         * @param[in] faults - the modelific fault combination to inject
         * @param[in] start_log_after_first_fault - if true, output is only printed after the first fault was injected
         * @param[in] log_cpu_state - if true, all current register values are printed after each executed instruction
         *
         * @returns true if the faults were exploitable.
         */
        bool trace(const Emulator& emulator, FaultCombination& faults, bool start_log_after_first_fault = true, bool log_cpu_state = false);

        /*
         * Verify a modelific fault combination.
         * Nothing is printed to stdout.
         * The given emulator is taken as the base state, i.e., you can initialize an emulator, add your own hooks, and emulate arbitrary instructions before starting fault injection.
         *
         * @param[in] emulator - the base emulator
         * @param[in] faults - the modelific fault combination to inject
         *
         * @returns true if the faults were exploitable.
         */
        bool verify(const Emulator& emulator, FaultCombination& faults);

    private:
        static void log_instructions(Emulator& emu, const Instruction& instruction, void* hook_context);
        static void log_cpu_state(Emulator& emu, const Instruction& instruction, void* hook_context);
        static void detect_end_of_execution(Emulator& emu, u32 address, u32 instr_size, void* hook_context);
        static void handle_instruction_faults(Emulator& emu, u32 address, u32 instr_size, void* hook_context);
        static void handle_register_faults(Emulator& emu, u32 address, u32 instr_size, void* hook_context);
        static void handle_permanent_register_fault_overwrite(Emulator& emu, Register reg, u32 value, void* hook_context);
        void fault_injected();

        Context m_ctx;
        std::unique_ptr<Disassembler> m_disassembler;
        bool m_end_reached;
        ExploitabilityModel::Decision m_decision;
        std::unique_ptr<ExploitabilityModel> m_exploitability_model;
        bool m_first_fault_reached;
        bool m_start_after_first_fault;
        bool m_log_cpu_state;
        bool m_faulted_this_instruction;
        bool m_verification_mode;
    };
} // namespace armory
