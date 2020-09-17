#pragma once

#include "m-ulator/types.h"
#include "m-ulator/mnemonics.h"
#include "m-ulator/conditions.h"
#include "m-ulator/registers.h"
#include "m-ulator/architectures.h"
#include "m-ulator/shift_types.h"

namespace mulator
{

    struct Instruction
    {
        Instruction();
        ~Instruction() = default;

        u32 address;
        u8 size;
        u32 encoding;

        Mnemonic name;

        Condition condition;

        ShiftType shift_type;
        u32 shift_amount;

        struct
        {
            bool S;
            bool wback;
            bool index;
            bool add;
            bool unaligned_allowed;
        } flags;

        enum class OperandType
        {
            NONE,
            I,    // immediate
            R,    // register
            RI,   // register, immediate
            RR,   // register, register
            RRI,  // register, register, immediate
            RRR,  // register, register, register
            RRII, // register, register, immediate, immediate
            RRRI, // register, register, register,  immediate
            RRRR  // register, register, register,  register
        } operand_type;

        Register Rd;
        Register Rn;
        Register Rm;
        Register Ra;
        u32 imm;
        u32 imm2;

        bool uses_immediate() const;
        bool uses_only_registers() const;
        u32 get_register_count() const;
        u32 get_immediate_count() const;
    };
}
