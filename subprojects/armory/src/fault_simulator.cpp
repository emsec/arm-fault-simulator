#include "armory/fault_simulator.h"

#include "armory/subset_chooser.h"
#include "m-ulator/emulator.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

// #########################################

using namespace armory;

template<typename T>
static void remove_duplicates(T& collection)
{
    std::sort(collection.begin(), collection.end());
    collection.erase(std::unique(collection.begin(), collection.end()), collection.end());
}

static u32 num_digits(u32 x)
{
    return (x < 10 ? 1 : (x < 100 ? 2 : (x < 1000 ? 3 : (x < 10000 ? 4 : (x < 100000 ? 5 : (x < 1000000 ? 6 : (x < 10000000 ? 7 : (x < 100000000 ? 8 : (x < 1000000000 ? 9 : 10)))))))));
}

FaultSimulator::FaultSimulator(const Context& ctx)
{
    m_ctx            = ctx;
    m_print_progress = false;
    m_num_threads    = 0;
    if (ctx.emulation_timeout == 0)
    {
        throw std::runtime_error("no timeout specified");
    }
    if (ctx.halting_points.empty())
    {
        throw std::runtime_error("no halting points specified");
    }
}

void FaultSimulator::enable_progress_printing(bool enable)
{
    m_print_progress = enable;
}

void FaultSimulator::set_number_of_threads(u32 threads)
{
    m_num_threads = threads;
}

std::vector<FaultCombination> FaultSimulator::simulate_faults(const Emulator& main_emulator, std::vector<std::pair<const FaultModel*, u32>> fault_models, u32 max_simulatenous_faults)
{
    // sort fault models: pull instruction fault models to the front
    std::sort(fault_models.begin(), fault_models.end(), [fault_models](const std::pair<const FaultModel*, u32>& a, const std::pair<const FaultModel*, u32>& b) -> bool {
        auto p_a = dynamic_cast<const InstructionFaultModel*>(a.first);
        auto p_b = dynamic_cast<const InstructionFaultModel*>(b.first);
        if (p_a && !p_b)
        {
            return true;
        }
        return std::find(fault_models.begin(), fault_models.end(), a) < std::find(fault_models.begin(), fault_models.end(), b);
    });

    // sort end addresses for binary search
    std::sort(m_ctx.halting_points.begin(), m_ctx.halting_points.end());

    // precompute all instructions that may be faulted
    gather_faultable_instructions(main_emulator);

    // get all suitable permutations of the indices of all fault models
    auto models_to_inject = compute_model_combinations(fault_models, max_simulatenous_faults);

    // prepare threading
    u32 num_threads = m_num_threads;
    if (num_threads == 0)
    {
        num_threads = std::thread::hardware_concurrency();
    }
    std::cerr << "Using " << std::dec << num_threads << " threads" << std::endl;

    // now, process all fault models

    std::map<std::vector<u32>, std::vector<FaultCombination>> memorized_faults;

    for (u32 model_index = 0; model_index < models_to_inject.size(); ++model_index)
    {
        const auto& current_models = models_to_inject[model_index];

        // get fault models from indices
        m_fault_models.clear();
        for (u32 i : current_models)
        {
            m_fault_models.push_back(fault_models[i].first);
        }

        std::vector<FaultCombination> known_fault_combinations = prepare_known_exploitable_faults(current_models, memorized_faults);

        if (m_print_progress)
        {
            std::cerr << "\rInjecting " << std::setfill(' ') << std::setw(num_digits(models_to_inject.size())) << model_index + 1 << "/" << models_to_inject.size() << ": ";
            for (u32 i = 0; i < m_fault_models.size(); ++i)
            {
                std::cerr << "'" << m_fault_models[i]->get_name() << "'";
                if (i < m_fault_models.size() - 1)
                    std::cerr << ", ";
            }
            std::cerr << std::endl;
        }

        m_thread_progress     = 0;
        m_progress            = 0;
        m_active_thread_count = 0;

        std::vector<std::thread> workers;
        for (u32 i = 0; i < num_threads - 1; ++i)
        {
            workers.emplace_back(&FaultSimulator::simulate, this, main_emulator);
        }
        simulate(main_emulator);

        // wait for threads to finish
        for (auto& w : workers)
        {
            w.join();
        }

        // store the results
        if (m_print_progress)
        {
            std::cerr << "\rremoving duplicates...                   \r" << std::flush;
        }

        remove_duplicates(m_new_exploitable_faults);
        known_fault_combinations.insert(known_fault_combinations.end(), m_new_exploitable_faults.begin(), m_new_exploitable_faults.end());
        memorized_faults[current_models] = known_fault_combinations;
        m_new_exploitable_faults.clear();
    }

    // all models have been simulated at this point

    if (m_print_progress)
    {
        std::cerr << "\raggregating final results...                   \r" << std::flush;
    }

    // this is an inefficient but working way to gather all exploitable faults:
    // 1. gather all exploitable fault combinations of all permutations (contains a lot of overlap)
    // 2. filter out duplicates
    std::vector<FaultCombination> result;
    for (const auto& it : memorized_faults)
    {
        result.reserve(result.size() + it.second.size());
        result.insert(result.end(), it.second.begin(), it.second.end());
    }
    remove_duplicates(result);

    if (m_print_progress)
    {
        std::cerr << "\r                                            \r" << std::flush;
    }

    return result;
}

