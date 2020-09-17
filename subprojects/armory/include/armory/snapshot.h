#pragma once

#include "m-ulator/emulator.h"

namespace armory
{
    using namespace mulator;

    class Snapshot
    {
    public:
        /*
         * Instantiates a new snapshot for the given emulator.
         * Snapshots can be used to reset an emulator to a previous state.
         * However, resetting invalidates all OTHER snapshots for the same emulator which where backed-up later.
         * Earlier backups work fine.
         *
         * This also registers a before-memory-read-hook in the emulator.
         * Clearing all hooks of the emulator will render the snapshot incorrect.
         */
        Snapshot(Emulator& emu);
        ~Snapshot();

        /*
         * Resets the snapshot.
         * This will cause the next call to backup() to be complete and not incremental based on monitored memory accesses.
         * Necessary when the emulator was restored with an older backup than saved in this snapshot.
         */
        void reset();

        /*
         * Save a backup of the current state including registers and RAM.
         */
        void backup();

        /*
         * Restores the emulator to the backed-up state in the snapshot.
         */
        void restore();

    private:
        Snapshot(const Snapshot&) = delete;
        static void on_memory_write(Emulator& emu, u32 address, u32 size, u32, void* user_data);

        Emulator& m_emulator;
        CPU_State m_state;
        u32 m_hook;
        u32 m_ram_offset;
        u32 m_ram_size;
        u8* m_ram_data;
        bool m_initialized;
        u32 m_low_change_start;
        u32 m_low_change_end;
        u32 m_high_change_start;
        u32 m_high_change_end;
    };
} // namespace armory
