#include "m-ulator/emulator.h"

#include "m-ulator/arm_functions.h"

#include <cstring>
#include <stdexcept>

using namespace mulator;

Emulator::Emulator(Architecture arch) : m_decoder(arch)
{
    m_next_hook_id = 1;
    m_cleanup_requested = false;
    m_emulated_time = 0;
    m_cpu_state.time = 0;
    m_return_code = ReturnCode::UNINITIALIZED;

    m_ram.bytes = nullptr;
    m_flash.bytes = nullptr;
    m_has_on_fetch_hooks = false;
    m_has_on_decode_hooks = false;
    m_has_on_execute_hooks = false;
    m_has_before_memory_read_hooks = false;
    m_has_after_memory_read_hooks = false;
    m_has_before_memory_write_hooks = false;
    m_has_after_memory_write_hooks = false;
    m_has_before_register_read_hooks = false;
    m_has_after_register_read_hooks = false;
    m_has_before_register_write_hooks = false;
    m_has_after_register_write_hooks = false;

    std::memset(&m_cpu_state, 0, sizeof(m_cpu_state));
    m_cpu_state.registers[Register::LR] = 0xFFFFFFFF;
}

Emulator::Emulator(const Emulator& other) : m_decoder(other.get_architecture())
{
    m_on_fetch_hooks = other.m_on_fetch_hooks;
    m_on_decode_hooks = other.m_on_decode_hooks;
    m_on_execute_hooks = other.m_on_execute_hooks;
    m_before_memory_read_hooks = other.m_before_memory_read_hooks;
    m_after_memory_read_hooks = other.m_after_memory_read_hooks;
    m_before_memory_write_hooks = other.m_before_memory_write_hooks;
    m_after_memory_write_hooks = other.m_after_memory_write_hooks;
    m_before_register_read_hooks = other.m_before_register_read_hooks;
    m_after_register_read_hooks = other.m_after_register_read_hooks;
    m_before_register_write_hooks = other.m_before_register_write_hooks;
    m_after_register_write_hooks = other.m_after_register_write_hooks;
    m_next_hook_id = other.m_next_hook_id;
    m_remove_hooks = other.m_remove_hooks;
    m_hook_id_type = other.m_hook_id_type;

    m_return_code = other.m_return_code;

    m_flash = other.m_flash;
    m_flash.bytes = new u8[m_flash.size];
    std::memcpy(m_flash.bytes, other.m_flash.bytes, m_flash.size);

    m_ram = other.m_ram;
    m_ram.bytes = new u8[m_ram.size];
    std::memcpy(m_ram.bytes, other.m_ram.bytes, m_ram.size);

    m_cpu_state = other.m_cpu_state;
    m_emulated_time = other.m_emulated_time;
}

Emulator::~Emulator()
{
    delete[] m_ram.bytes;
    delete[] m_flash.bytes;
}

InstructionDecoder Emulator::get_decoder() const
{
    return m_decoder;
}

Architecture Emulator::get_architecture() const
{
    return m_decoder.get_architecture();
}

void Emulator::set_flash_region(u32 offset, u32 size)
{
    if (m_flash.bytes != nullptr)
    {
        delete[] m_flash.bytes;
    }

    m_flash.offset = offset;
    m_flash.size = size;
    m_flash.bytes = new u8[size];
    m_flash.access.read = true;
    m_flash.access.write = false;
    m_flash.access.execute = true;
    std::memset(m_flash.bytes, 0xFF, size);
}
u32 Emulator::get_flash_offset() const
{
    return m_flash.offset;
}
u32 Emulator::get_flash_size() const
{
    return m_flash.size;
}
u8* Emulator::get_flash() const
{
    return m_flash.bytes;
}