///////////////////////////////////////////
/////////////  PRIVATE  ///////////////////
///////////////////////////////////////////

void FaultSimulator::gather_faultable_instructions(const Emulator& main_emulator)
{
    m_all_instructions.clear();

    auto flash = main_emulator.get_flash();
    u32 i      = 0;
    while (true)
    {
        auto size = InstructionDecoder::get_instruction_size(flash + i);
        auto addr = main_emulator.get_flash_offset() + i;

        if (i + size >= main_emulator.get_flash_size())
        {
            break;
        }

        u32 value = 0xFFFF0000 | ((u16*)(flash + i))[0];
        if (size == 4)
        {
            value = (value << 16) | ((u16*)(flash + i))[1];
        }
        i += size;

        // check if the current address is in an ignore range
        for (const auto& range : m_ctx.ignore_memory_ranges)
        {
            if (addr >= range.offset && addr < range.offset + range.size)
            {
                value = 0xFFFFFFFF;
                break;
            }
        }

        if (value != 0xFFFFFFFF)
        {
            m_all_instructions.push_back(std::make_pair(addr, size));
        }
    }
}

std::vector<std::vector<u32>> FaultSimulator::compute_model_combinations(const std::vector<std::pair<const FaultModel*, u32>>& fault_models, u32 max_simulatenous_faults)
{
    // split fault models into permanent and non-permanent
    // add their indices multiple times by how often they should be injected
    std::vector<u32> non_permanent_fault_ids;
    std::vector<u32> permanent_fault_ids;
    for (u32 i = 0; i < fault_models.size(); ++i)
    {
        if (fault_models[i].first->is_permanent())
        {
            for (u32 j = 0; j < fault_models[i].second; ++j)
            {
                permanent_fault_ids.push_back(i);
            }
        }
        else
        {
            for (u32 j = 0; j < fault_models[i].second; ++j)
            {
                non_permanent_fault_ids.push_back(i);
            }
        }
    }

    // for permanent fault models the order does not matter
    // --> simply gather all subsets
    std::set<std::vector<u32>> permanent_fault_id_subsets;
    if (!permanent_fault_ids.empty())
    {
        u32 upper_bound = permanent_fault_ids.size();
        if (max_simulatenous_faults != 0 && max_simulatenous_faults < upper_bound)
        {
            upper_bound = max_simulatenous_faults;
        }

        for (u32 i = 1; i <= upper_bound; ++i)
        {
            auto chooser = make_chooser(permanent_fault_ids, i, [](const auto&) { return true; });

            while (chooser.advance())
            {
                std::vector<u32> tmp;
                for (auto it : chooser.subset())
                {
                    tmp.push_back(*it);
                }
                permanent_fault_id_subsets.insert(tmp);
            }
        }
    }

    std::set<std::vector<u32>> all_permutations(permanent_fault_id_subsets);

    // for non-permanent fault models the order DOES matter
    // --> add all permutations of all subsets, prepend all subsets of permanent models each time
    if (!non_permanent_fault_ids.empty())
    {
        u32 upper_bound = non_permanent_fault_ids.size();
        if (max_simulatenous_faults != 0 && max_simulatenous_faults < upper_bound)
        {
            upper_bound = max_simulatenous_faults;
        }

        for (u32 i = 1; i <= upper_bound; ++i)
        {
            auto chooser = make_chooser(non_permanent_fault_ids, i, [](const auto&) { return true; });

            while (chooser.advance())
            {
                std::vector<u32> subset;
                for (auto it : chooser.subset())
                {
                    subset.push_back(*it);
                }

                // insert not only all permutations of the subset of non-permanent faults,
                // but also prepend all subsets of permanent faults
                do
                {
                    all_permutations.insert(subset);
                    for (const auto& permanent_fault_subset : permanent_fault_id_subsets)
                    {
                        if (max_simulatenous_faults == 0 || permanent_fault_subset.size() + subset.size() <= max_simulatenous_faults)
                        {
                            std::vector<u32> combined_subset;
                            combined_subset.insert(combined_subset.end(), permanent_fault_subset.begin(), permanent_fault_subset.end());
                            combined_subset.insert(combined_subset.end(), subset.begin(), subset.end());
                            all_permutations.insert(combined_subset);
                        }
                    }
                } while (std::next_permutation(subset.begin(), subset.end()));
            }
        }
    }

    // assemble final output
    // sort all the fault model permutations by their total size
    std::vector<std::vector<u32>> ordered_permutations(all_permutations.begin(), all_permutations.end());
    std::sort(ordered_permutations.begin(), ordered_permutations.end(), [](const std::vector<u32>& a, const std::vector<u32>& b) {
        if (a.size() != b.size())
        {
            return a.size() < b.size();
        }
        for (u32 i = 0; i < a.size(); ++i)
        {
            if (a[i] < b[i])
            {
                return true;
            }
            else if (a[i] > b[i])
            {
                return false;
            }
        }
        return false;
    });

    return ordered_permutations;
}

