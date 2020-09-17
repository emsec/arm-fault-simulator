#include "armory/fault_tracer.h"
#include "fault_simulator/fault_models.h"
#include "fault_simulator/fault_simulator.h"
#include "termcolor.h"

#include <chrono>
#include <cstring>
#include <iostream>

using namespace armory;

int main(int argc, char** argv)
{
    // parse command line arguments, generated by start_emulation.py
    fault_simulator::Configuration config;
    try
    {
        config = fault_simulator::parse_arguments(argc, argv);
    }
    catch (std::exception& ex)
    {
        std::cout << ex.what() << std::endl;
        return 1;
    }

    // setup the emulator instance
    Emulator main_emulator(config.arch);
    main_emulator.set_flash_region(config.flash.offset, config.flash.size);
    main_emulator.set_ram_region(config.ram.offset, config.ram.size);
    for (const auto& s : config.binary)
    {
        main_emulator.write_memory(s.offset, s.bytes.data(), s.bytes.size());
    }
    main_emulator.write_register(Register::SP, config.ram.offset + config.ram.size);
    main_emulator.write_register(Register::PC, config.start_address);
    main_emulator.write_register(Register::LR, 0xFFFFFFFF);

    // example function `interesting_to_fault` gets a 32-byte array as first parameter, so we fill it with random data
    for (u32 i = 0; i < 8; ++i)
    {
        u32 random = rand();
        main_emulator.write_memory(config.ram.offset + 4 * i, (u8*)&random, 4);
    }
    main_emulator.write_register(Register::R0, config.ram.offset);

    // set a maximum number of instructions to execute before a fault is automatically regarded as not exploitable
    config.faulting_context.emulation_timeout = 500;

    // we could select a different exploitability model...
    // setting this to nullptr is equivalent to regarding every fault that makes execution reach a halting point as exploitable
    config.faulting_context.exploitability_model = nullptr;


    // now let's inject some faults

    u32 total_faults = 0;
    auto start       = std::chrono::steady_clock::now();

    for (const auto& spec : fault_models::all_fault_models)
    {
        // inject the model 1 time
        auto model_injection = std::make_pair(spec, 1);

        // inject the model 2 times
        // auto model_injection = std::make_pair(spec, 2);

        auto exploitable_faults = fault_simulator::find_exploitable_faults(main_emulator, config, {model_injection});
        total_faults += exploitable_faults.size();
    }

    auto end       = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;

    std::cout << "_______________________________________" << std::endl;
    std::cout << "All tested models combined: " << std::dec << total_faults << " exploitable faults" << std::endl;
    std::cout << "Total time: " << seconds << " seconds" << std::endl << std::endl;

    return 0;
}