void Emulator::set_ram_region(u32 offset, u32 size)
{
    if (m_ram.bytes != nullptr)
    {
        delete[] m_ram.bytes;
    }

    m_ram.offset = offset;
    m_ram.size = size;
    m_ram.bytes = new u8[size];
    m_ram.access.read = true;
    m_ram.access.write = true;
    m_ram.access.execute = false;

    std::memset(m_ram.bytes, 0x00, size);
}
u32 Emulator::get_ram_offset() const
{
    return m_ram.offset;
}
u32 Emulator::get_ram_size() const
{
    return m_ram.size;
}
u8* Emulator::get_ram() const
{
    return m_ram.bytes;
}

u32 Emulator::add_before_fetch_hook(hook_on_fetch_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_on_fetch_hooks.emplace_back(id, func, hook_on_fetch_func(), user_data);
    m_hook_id_type[id] = 0;
    m_has_on_fetch_hooks = true;
    return id;
}

u32 Emulator::add_instruction_decoded_hook(hook_instruction_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_on_decode_hooks.emplace_back(id, func, hook_instruction_func(), user_data);
    m_hook_id_type[id] = 1;
    m_has_on_decode_hooks = true;
    return id;
}

u32 Emulator::add_instruction_executed_hook(hook_instruction_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_on_execute_hooks.emplace_back(id, func, hook_instruction_func(), user_data);
    m_hook_id_type[id] = 2;
    m_has_on_execute_hooks = true;
    return id;
}

u32 Emulator::add_memory_before_read_hook(hook_on_memory_access_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_before_memory_read_hooks.emplace_back(id, func, hook_on_memory_access_func(), user_data);
    m_hook_id_type[id] = 3;
    m_has_before_memory_read_hooks = true;
    return id;
}

u32 Emulator::add_memory_after_read_hook(hook_on_memory_access_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_after_memory_read_hooks.emplace_back(id, func, hook_on_memory_access_func(), user_data);
    m_hook_id_type[id] = 4;
    m_has_after_memory_read_hooks = true;
    return id;
}

u32 Emulator::add_memory_before_write_hook(hook_on_memory_access_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_before_memory_write_hooks.emplace_back(id, func, hook_on_memory_access_func(), user_data);
    m_hook_id_type[id] = 5;
    m_has_before_memory_write_hooks = true;
    return id;
}

u32 Emulator::add_memory_after_write_hook(hook_on_memory_access_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_after_memory_write_hooks.emplace_back(id, func, hook_on_memory_access_func(), user_data);
    m_hook_id_type[id] = 6;
    m_has_after_memory_write_hooks = true;
    return id;
}
u32 Emulator::add_register_before_read_hook(hook_on_register_access_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_before_register_read_hooks.emplace_back(id, func, hook_on_register_access_func(), user_data);
    m_hook_id_type[id] = 7;
    m_has_before_register_read_hooks = true;
    return id;
}

u32 Emulator::add_register_after_read_hook(hook_on_register_access_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_after_register_read_hooks.emplace_back(id, func, hook_on_register_access_func(), user_data);
    m_hook_id_type[id] = 8;
    m_has_after_register_read_hooks = true;
    return id;
}

u32 Emulator::add_register_before_write_hook(hook_on_register_access_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_before_register_write_hooks.emplace_back(id, func, hook_on_register_access_func(), user_data);
    m_hook_id_type[id] = 9;
    m_has_before_register_write_hooks = true;
    return id;
}

u32 Emulator::add_register_after_write_hook(hook_on_register_access_ptr func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_after_register_write_hooks.emplace_back(id, func, hook_on_register_access_func(), user_data);
    m_hook_id_type[id] = 10;
    m_has_after_register_write_hooks = true;
    return id;
}

u32 Emulator::add_before_fetch_hook(const hook_on_fetch_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_on_fetch_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 0;
    m_has_on_fetch_hooks = true;
    return id;
}

u32 Emulator::add_instruction_decoded_hook(const hook_instruction_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_on_decode_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 1;
    m_has_on_decode_hooks = true;
    return id;
}