void FaultSimulator::detect_end_of_execution(Emulator& emu, u32 address, u32 instr_size, void* user_data)
{
    UNUSED(instr_size);
    auto [inst_ptr, thread_ctx] = *((std::tuple<FaultSimulator*, ThreadContext*>*)user_data);

    if (std::binary_search(inst_ptr->m_ctx.halting_points.begin(), inst_ptr->m_ctx.halting_points.end(), address))
    {
        if (thread_ctx->exploitability_model == nullptr)
        {
            thread_ctx->decision = ExploitabilityModel::Decision::EXPLOITABLE;
        }
        else
        {
            thread_ctx->decision = thread_ctx->exploitability_model->evaluate(emu, inst_ptr->m_ctx, address);
        }
        if (thread_ctx->decision != ExploitabilityModel::Decision::CONTINUE_SIMULATION)
        {
            thread_ctx->end_reached = true;
            emu.stop_emulation();
        }
    }
}

void FaultSimulator::print_progress()
{
    std::cerr << "\r" << m_progress << "% (" << m_active_thread_count << " active threads)   " << std::flush;
}

void FaultSimulator::simulate(const Emulator& main_emulator)
{
    using namespace std::placeholders;

    if (m_print_progress)
    {
        std::lock_guard<std::mutex> lock(m_progress_mutex);
        m_active_thread_count++;
        print_progress();
    }

    ThreadContext thread_ctx(main_emulator);
    thread_ctx.exploitability_model = nullptr;

    std::tuple<FaultSimulator*, ThreadContext*> hook_user_data(this, &thread_ctx);

    thread_ctx.emu.before_fetch_hook.add(&FaultSimulator::detect_end_of_execution, &hook_user_data);

    for (u32 i = 0; i < m_fault_models.size(); ++i)
    {
        thread_ctx.snapshots.push_back(std::make_unique<Snapshot>(thread_ctx.emu));
    }

    simulate_fault(thread_ctx, 0, 0, m_ctx.emulation_timeout, {});

    if (m_print_progress)
    {
        std::lock_guard<std::mutex> lock(m_progress_mutex);
        m_active_thread_count--;
        print_progress();
    }

    {
        std::lock_guard<std::mutex> lock(m_progress_mutex);
        m_new_exploitable_faults.insert(m_new_exploitable_faults.end(), thread_ctx.new_faults.begin(), thread_ctx.new_faults.end());
    }
}

