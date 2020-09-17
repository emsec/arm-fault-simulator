#pragma once

#include "fault_simulator/configuration.h"

#include "armory/instruction_fault_model.h"
#include "armory/register_fault_model.h"
#include "armory/fault_combination.h"

namespace fault_simulator
{
    armory::InstructionFault build_fault(const armory::InstructionFaultModel* model, u32 fault_model_iteration, u32 time, u32 address, u32 instr_size);
    armory::RegisterFault build_fault(const armory::RegisterFaultModel* model, u32 fault_model_iteration, u32 time, armory::Register reg);

    std::vector<armory::FaultCombination> find_exploitable_faults(const mulator::Emulator& emu, const Configuration& config, const std::vector<std::pair<const armory::FaultModel*, u32>>& fault_injectors, u32 max_simulatenous_faults = 0, bool verify_faults = false);

    Configuration parse_arguments(int argc, char** argv);
} // namespace fault_simulator