u32 Emulator::add_instruction_executed_hook(const hook_instruction_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_on_execute_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 2;
    m_has_on_execute_hooks = true;
    return id;
}

u32 Emulator::add_memory_before_read_hook(const hook_on_memory_access_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_before_memory_read_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 3;
    m_has_before_memory_read_hooks = true;
    return id;
}

u32 Emulator::add_memory_after_read_hook(const hook_on_memory_access_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_after_memory_read_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 4;
    m_has_after_memory_read_hooks = true;
    return id;
}

u32 Emulator::add_memory_before_write_hook(const hook_on_memory_access_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_before_memory_write_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 5;
    m_has_before_memory_write_hooks = true;
    return id;
}

u32 Emulator::add_memory_after_write_hook(const hook_on_memory_access_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_after_memory_write_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 6;
    m_has_after_memory_write_hooks = true;
    return id;
}
u32 Emulator::add_register_before_read_hook(const hook_on_register_access_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_before_register_read_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 7;
    m_has_before_register_read_hooks = true;
    return id;
}

u32 Emulator::add_register_after_read_hook(const hook_on_register_access_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_after_register_read_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 8;
    m_has_after_register_read_hooks = true;
    return id;
}

u32 Emulator::add_register_before_write_hook(const hook_on_register_access_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_before_register_write_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 9;
    m_has_before_register_write_hooks = true;
    return id;
}

u32 Emulator::add_register_after_write_hook(const hook_on_register_access_func& func, void* user_data)
{
    auto id = m_next_hook_id++;
    m_after_register_write_hooks.emplace_back(id, nullptr, func, user_data);
    m_hook_id_type[id] = 10;
    m_has_after_register_write_hooks = true;
    return id;
}

void Emulator::remove_hook(u32 id)
{
    m_remove_hooks.push_back(id);
    m_cleanup_requested = true;
}

void Emulator::cleanup_hooks()
{
    if (m_cleanup_requested)
    {
        for (u32 id : m_remove_hooks)
        {
            cleanup_hook(id);
        }
        m_remove_hooks.clear();
        m_cleanup_requested = false;
    }
}

