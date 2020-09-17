#pragma once

#include "armory/context.h"

namespace fault_simulator
{
    struct CodeSection
    {
        std::string name;
        std::vector<u8> bytes;
        u32 offset;
    };

    struct Configuration
    {
        mulator::Architecture arch;
        armory::MemoryRange flash;
        armory::MemoryRange ram;
        u32 start_address;
        std::vector<CodeSection> binary;
        std::unordered_map<u32, std::string> halt_address_symbols;
        std::unordered_map<u32, std::string> symbols;

        armory::Context faulting_context;
        u32 num_threads;
    };
} // namespace fault_simulator
