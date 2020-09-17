#include "armory/snapshot.h"

namespace armory
{

    Snapshot::Snapshot(Emulator& emu) : m_emulator(emu)
    {
        m_ram_offset = emu.get_ram_offset();
        m_ram_size = emu.get_ram_size();
        m_ram_data = new u8[m_ram_size];
        m_initialized = false;
        m_hook = m_emulator.add_memory_after_write_hook(Snapshot::on_memory_write, this);
        reset();
    }

    Snapshot::~Snapshot()
    {
        m_emulator.remove_hook(m_hook);
        delete[] m_ram_data;
    }

    void Snapshot::reset()
    {
        m_low_change_start = 0;
        m_low_change_end = 0;

        m_high_change_start = 0;
        m_high_change_end = 0;

        m_initialized = false;
    }

    void Snapshot::backup()
    {
        m_state = m_emulator.get_cpu_state();
        if (!m_initialized)
        {
            m_emulator.read_memory(m_ram_offset, m_ram_data, m_ram_size);
            m_initialized = true;
        }
        else
        {
            u32 size = m_low_change_end - m_low_change_start;
            if (size != 0)
            {
                u32 offset = m_low_change_start - m_ram_offset;
                m_emulator.read_memory(m_low_change_start, m_ram_data + offset, size);
            }
            size = m_high_change_end - m_high_change_start;
            if (size != 0)
            {
                u32 offset = m_high_change_start - m_ram_offset;
                m_emulator.read_memory(m_high_change_start, m_ram_data + offset, size);
            }
        }
        m_low_change_start = 0;
        m_low_change_end = 0;
        m_high_change_start = 0;
        m_high_change_end = 0;
    }

    void Snapshot::restore()
    {
        if (!m_initialized)
            return;

        m_emulator.set_cpu_state(m_state);

        u32 size = m_low_change_end - m_low_change_start;
        if (size != 0)
        {
            u32 offset = m_low_change_start - m_ram_offset;
            m_emulator.write_memory(m_low_change_start, m_ram_data + offset, size);
        }
        size = m_high_change_end - m_high_change_start;
        if (size != 0)
        {
            u32 offset = m_high_change_start - m_ram_offset;
            m_emulator.write_memory(m_high_change_start, m_ram_data + offset, size);
        }
        m_low_change_start = 0;
        m_low_change_end = 0;
        m_high_change_start = 0;
        m_high_change_end = 0;
    }

    void Snapshot::on_memory_write(Emulator& emu, u32 address, u32 size, u32, void* user_data)
    {
        auto snapshot = (Snapshot*)user_data;
        auto sp_boundary = emu.read_register(Register::SP) - 20 * 4;

        if (address < sp_boundary)
        {
            if (snapshot->m_low_change_start == snapshot->m_low_change_end)
            {
                snapshot->m_low_change_start = address;
                snapshot->m_low_change_end = address + size;
            }
            else
            {
                if (address < snapshot->m_low_change_start)
                {
                    snapshot->m_low_change_start = address;
                }
                if (address + size > snapshot->m_low_change_end)
                {
                    snapshot->m_low_change_end = address + size;
                }
            }
        }
        else
        {
            if (snapshot->m_high_change_start == snapshot->m_high_change_end)
            {
                snapshot->m_high_change_start = address;
                snapshot->m_high_change_end = address + size;
            }
            else
            {
                if (address < snapshot->m_high_change_start)
                {
                    snapshot->m_high_change_start = address;
                }
                if (address + size > snapshot->m_high_change_end)
                {
                    snapshot->m_high_change_end = address + size;
                }
            }
        }
    }
} // namespace armory