void Emulator::cleanup_hook(u32 id)
{
    auto map_it = m_hook_id_type.find(id);
    if (map_it != m_hook_id_type.end())
    {
        auto typ = map_it->second;
        m_hook_id_type.erase(map_it);
        if (typ == 0)
        {
            for (auto it = m_on_fetch_hooks.begin(); it != m_on_fetch_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_on_fetch_hooks.erase(it);
                    m_has_on_fetch_hooks = !m_on_fetch_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 1)
        {
            for (auto it = m_on_decode_hooks.begin(); it != m_on_decode_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_on_decode_hooks.erase(it);
                    m_has_on_decode_hooks = !m_on_decode_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 2)
        {
            for (auto it = m_on_execute_hooks.begin(); it != m_on_execute_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_on_execute_hooks.erase(it);
                    m_has_on_execute_hooks = !m_on_execute_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 3)
        {
            for (auto it = m_before_memory_read_hooks.begin(); it != m_before_memory_read_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_before_memory_read_hooks.erase(it);
                    m_has_before_memory_read_hooks = !m_before_memory_read_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 4)
        {
            for (auto it = m_after_memory_read_hooks.begin(); it != m_after_memory_read_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_after_memory_read_hooks.erase(it);
                    m_has_after_memory_read_hooks = !m_after_memory_read_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 5)
        {
            for (auto it = m_before_memory_write_hooks.begin(); it != m_before_memory_write_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_before_memory_write_hooks.erase(it);
                    m_has_before_memory_write_hooks = !m_before_memory_write_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 6)
        {
            for (auto it = m_after_memory_write_hooks.begin(); it != m_after_memory_write_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_after_memory_write_hooks.erase(it);
                    m_has_after_memory_write_hooks = !m_after_memory_write_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 7)
        {
            for (auto it = m_before_register_read_hooks.begin(); it != m_before_register_read_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_before_register_read_hooks.erase(it);
                    m_has_before_register_read_hooks = !m_before_register_read_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 8)
        {
            for (auto it = m_after_register_read_hooks.begin(); it != m_after_register_read_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_after_register_read_hooks.erase(it);
                    m_has_after_register_read_hooks = !m_after_register_read_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 9)
        {
            for (auto it = m_before_register_write_hooks.begin(); it != m_before_register_write_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_before_register_write_hooks.erase(it);
                    m_has_before_register_write_hooks = !m_before_register_write_hooks.empty();
                    return;
                }
            }
        }
        else if (typ == 10)
        {
            for (auto it = m_after_register_write_hooks.begin(); it != m_after_register_write_hooks.end(); ++it)
            {
                if (std::get<0>(*it) == id)
                {
                    m_after_register_write_hooks.erase(it);
                    m_has_after_register_write_hooks = !m_after_register_write_hooks.empty();
                    return;
                }
            }
        }
    }
}
void Emulator::clear_hooks()
{
    m_on_fetch_hooks.clear();
    m_on_decode_hooks.clear();
    m_on_execute_hooks.clear();
    m_before_memory_read_hooks.clear();
    m_after_memory_read_hooks.clear();
    m_before_memory_write_hooks.clear();
    m_after_memory_write_hooks.clear();
    m_before_register_read_hooks.clear();
    m_after_register_read_hooks.clear();
    m_before_register_write_hooks.clear();
    m_after_register_write_hooks.clear();
    m_has_on_fetch_hooks = false;
    m_has_on_decode_hooks = false;
    m_has_on_execute_hooks = false;
    m_has_before_memory_read_hooks = false;
    m_has_after_memory_read_hooks = false;
    m_has_before_memory_write_hooks = false;
    m_has_after_memory_write_hooks = false;
    m_has_before_register_read_hooks = false;
    m_has_after_register_read_hooks = false;
    m_has_before_register_write_hooks = false;
    m_has_after_register_write_hooks = false;
    m_next_hook_id = 1;
}

u32 Emulator::read_register(Register reg) const
{
    if ((u32)reg >= REGISTER_COUNT)
    {
        throw std::runtime_error("INVALID_REGISTER");
    }

    if (reg == PC)
    {
        return m_cpu_state.registers[Register::PC] + 4;
    }

    return m_cpu_state.registers[reg];
}

void Emulator::write_register(Register reg, u32 value)
{
    if ((u32)reg >= REGISTER_COUNT)
    {
        throw std::runtime_error("INVALID_REGISTER");
    }

    if (reg == SP)
    {
        value = arm_functions::align(value, 4);
    }
    else if (reg == PSR)
    {
        if (get_architecture() > Architecture::ARMv6M)
        {
            value &= 0xFE00FC00;
        }
        else
        {
            value &= 0xF0000000;
        }
        m_cpu_state.psr.N = _1BIT(value >> 31);
        m_cpu_state.psr.Z = _1BIT(value >> 30);
        m_cpu_state.psr.C = _1BIT(value >> 29);
        m_cpu_state.psr.V = _1BIT(value >> 28);
        m_cpu_state.psr.Q = _1BIT(value >> 27);

        m_cpu_state.psr.it_state = (_2BIT(value >> 25) << 6) | _6BIT(value >> 10);
    }

    m_cpu_state.registers[reg] = value;
}

void Emulator::read_memory(u32 address, u8* buffer, u32 len) const
{
    if (m_flash.contains(address, len))
    {
        std::memcpy(buffer, m_flash.get(address), len);
        return;
    }
    if (m_ram.contains(address, len))
    {
        std::memcpy(buffer, m_ram.get(address), len);
        return;
    }
    throw std::runtime_error("INVALID_MEMORY_ACCESS");
}

void Emulator::write_memory(u32 dst_address, const u8* buffer, u32 len)
{
    if (m_flash.contains(dst_address, len))
    {
        std::memcpy(m_flash.get(dst_address), buffer, len);
        return;
    }
    if (m_ram.contains(dst_address, len))
    {
        std::memcpy(m_ram.get(dst_address), buffer, len);
        return;
    }
    throw std::runtime_error("INVALID_MEMORY_ACCESS");
}

ReturnCode Emulator::emulate(u64 max_instructions)
{
    return emulate(1, max_instructions);
}

ReturnCode Emulator::emulate(u32 end_address, u64 max_instructions)
{
    if (m_ram.bytes == nullptr || m_flash.bytes == nullptr)
    {
        return ReturnCode::UNINITIALIZED;
    }

    cleanup_hooks();

    m_return_code = ReturnCode::OK;
    m_emulated_time = 0;

    branch_write_PC(m_cpu_state.registers[PC]);

    while (m_return_code == ReturnCode::OK)
    {
        if (m_emulated_time >= max_instructions)
        {
            m_return_code = ReturnCode::MAX_INSTRUCTIONS_REACHED;
            break;
        }
        if (m_cpu_state.registers[PC] == end_address)
        {
            m_return_code = ReturnCode::END_ADDRESS_REACHED;
            break;
        }
        clock_cpu();
    }

    cleanup_hooks();

    return m_return_code;
}

u32 Emulator::get_time() const
{
    return m_cpu_state.time;
}

u32 Emulator::get_emulated_time() const
{
    return m_emulated_time;
}

void Emulator::stop_emulation()
{
    m_return_code = ReturnCode::STOP_EMULATION_CALLED;
}

bool Emulator::is_running() const
{
    return m_return_code == ReturnCode::OK;
}

CPU_State Emulator::get_cpu_state() const
{
    return m_cpu_state;
}

void Emulator::set_cpu_state(const CPU_State& state)
{
    m_cpu_state = state;
}

///////////////////////////////////////////
/////////////  PRIVATE  ///////////////////
///////////////////////////////////////////

u8* Emulator::validate_address(u32 address)
{
    if (m_flash.access.execute && m_flash.contains(address, 2))
    {
        u32 size = m_decoder.get_instruction_size(m_flash.get(address));
        if (size == 2 || m_flash.contains(address, size))
        {
            return m_flash.get(address);
        }
        else
        {
            m_return_code = ReturnCode::INCOMPLETE_DATA;
            return nullptr;
        }
    }
    else if (m_ram.access.execute && m_ram.contains(address, 2))
    {
        u32 size = m_decoder.get_instruction_size(m_ram.get(address));
        if (size == 2 || m_ram.contains(address, size))
        {
            return m_ram.get(address);
        }
        else
        {
            m_return_code = ReturnCode::INCOMPLETE_DATA;
            return nullptr;
        }
    }

    m_return_code = ReturnCode::INVALID_MEMORY_ACCESS;
    return nullptr;
}

void Emulator::clock_cpu()
{
    u32 address = read_register_internal(Register::PC) - 4;
    auto memory = validate_address(address);

    if (m_return_code != ReturnCode::OK)
    {
        return;
    }

    auto instr_size = m_decoder.get_instruction_size(memory);

    if (m_has_on_fetch_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_on_fetch_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, address, instr_size, ctx);
            }
            else
            {
                func_std(*this, address, instr_size, ctx);
            }
        }
        cleanup_hooks();
    }

    if (m_return_code != ReturnCode::OK)
    {
        return;
    }

    if (m_cpu_state.registers[PC] != address) // hook changed PC
    {
        address = read_register_internal(Register::PC) - 4;
        memory = validate_address(address);
        if (m_return_code != ReturnCode::OK)
        {
            return;
        }
    }

    //dummy read to execute hooks
    read_register_internal(Register::PSR);

    auto [status, instr] = m_decoder.decode_instruction(address, memory, m_decoder.get_instruction_size(memory), in_IT_block(), last_in_IT_block());

    if (status != ReturnCode::OK)
    {
        m_return_code = status;
        return;
    }

    if (m_has_on_decode_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_on_decode_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, instr, ctx);
            }
            else
            {
                func_std(*this, instr, ctx);
            }
        }
        cleanup_hooks();
    }

    if (m_return_code != ReturnCode::OK)
    {
        return;
    }

    execute(instr);

    if (m_psr_updated)
    {
        u32 value = 0;
        value |= ((u32)m_cpu_state.psr.N) << 31;
        value |= ((u32)m_cpu_state.psr.Z) << 30;
        value |= ((u32)m_cpu_state.psr.C) << 29;
        value |= ((u32)m_cpu_state.psr.V) << 28;
        value |= ((u32)m_cpu_state.psr.Q) << 27;

        value |= (((u32)m_cpu_state.psr.it_state >> 6) << 25) | (_6BIT((u32)m_cpu_state.psr.it_state) << 10);

        write_register_internal(Register::PSR, value);
    }

    if (m_has_on_execute_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_on_execute_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, instr, ctx);
            }
            else
            {
                func_std(*this, instr, ctx);
            }
        }
        cleanup_hooks();
    }

    m_cpu_state.time++;
    m_emulated_time++;
}

