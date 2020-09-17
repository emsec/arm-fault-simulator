#pragma once

#include "armory/context.h"
#include "armory/fault_combination.h"
#include "armory/snapshot.h"
#include "m-ulator/emulator.h"

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace armory
{
    using namespace mulator;

    class FaultSimulator
    {
    public:
        /*
         * Creates a new fault simulator with the given context.
         * The context defines when a fault is exploitable
         */
        FaultSimulator(const Context& ctx);
        ~FaultSimulator() = default;

        /*
         * Enables printing of progress information.
         * Disabled by default.
         * Information is printed to stderr in order to be separable from other output.
         */
        void enable_progress_printing(bool enable);

        /*
         * Sets the number of threads used for fault simulation.
         * A value of 0 (default) uses the same number of threads as CPU cores are available.
         */
        void set_number_of_threads(u32 threads);

        /*
         * Start the fault simulation.
         * The given emulator is taken as the base state, i.e., you can initialize an emulator, add your own hooks, and emulate arbitrary instructions before starting fault injection.
         * 'fault_models' contains all FaultModels to be tested, together with their amount.
         * Every FaultModel must appear only once in the vector, use the amount to test multiple instances of the same model.
         *
         * The fault simulator will automatically test all permutations and combinations of the models.
         * A maximum of 'max_simulatenous_faults' faults is injected during a single test. A value of 0 indicates no upper limit.
         * This function automatically utilizes all available CPU cores.
         */
        std::vector<FaultCombination> simulate_faults(const Emulator& emulator, std::vector<std::pair<const FaultModel*, u32>> fault_models, u32 max_simulatenous_faults);

    private:
        struct ThreadContext
        {
            ThreadContext(const Emulator& main_emulator) : emu(main_emulator), decoder(emu.get_decoder())
            {
            }

            Emulator emu;
            InstructionDecoder decoder;
            bool end_reached;
            ExploitabilityModel::Decision decision;
            std::unique_ptr<ExploitabilityModel> exploitability_model;
            std::vector<std::unique_ptr<Snapshot>> snapshots;
            std::vector<FaultCombination> new_faults;
        };

        void gather_faultable_instructions(const Emulator& main_emulator);

        std::vector<std::vector<u32>> compute_model_combinations(const std::vector<std::pair<const FaultModel*, u32>>& fault_models, u32 max_simulatenous_faults);

        std::vector<FaultCombination> prepare_known_exploitable_faults(const std::vector<mulator::u32>& current_models, const std::map<std::vector<u32>, std::vector<FaultCombination>>& memorized_faults);

        static void detect_end_of_execution(Emulator& emu, u32 address, u32 instr_size, void* hook_context);

        void simulate(const Emulator& main_emulator);

        static void instruction_collector(Emulator& emu, const Instruction& instruction, void* user_data);
        std::vector<std::tuple<u32, u32>> get_instruction_order(ThreadContext& thread_ctx, u32 remaining_cycles);

        static void add_new_registers_vector(Emulator& emu, const Instruction& instruction, void* user_data);

        void simulate_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 order, u32 remaining_cycles, const FaultCombination& current_chain);

        void simulate_permanent_instruction_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 order, u32 remaining_cycles, const FaultCombination& current_chain);
        void simulate_instruction_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 order, u32 remaining_cycles, const FaultCombination& current_chain);

        static void handle_permanent_register_fault_overwrite(Emulator& emu, Register reg, u32 value, void* hook_context);

        void simulate_permanent_register_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 order, u32 remaining_cycles, const FaultCombination& current_chain);
        void simulate_register_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 order, u32 remaining_cycles, const FaultCombination& current_chain);

        void print_progress();

        bool is_fault_redundant(const FaultCombination& c);

        Context m_ctx;
        std::vector<const FaultModel*> m_fault_models;
        std::vector<std::pair<u32, u8>> m_all_instructions;

        bool m_print_progress;
        double m_progress;

        u32 m_num_threads;
        u32 m_active_thread_count;

        std::mutex m_progress_mutex;
        u32 m_thread_progress;

        std::unordered_map<std::size_t, std::vector<FaultCombination>> m_known_exploitable_faults;
        std::vector<size_t> m_known_exploitable_fault_hashes;
        std::vector<FaultCombination> m_new_exploitable_faults;
    };
}    // namespace armory