void FaultSimulator::simulate_fault(ThreadContext& thread_ctx, u32 recursion_data, u32 order, u32 remaining_cycles, const FaultCombination& chain)
{
    if (m_fault_models[order]->is_permanent())
    {
        if (dynamic_cast<const InstructionFaultModel*>(m_fault_models[order]))
        {
            simulate_permanent_instruction_fault(thread_ctx, recursion_data, order, remaining_cycles, chain);
        }
        else if (dynamic_cast<const RegisterFaultModel*>(m_fault_models[order]))
        {
            simulate_permanent_register_fault(thread_ctx, recursion_data, order, remaining_cycles, chain);
        }
    }
    else
    {
        if (dynamic_cast<const InstructionFaultModel*>(m_fault_models[order]))
        {
            simulate_instruction_fault(thread_ctx, recursion_data, order, remaining_cycles, chain);
        }
        else if (dynamic_cast<const RegisterFaultModel*>(m_fault_models[order]))
        {
            simulate_register_fault(thread_ctx, recursion_data, order, remaining_cycles, chain);
        }
    }
}

std::vector<FaultCombination> FaultSimulator::prepare_known_exploitable_faults(const std::vector<mulator::u32>& current_models,
                                                                               const std::map<std::vector<u32>, std::vector<FaultCombination>>& memorized_faults)
{
    std::vector<FaultCombination> known_fault_combinations;

    m_known_exploitable_fault_hashes.clear();
    m_known_exploitable_faults.clear();

    // pull in already found faults
    if (current_models.size() > 1)
    {
        // since this is a higher-order injection, prepare the known exploitable faults of lower order to compare against
        std::vector<u32> preceding_models(current_models);
        preceding_models.pop_back();

        // add faults that were already found for the newly added model in isolation
        std::vector<u32> current_iteration = {current_models.back()};
        if (auto it = memorized_faults.find(current_iteration); it != memorized_faults.end())
        {
            known_fault_combinations.insert(known_fault_combinations.end(), it->second.begin(), it->second.end());
        }

        // add faults that were already found for the preceding model
        if (auto it = memorized_faults.find(preceding_models); it != memorized_faults.end())
        {
            known_fault_combinations.insert(known_fault_combinations.end(), it->second.begin(), it->second.end());
        }

        for (const auto& fc : known_fault_combinations)
        {
            auto& faults = fc.get_sorted_faults();
            for (u32 i = 1; i <= faults.size(); ++i)
            {
                auto chooser = make_chooser(faults, i, [](const auto&) { return true; });

                while (chooser.advance())
                {
                    size_t hash = 0;
                    for (auto it : chooser.subset())
                    {
                        if (auto ptr = dynamic_cast<const InstructionFault*>(*it); ptr != nullptr)
                        {
                            hash_combine(hash, *ptr);
                        }
                        else if (auto ptr = dynamic_cast<const RegisterFault*>(*it); ptr != nullptr)
                        {
                            hash_combine(hash, *ptr);
                        }
                    }
                    m_known_exploitable_fault_hashes.push_back(hash);
                    m_known_exploitable_faults[hash].push_back(fc);
                }
            }
        }
        remove_duplicates(m_known_exploitable_fault_hashes);
    }

    return known_fault_combinations;
}

bool FaultSimulator::is_fault_redundant(const FaultCombination& c)
{
    if (m_known_exploitable_fault_hashes.empty())
    {
        return false;
    }
    auto& faults = c.get_sorted_faults();
    for (u32 i = 1; i <= faults.size(); ++i)
    {
        auto chooser = make_chooser(faults, i, [](const auto&) { return true; });

        while (chooser.advance())
        {
            size_t hash = 0;
            for (auto it : chooser.subset())
            {
                if (auto ptr = dynamic_cast<const InstructionFault*>(*it); ptr != nullptr)
                {
                    hash_combine(hash, *ptr);
                }
                else if (auto ptr = dynamic_cast<const RegisterFault*>(*it); ptr != nullptr)
                {
                    hash_combine(hash, *ptr);
                }
            }
            if (std::binary_search(m_known_exploitable_fault_hashes.begin(), m_known_exploitable_fault_hashes.end(), hash))
            {
                for (const auto& f : m_known_exploitable_faults[hash])
                {
                    if (f.size() > c.size())
                    {
                        break;
                    }
                    if (c.includes(f))
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