u32 Emulator::read_register_internal(Register reg)
{
    if ((u32)reg >= REGISTER_COUNT)
    {
        m_return_code = ReturnCode::INVALID_REGISTER;
        return 0;
    }

    if (m_return_code != ReturnCode::OK)
    {
        return 0;
    }

    u32 value;

    if (m_has_before_register_read_hooks)
    {
        if (reg == PC)
        {
            value = m_cpu_state.registers[Register::PC] + 4;
        }
        else
        {
            value = m_cpu_state.registers[reg];
        }

        for (auto [id, func_ptr, func_std, ctx] : m_before_register_read_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, reg, value, ctx);
            }
            else
            {
                func_std(*this, reg, value, ctx);
            }
        }
        cleanup_hooks();

        if (m_return_code != ReturnCode::OK)
        {
            return 0;
        }
    }

    if (reg == PC)
    {
        value = m_cpu_state.registers[Register::PC] + 4;
    }
    else
    {
        value = m_cpu_state.registers[reg];
    }

    if (m_has_after_register_read_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_after_register_read_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, reg, value, ctx);
            }
            else
            {
                func_std(*this, reg, value, ctx);
            }
        }
        cleanup_hooks();
    }

    return value;
}

void Emulator::write_register_internal(Register reg, u32 value)
{
    if (m_return_code != ReturnCode::OK)
    {
        return;
    }

    if ((u32)reg >= REGISTER_COUNT)
    {
        m_return_code = ReturnCode::INVALID_REGISTER;
        return;
    }

    if (reg == SP)
    {
        value = arm_functions::align(value, 4);
    }
    else if (reg == PSR)
    {
        if (get_architecture() > Architecture::ARMv6M)
        {
            value &= 0xFE00FC00;
        }
        else
        {
            value &= 0xF0000000;
        }
    }

    if (m_has_before_register_write_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_before_register_write_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, reg, value, ctx);
            }
            else
            {
                func_std(*this, reg, value, ctx);
            }
        }
        cleanup_hooks();
    }

    if (m_return_code != ReturnCode::OK)
    {
        return;
    }

    if (reg == PSR)
    {
        m_cpu_state.psr.N = _1BIT(value >> 31);
        m_cpu_state.psr.Z = _1BIT(value >> 30);
        m_cpu_state.psr.C = _1BIT(value >> 29);
        m_cpu_state.psr.V = _1BIT(value >> 28);
        m_cpu_state.psr.Q = _1BIT(value >> 27);

        m_cpu_state.psr.it_state = (_2BIT(value >> 25) << 6) | _6BIT(value >> 10);
    }

    m_cpu_state.registers[reg] = value;

    if (m_has_after_register_write_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_after_register_write_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, reg, value, ctx);
            }
            else
            {
                func_std(*this, reg, value, ctx);
            }
        }
        cleanup_hooks();
    }
}

u32 Emulator::read_memory_internal(u32 address, u8 bytes)
{
    if (m_return_code != ReturnCode::OK)
    {
        return 0;
    }

    if (bytes != 4 && bytes != 2 && bytes != 1)
    {
        m_return_code = ReturnCode::INVALID_MEMORY_ACCESS;
        return 0;
    }

    MemoryRegion* mem = nullptr;

    if (m_ram.contains(address, bytes))
    {
        mem = &m_ram;
    }
    else if (m_flash.contains(address, bytes))
    {
        mem = &m_flash;
    }

    if (mem == nullptr || !mem->access.read)
    {
        m_return_code = ReturnCode::INVALID_MEMORY_ACCESS;
        return 0;
    }

    u8* memory = mem->get(address);
    u32 value;

    if (m_has_before_memory_read_hooks)
    {
        if (bytes == 1)
        {
            value = *(memory);
        }
        else if (bytes == 2)
        {
            value = *((u16*)memory);
        }
        else
        {
            value = *((u32*)memory);
        }

        for (auto [id, func_ptr, func_std, ctx] : m_before_memory_read_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, address, bytes, value, ctx);
            }
            else
            {
                func_std(*this, address, bytes, value, ctx);
            }
        }
        cleanup_hooks();
    }

    if (m_return_code != ReturnCode::OK)
    {
        return 0;
    }

    // read again since hook might have changed data
    if (bytes == 1)
    {
        value = *(memory);
    }
    else if (bytes == 2)
    {
        value = *((u16*)memory);
    }
    else
    {
        value = *((u32*)memory);
    }

    if (m_has_after_memory_read_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_after_memory_read_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, address, bytes, value, ctx);
            }
            else
            {
                func_std(*this, address, bytes, value, ctx);
            }
        }
        cleanup_hooks();
    }

    return value;
}

void Emulator::write_memory_internal(u32 address, u32 value, u8 bytes)
{
    if (m_return_code != ReturnCode::OK)
    {
        return;
    }

    if (bytes != 4 && bytes != 2 && bytes != 1)
    {
        m_return_code = ReturnCode::INVALID_MEMORY_ACCESS;
        return;
    }

    MemoryRegion* mem = nullptr;

    if (m_ram.contains(address, bytes))
    {
        mem = &m_ram;
    }
    else if (m_flash.contains(address, bytes))
    {
        mem = &m_flash;
    }

    if (mem == nullptr || !mem->access.write)
    {
        m_return_code = ReturnCode::INVALID_MEMORY_ACCESS;
        return;
    }

    u8* memory = mem->get(address);

    if (bytes == 1)
    {
        value = (u8)value;
    }
    else if (bytes == 2)
    {
        value = (u16)value;
    }

    if (m_has_before_memory_write_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_before_memory_write_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, address, bytes, value, ctx);
            }
            else
            {
                func_std(*this, address, bytes, value, ctx);
            }
        }
        cleanup_hooks();
    }

    if (m_return_code != ReturnCode::OK)
    {
        return;
    }

    if (bytes == 1)
    {
        *(memory) = (u8)value;
    }
    else if (bytes == 2)
    {
        *((u16*)memory) = (u16)value;
    }
    else
    {
        *((u32*)memory) = value;
    }

    if (m_has_after_memory_write_hooks)
    {
        for (auto [id, func_ptr, func_std, ctx] : m_after_memory_write_hooks)
        {
            UNUSED(id);
            if (func_ptr != nullptr)
            {
                func_ptr(*this, address, bytes, value, ctx);
            }
            else
            {
                func_std(*this, address, bytes, value, ctx);
            }
        }
        cleanup_hooks();
    }
}
