#include "m-ulator/arm_functions.h"
#include "m-ulator/emulator.h"
#include "m-ulator/mnemonics.h"
#include <cmath>

// #include <iomanip>
// #include <iostream>

using namespace mulator;

#define cpu m_cpu_state
#define ADVANCE_PC                                       \
    if (increment_pc && m_return_code == ReturnCode::OK) \
        write_register_internal(Register::PC, cpu.registers[PC] + instr.size);

#define UPDATE_PSR m_psr_updated = true;

#define CHECK_SHIFT(X)                                      \
    if (!X)                                                 \
    {                                                       \
        m_return_code = ReturnCode::INVALID_SHIFT_ARGUMENT; \
        return false;                                       \
    }
#define CHECK_IMM(X)                                   \
    if (!X)                                            \
    {                                                  \
        m_return_code = ReturnCode::INVALID_IMMEDIATE; \
        return false;                                  \
    }

using OperandType = Instruction::OperandType;

bool Emulator::in_IT_block() const
{
    if (m_decoder.get_architecture() == Architecture::ARMv6M)
    {
        return false;
    }

    return _4BIT(cpu.psr.it_state) != 0;
}

bool Emulator::last_in_IT_block() const
{
    if (m_decoder.get_architecture() == Architecture::ARMv6M)
    {
        return false;
    }
    return _4BIT(cpu.psr.it_state) == 0b1000;
}

Condition Emulator::pop_IT_condition()
{
    Condition c = (Condition)(cpu.psr.it_state >> 4);
    if (_3BIT(cpu.psr.it_state) == 0)
    {
        cpu.psr.it_state = 0;
    }
    else
    {
        cpu.psr.it_state = (cpu.psr.it_state & 0xE0) | ((cpu.psr.it_state << 1) & 0x1F);
    }
    UPDATE_PSR
    return c;
}

bool Emulator::evaluate_condition(const Instruction& instr)
{
    bool result = false;

    Condition condition = instr.condition;

    if (in_IT_block())
    {
        condition = pop_IT_condition();
    }

    // Evaluate base condition.
    switch (condition >> 1)
    {
    case 0b000:
        result = cpu.psr.Z;
        break; // EQ or NE
    case 0b001:
        result = cpu.psr.C;
        break; // CS or CC
    case 0b010:
        result = cpu.psr.N;
        break; // MI or PL
    case 0b011:
        result = cpu.psr.V;
        break; // VS or VC
    case 0b100:
        result = cpu.psr.C && !cpu.psr.Z;
        break; // HI or LS
    case 0b101:
        result = cpu.psr.N == cpu.psr.V;
        break; // GE or LT
    case 0b110:
        result = cpu.psr.N == cpu.psr.V && !cpu.psr.Z;
        break; // GT or LE
    case 0b111:
        result = true;
        break; // AL, condition flag values in the set '111x' indicate the instruction is always executed.
    }

    // Otherwise, invert condition if necessary.
    if ((condition & 1) == 1)
    {
        result = !result;
    }

    if (condition == 0b1111)
    {
        m_return_code = ReturnCode::UNSUPPORTED;
        return false;
    }

    return result;
}

void Emulator::branch_write_PC(u32 address)
{
    write_register_internal(Register::PC, address & 0xFFFFFFFE);
}

void Emulator::bx_write_PC(u32 address)
{
    if ((address & 1) == 0)
    {
        m_return_code = ReturnCode::HARD_FAULT;
        return;
    }
    write_register_internal(Register::PC, address & 0xFFFFFFFE);
}

void Emulator::blx_write_PC(u32 address)
{
    bx_write_PC(address);
}

bool Emulator::exclusive_monitors_pass(u32 address, u32 align) const
{
    UNUSED(align);
    return address == m_cpu_state.exclusive_address;
}

void Emulator::set_exclusive_monitors(u32 address, u32 align)
{
    if (arm_functions::align(address, align) != address)
    {
        m_return_code = ReturnCode::INVALID_ALIGNMENT;
        return;
    }
    m_cpu_state.exclusive_address = address;
}

bool Emulator::in_priviledged_mode() const
{
    return true;
}

i32 Emulator::get_execution_priority() const
{
    return 0;
}

bool Emulator::execute(const Instruction& instr)
{
    m_psr_updated = false;
    bool instruction_executed = false;
    bool increment_pc = true;
    if (instr.name == Mnemonic::ADC)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            if (instr.uses_only_registers())
            {
                auto [shift_ok, shifted] = arm_functions::shift(read_register_internal(instr.Rm), instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = shifted;
            }
            else
            {
                value = instr.imm;
            }
            auto [result, carry, overflow] = arm_functions::add_with_carry(read_register_internal(instr.Rn), value, cpu.psr.C);
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                cpu.psr.C = carry;
                cpu.psr.V = overflow;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::ADD || instr.name == Mnemonic::ADDW)
    {
        if (evaluate_condition(instr))
        {
            u32 value = 0;
            if (instr.uses_only_registers())
            {
                auto [shift_ok, shifted] = arm_functions::shift(read_register_internal(instr.Rm), instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = shifted;
            }
            else
            {
                value = instr.imm;
            }
            auto [result, carry, overflow] = arm_functions::add_with_carry(read_register_internal(instr.Rn), value, 0);
            if (instr.Rd == Register::PC)
            {
                cpu.registers[PC] = result & ~((u32)1);
                increment_pc = false;
            }
            else
            {
                write_register_internal(instr.Rd, result);
                if (instr.flags.S)
                {
                    cpu.psr.N = result >> 31;
                    cpu.psr.Z = (result == 0);
                    cpu.psr.C = carry;
                    cpu.psr.V = overflow;
                    UPDATE_PSR
                }
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::AND || instr.name == Mnemonic::BIC || instr.name == Mnemonic::EOR || instr.name == Mnemonic::ORR || instr.name == Mnemonic::ORN)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            u8 carry;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, s, c] = arm_functions::shift_c(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = s;
                carry = c;
            }
            else
            {
                auto [imm_ok, s, c] = arm_functions::thumb_expand_imm_C(instr.imm, cpu.psr.C);
                CHECK_IMM(imm_ok);
                value = s;
                carry = c;
            }

            u32 reg = read_register_internal(instr.Rn);

            u32 result;
            if (instr.name == Mnemonic::AND)
            {
                result = reg & value;
            }
            else if (instr.name == Mnemonic::BIC)
            {
                result = reg & ~value;
            }
            else if (instr.name == Mnemonic::EOR)
            {
                result = reg ^ value;
            }
            else if (instr.name == Mnemonic::ORR)
            {
                result = reg | value;
            }
            else // if (instr.name == Mnemonic::ORN)
            {
                result = reg | ~value;
            }
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                cpu.psr.C = carry;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::ASR || instr.name == Mnemonic::LSL || instr.name == Mnemonic::LSR || instr.name == Mnemonic::ROR)
    {
        if (evaluate_condition(instr))
        {
            u32 amount = 0;
            u32 base;
            if (instr.uses_only_registers())
            {
                {
                    u32 reg = read_register_internal(instr.Rm);

                    amount = reg & 0xFF;
                }
                {
                    u32 reg = read_register_internal(instr.Rn);

                    base = reg;
                }
            }
            else
            {
                amount = instr.shift_amount;

                u32 reg = read_register_internal(instr.Rm);

                base = reg;
            }
            auto [shift_ok, result, carry] = arm_functions::shift_c(base, instr.shift_type, amount, cpu.psr.C);
            CHECK_SHIFT(shift_ok);
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                cpu.psr.C = carry;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::RRX)
    {
        if (evaluate_condition(instr))
        {
            u32 reg = read_register_internal(instr.Rm);

            auto [shift_ok, result, carry] = arm_functions::shift_c(reg, ShiftType::RRX, 1, cpu.psr.C);
            CHECK_SHIFT(shift_ok);
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                cpu.psr.C = carry;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (
        instr.name == Mnemonic::LDR || instr.name == Mnemonic::LDRT || instr.name == Mnemonic::LDRH || instr.name == Mnemonic::LDRHT || instr.name == Mnemonic::LDRB || instr.name == Mnemonic::LDRBT || instr.name == Mnemonic::LDRSH || instr.name == Mnemonic::LDRSHT || instr.name == Mnemonic::LDRSB || instr.name == Mnemonic::LDRSBT)
    {
        if (evaluate_condition(instr))
        {
            u8 bytes = 4;
            if (instr.name == Mnemonic::LDRB || instr.name == Mnemonic::LDRBT || instr.name == Mnemonic::LDRSB || instr.name == Mnemonic::LDRSBT)
            {
                bytes = 1;
            }
            else if (instr.name == Mnemonic::LDRH || instr.name == Mnemonic::LDRHT || instr.name == Mnemonic::LDRSH || instr.name == Mnemonic::LDRSHT)
            {
                bytes = 2;
            }

            if (instr.operand_type == OperandType::RI)
            {
                u32 reg = read_register_internal(Register::PC);

                u32 address = arm_functions::align(reg, 4);
                address += ((instr.flags.add) ? instr.imm : -instr.imm);
                u32 result = read_memory_internal(address, bytes);
                if (instr.name == Mnemonic::LDRSB || instr.name == Mnemonic::LDRSH)
                {
                    result = arm_functions::sign_extend(result, 8 * bytes);
                }
                write_register_internal(instr.Rd, result);
            }
            else
            {
                u32 offset_address = 0;

                u32 address = read_register_internal(instr.Rn);

                if (instr.uses_only_registers())
                {
                    auto [shift_ok, shifted] = arm_functions::shift(read_register_internal(instr.Rm), instr.shift_type, instr.shift_amount, cpu.psr.C);
                    CHECK_SHIFT(shift_ok);
                    address += shifted;
                }
                else
                {
                    offset_address = address + ((instr.flags.add) ? instr.imm : -instr.imm);
                    if (instr.flags.index)
                    {
                        address = offset_address;
                    }
                }

                bool ignore_alignment = _1BIT(cpu.CCR >> 3) == 0;

                if (!ignore_alignment && arm_functions::align(address, bytes) != address)
                {
                    m_return_code = ReturnCode::HARD_FAULT;
                    return false;
                }

                u32 result = read_memory_internal(address, bytes);

                if (instr.name == Mnemonic::LDRSB || instr.name == Mnemonic::LDRSBT || instr.name == Mnemonic::LDRSH || instr.name == Mnemonic::LDRSHT)
                {
                    result = arm_functions::sign_extend(result, 8 * bytes);
                }

                write_register_internal(instr.Rd, result);

                if (instr.flags.wback)
                {
                    write_register_internal(instr.Rn, offset_address);
                }
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::MUL)
    {
        if (evaluate_condition(instr))
        {
            u32 result = read_register_internal(instr.Rn) * read_register_internal(instr.Rm);
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::SBC)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, shifted] = arm_functions::shift(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = shifted;
            }
            else
            {
                value = instr.imm;
            }
            u32 reg = read_register_internal(instr.Rn);
            auto [result, carry, overflow] = arm_functions::add_with_carry(reg, ~value, cpu.psr.C);
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                cpu.psr.C = carry;
                cpu.psr.V = overflow;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::STR || instr.name == Mnemonic::STRT || instr.name == Mnemonic::STRH || instr.name == Mnemonic::STRHT || instr.name == Mnemonic::STRB || instr.name == Mnemonic::STRBT)
    {
        if (evaluate_condition(instr))
        {
            u32 offset_addr = 0;

            u32 address = read_register_internal(instr.Rn);

            if (instr.uses_only_registers())
            {
                auto [shift_ok, shifted] = arm_functions::shift(read_register_internal(instr.Rm), instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                address += shifted;
            }
            else
            {
                offset_addr = address + ((instr.flags.add) ? instr.imm : -instr.imm);
                if (instr.flags.index)
                {
                    address = offset_addr;
                }
            }

            u8 bytes = 4;
            if (instr.name == Mnemonic::STRB || instr.name == Mnemonic::STRBT)
            {
                bytes = 1;
            }
            else if (instr.name == Mnemonic::STRH || instr.name == Mnemonic::STRHT)
            {
                bytes = 2;
            }

            bool ignore_alignment = _1BIT(cpu.CCR >> 3) == 0;

            if (!ignore_alignment && arm_functions::align(address, bytes) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            write_memory_internal(address, read_register_internal(instr.Rd), bytes);

            if (instr.flags.wback)
            {
                write_register_internal(instr.Rn, offset_addr);
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::SUB || instr.name == Mnemonic::SUBW)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, shifted] = arm_functions::shift(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = shifted;
            }
            else
            {
                value = instr.imm;
            }

            u32 reg = read_register_internal(instr.Rn);

            auto [result, carry, overflow] = arm_functions::add_with_carry(reg, ~value, 1);

            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                cpu.psr.C = carry;
                cpu.psr.V = overflow;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::RSB)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, shifted] = arm_functions::shift(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = shifted;
            }
            else
            {
                value = instr.imm;
            }
            u32 reg = read_register_internal(instr.Rn);
            auto [result, carry, overflow] = arm_functions::add_with_carry(~reg, value, 1);
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                cpu.psr.C = carry;
                cpu.psr.V = overflow;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::CMN)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, shifted] = arm_functions::shift(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = shifted;
            }
            else
            {
                value = instr.imm;
            }
            u32 reg = read_register_internal(instr.Rn);
            auto [result, carry, overflow] = arm_functions::add_with_carry(reg, value, 0);
            cpu.psr.N = result >> 31;
            cpu.psr.Z = (result == 0);
            cpu.psr.C = carry;
            cpu.psr.V = overflow;
            UPDATE_PSR
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::CMP)
    {
        if (evaluate_condition(instr))
        {
            u32 value = 0;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, shifted] = arm_functions::shift(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = shifted;
            }
            else
            {
                value = instr.imm;
            }

            u32 reg = read_register_internal(instr.Rn);

            auto [result, carry, overflow] = arm_functions::add_with_carry(reg, ~value, 1);

            cpu.psr.N = result >> 31;
            cpu.psr.Z = (result == 0);
            cpu.psr.C = carry;
            cpu.psr.V = overflow;
            UPDATE_PSR
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::MOV || instr.name == Mnemonic::MOVW || instr.name == Mnemonic::MOVT)
    {
        if (evaluate_condition(instr))
        {
            if (instr.Rd == Register::PC)
            {
                u32 reg = read_register_internal(instr.Rm);
                branch_write_PC(reg);
            }
            else
            {
                u32 value = 0;
                u8 carry = cpu.psr.C;
                if (instr.name == Mnemonic::MOVW)
                {
                    value = instr.imm;
                }
                else if (instr.name == Mnemonic::MOVT)
                {
                    u32 reg = read_register_internal(instr.Rd);
                    value = (reg & 0xFFFF) | (instr.imm << 16);
                }
                else if (instr.uses_only_registers())
                {
                    u32 reg = read_register_internal(instr.Rm);
                    value = reg;
                }
                else
                {
                    auto [imm_ok, v, c] = arm_functions::thumb_expand_imm_C(instr.imm, carry);
                    CHECK_IMM(imm_ok);
                    value = v;
                    carry = c;
                }
                write_register_internal(instr.Rd, value);
                if (instr.flags.S)
                {
                    cpu.psr.N = value >> 31;
                    cpu.psr.Z = (value == 0);
                    cpu.psr.C = carry;
                    UPDATE_PSR
                }
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::MVN)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            u8 carry;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, v, c] = arm_functions::shift_c(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = v;
                carry = c;
            }
            else
            {
                auto [imm_ok, v, c] = arm_functions::thumb_expand_imm_C(instr.imm, cpu.psr.C);
                CHECK_IMM(imm_ok);
                value = v;
                carry = c;
            }

            u32 result = ~value;
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                cpu.psr.C = carry;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::RBIT)
    {
        if (evaluate_condition(instr))
        {
            u32 n = read_register_internal(instr.Rm);

            // reverse bits in 32-bit integer
            n = ((n >> 1) & 0x55555555) | ((n << 1) & 0xaaaaaaaa);
            n = ((n >> 2) & 0x33333333) | ((n << 2) & 0xcccccccc);
            n = ((n >> 4) & 0x0f0f0f0f) | ((n << 4) & 0xf0f0f0f0);
            n = ((n >> 8) & 0x00ff00ff) | ((n << 8) & 0xff00ff00);
            n = ((n >> 16) & 0x0000ffff) | ((n << 16) & 0xffff0000);

            write_register_internal(instr.Rd, n);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::REV)
    {
        if (evaluate_condition(instr))
        {
            u32 val = read_register_internal(instr.Rm);
            u32 result = ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) | ((val & 0xFF0000) >> 8) | ((val >> 24) & 0xFF);
            write_register_internal(instr.Rd, result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::REV16)
    {
        if (evaluate_condition(instr))
        {
            u32 val = read_register_internal(instr.Rm);
            u32 result = ((val & 0xFF) << 8) | ((val & 0xFF00) >> 8) | ((val & 0xFF0000) << 8) | ((val & 0xFF000000) >> 8);
            write_register_internal(instr.Rd, result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::REVSH)
    {
        if (evaluate_condition(instr))
        {
            u32 val = read_register_internal(instr.Rm);
            u32 result = arm_functions::sign_extend(((val & 0xFF) << 8) | ((val & 0xFF00) >> 8), 16);
            write_register_internal(instr.Rd, result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::SXTB || instr.name == Mnemonic::SXTH || instr.name == Mnemonic::UXTB || instr.name == Mnemonic::UXTH)
    {
        if (evaluate_condition(instr))
        {
            u32 reg = read_register_internal(instr.Rm);
            auto [shift_ok, rotated] = arm_functions::ROR(reg, instr.shift_amount);
            CHECK_SHIFT(shift_ok);
            if (instr.name == Mnemonic::SXTB)
            {
                write_register_internal(instr.Rd, arm_functions::sign_extend(rotated & 0xFF, 8));
            }
            else if (instr.name == Mnemonic::SXTH)
            {
                write_register_internal(instr.Rd, arm_functions::sign_extend(rotated & 0xFFFF, 16));
            }
            else if (instr.name == Mnemonic::UXTB)
            {
                write_register_internal(instr.Rd, rotated & 0xFF);
            }
            else // UXTH
            {
                write_register_internal(instr.Rd, rotated & 0xFFFF);
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::TST)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            u8 carry;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, s, c] = arm_functions::shift_c(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = s;
                carry = c;
            }
            else
            {
                auto [imm_ok, s, c] = arm_functions::thumb_expand_imm_C(instr.imm, cpu.psr.C);
                CHECK_IMM(imm_ok);
                value = s;
                carry = c;
            }
            u32 reg = read_register_internal(instr.Rn);
            u32 result = reg & value;
            cpu.psr.N = result >> 31;
            cpu.psr.Z = (result == 0);
            cpu.psr.C = carry;
            UPDATE_PSR
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::ADR)
    {
        if (evaluate_condition(instr))
        {
            u32 reg = read_register_internal(PC);
            u32 value = arm_functions::align(reg, 4);
            value += (instr.flags.add) ? instr.imm : -instr.imm;
            write_register_internal(instr.Rd, value);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::LDM)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(instr.Rn);
            if (arm_functions::align(address, 4) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }
            u32 cnt = 0;
            for (u32 i = 0; i < 15; ++i)
            {
                if ((instr.imm >> i) & 1)
                {
                    write_register_internal((Register)i, read_memory_internal(address, 4));
                    address += 4;
                    cnt++;
                }
            }
            if ((instr.imm >> 15) & 1)
            {
                bx_write_PC(read_memory_internal(address, 4));
                increment_pc = false;
            }
            if (instr.flags.wback)
            {
                write_register_internal(instr.Rn, address);
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::STM)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(instr.Rn);
            if (arm_functions::align(address, 4) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            u32 cnt = 0;
            for (u32 i = 0; i < 15; ++i)
            {
                if ((instr.imm >> i) & 1)
                {
                    write_memory_internal(address, read_register_internal((Register)i), 4);
                    address += 4;
                    cnt++;
                }
            }
            if (instr.flags.wback)
            {
                write_register_internal(instr.Rn, address);
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::CBZ || instr.name == Mnemonic::CBNZ)
    {
        u32 value = read_register_internal(instr.Rn);
        if ((instr.name == Mnemonic::CBZ && value == 0) || (instr.name == Mnemonic::CBNZ && value != 0))
        {
            branch_write_PC(read_register_internal(PC) + instr.imm);
            increment_pc = false;
        }
        ADVANCE_PC
        instruction_executed = true;
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::BLX)
    {
        if (evaluate_condition(instr))
        {
            write_register_internal(LR, (read_register_internal(PC) - 2) | 1);
            blx_write_PC(read_register_internal(instr.Rm));
            instruction_executed = true;
            increment_pc = false;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::BX)
    {
        if (evaluate_condition(instr))
        {
            u32 reg = read_register_internal(instr.Rm);
            bx_write_PC(reg);
            instruction_executed = true;
            increment_pc = false;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::B)
    {
        if (evaluate_condition(instr))
        {
            u32 reg = read_register_internal(PC);
            branch_write_PC(reg + instr.imm);
            instruction_executed = true;
            increment_pc = false;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::BL)
    {
        if (evaluate_condition(instr))
        {
            u32 reg = read_register_internal(PC);
            write_register_internal(LR, reg | 1);
            branch_write_PC(reg + instr.imm);

            instruction_executed = true;
            increment_pc = false;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::POP)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(SP);
            if (arm_functions::align(address, 4) != address && !instr.flags.unaligned_allowed)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            u32 end_val = address + arm_functions::bit_count(instr.imm) * 4;

            for (u32 i = 0; i < 15; ++i)
            {
                if ((instr.imm >> i) & 1)
                {
                    write_register_internal((Register)i, read_memory_internal(address, 4));
                    address += 4;
                }
            }
            if ((instr.imm >> 15) & 1)
            {
                bx_write_PC(read_memory_internal(address, 4));
                increment_pc = false;
            }

            write_register_internal(SP, end_val);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::PUSH)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(SP);

            address -= arm_functions::bit_count(instr.imm) * 4;

            u32 end_val = address;

            if (arm_functions::align(address, 4) != address && !instr.flags.unaligned_allowed)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            for (u32 i = 0; i < 15; ++i)
            {
                if ((instr.imm >> i) & 1)
                {
                    u32 reg1 = read_register_internal((Register)i);

                    write_memory_internal(address, reg1, 4);
                    address += 4;
                }
            }

            write_register_internal(SP, end_val);

            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::IT && m_decoder.get_architecture() >= Architecture::ARMv7M)
    {
        cpu.psr.it_state = instr.imm;
        UPDATE_PSR
        ADVANCE_PC
        instruction_executed = true;
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::NOP || instr.name == Mnemonic::SEV || instr.name == Mnemonic::DSB || instr.name == Mnemonic::ISB || instr.name == Mnemonic::DMB || instr.name == Mnemonic::CSDB || instr.name == Mnemonic::DBG || instr.name == Mnemonic::CLREX || instr.name == Mnemonic::SSBB || instr.name == Mnemonic::PSSBB || instr.name == Mnemonic::PLD || instr.name == Mnemonic::PLI)
    {
        ADVANCE_PC
        instruction_executed = true;
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::UDF)
    {
        m_return_code = ReturnCode::UNDEFINED;
        return false;
    }
    else if (instr.name == Mnemonic::STMDB)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(instr.Rn);

            address -= arm_functions::bit_count(instr.imm) * 4;
            u32 end_val = address;

            if (arm_functions::align(address, 4) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            for (u32 i = 0; i < 15; ++i)
            {
                if ((instr.imm >> i) & 1)
                {
                    u32 reg = read_register_internal((Register)i);

                    write_memory_internal(address, reg, 4);
                    address += 4;
                }
            }

            if (instr.flags.wback)
            {
                write_register_internal(instr.Rn, end_val);
            }

            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::LDMDB)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(instr.Rn);

            address -= arm_functions::bit_count(instr.imm) * 4;
            u32 end_val = address;

            if (arm_functions::align(address, 4) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            for (u32 i = 0; i < 15; ++i)
            {
                if ((instr.imm >> i) & 1)
                {
                    write_register_internal((Register)i, read_memory_internal(address, 4));
                    address += 4;
                }
            }

            if ((instr.imm >> 15) & 1)
            {
                bx_write_PC(read_memory_internal(address, 4));
                increment_pc = false;
            }

            if (instr.flags.wback && _1BIT(instr.imm > instr.Rn) != 0)
            {
                write_register_internal(instr.Rn, end_val);
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::STRD)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(instr.Rn);
            u32 offset_address = address + ((instr.flags.add) ? instr.imm : -instr.imm);
            if (instr.flags.index)
            {
                address = offset_address;
            }

            if (arm_functions::align(address, 4) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            write_memory_internal(address, read_register_internal(instr.Rd), 4);
            write_memory_internal(address + 4, read_register_internal(instr.Rm), 4);

            if (instr.flags.wback)
            {
                write_register_internal(instr.Rn, offset_address);
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::LDRD)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(instr.Rn);

            if (instr.operand_type == OperandType::RRI) // LDRD (literal)
            {
                address = arm_functions::align(address, 4);
            }

            u32 offset_address = address + ((instr.flags.add) ? instr.imm : -instr.imm);

            if (instr.flags.index)
            {
                address = offset_address;
            }

            if (arm_functions::align(address, 4) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            write_register_internal(instr.Rd, read_memory_internal(address, 4));
            write_register_internal(instr.Rm, read_memory_internal(address + 4, 4));

            if (instr.flags.wback)
            {
                write_register_internal(instr.Rn, offset_address);
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::STREX || instr.name == Mnemonic::STREXB || instr.name == Mnemonic::STREXH)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(instr.Rn);
            if (instr.name == Mnemonic::STREX)
            {
                address += instr.imm;
            }

            u32 bytes = 4;
            if (instr.name == Mnemonic::STREXB)
            {
                bytes = 1;
            }
            else if (instr.name == Mnemonic::STREXH)
            {
                bytes = 2;
            }

            if (arm_functions::align(address, bytes) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            if (exclusive_monitors_pass(address, bytes))
            {
                write_memory_internal(address, read_register_internal(instr.Rm), bytes);
                write_register_internal(instr.Rd, 0);
            }
            else
            {
                write_register_internal(instr.Rd, 1);
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::LDREX || instr.name == Mnemonic::LDREXB || instr.name == Mnemonic::LDREXH)
    {
        if (evaluate_condition(instr))
        {
            u32 address = read_register_internal(instr.Rn);
            if (instr.name == Mnemonic::LDREX)
            {
                address += instr.imm;
            }

            u8 bytes = 4;
            if (instr.name == Mnemonic::LDREXB)
            {
                bytes = 1;
            }
            else if (instr.name == Mnemonic::LDREXH)
            {
                bytes = 2;
            }

            if (arm_functions::align(address, bytes) != address)
            {
                m_return_code = ReturnCode::HARD_FAULT;
                return false;
            }

            set_exclusive_monitors(address, bytes);
            write_register_internal(instr.Rd, read_memory_internal(address, bytes));
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::TBB || instr.name == Mnemonic::TBH)
    {
        if (evaluate_condition(instr))
        {
            u32 reg1 = read_register_internal(instr.Rn);
            u32 reg2 = read_register_internal(instr.Rm);

            u32 data;
            if (instr.name == Mnemonic::TBH)
            {
                auto [shift_ok, shifted] = arm_functions::LSL(reg2, 1);
                CHECK_SHIFT(shift_ok);
                u32 address = reg1 + shifted;
                bool ignore_alignment = _1BIT(cpu.CCR >> 3) == 0;
                if (!ignore_alignment && arm_functions::align(address, 2) != address)
                {
                    m_return_code = ReturnCode::HARD_FAULT;
                    return false;
                }
                data = read_memory_internal(address, 2);
            }
            else
            {
                data = read_memory_internal(reg1 + reg2, 1);
            }

            branch_write_PC(read_register_internal(PC) + 2 * data);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::TEQ)
    {
        if (evaluate_condition(instr))
        {
            u32 value;
            u8 carry;
            if (instr.uses_only_registers())
            {
                u32 reg = read_register_internal(instr.Rm);
                auto [shift_ok, s, c] = arm_functions::shift_c(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
                CHECK_SHIFT(shift_ok);
                value = s;
                carry = c;
            }
            else
            {
                auto [imm_ok, s, c] = arm_functions::thumb_expand_imm_C(instr.imm, cpu.psr.C);
                CHECK_IMM(imm_ok);
                value = s;
                carry = c;
            }

            u32 reg = read_register_internal(instr.Rn);
            u32 result = reg ^ value;

            cpu.psr.N = result >> 31;
            cpu.psr.Z = (result == 0);
            cpu.psr.C = carry;
            UPDATE_PSR

            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::SSAT || instr.name == Mnemonic::USAT)
    {
        if (evaluate_condition(instr))
        {
            u32 reg = read_register_internal(instr.Rn);
            auto [shift_ok, value] = arm_functions::shift(reg, instr.shift_type, instr.shift_amount, cpu.psr.C);
            CHECK_SHIFT(shift_ok);
            u32 result;
            bool sat;
            if (instr.name == Mnemonic::SSAT)
            {
                auto [r, s] = arm_functions::signed_sat_Q(value, instr.imm);
                sat = s;
                result = arm_functions::sign_extend(r, 5);
            }
            else
            {
                auto [r, s] = arm_functions::unsigned_sat_Q(value, instr.imm);
                sat = s;
                result = r;
            }
            write_register_internal(instr.Rd, result);
            if (sat)
            {
                cpu.psr.Q = true;
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::SBFX || instr.name == Mnemonic::UBFX)
    {
        if (evaluate_condition(instr))
        {
            u32 lsbit = instr.imm;
            u32 width = instr.imm2;

            u32 reg = read_register_internal(instr.Rn);

            u32 result = (reg >> lsbit) & (0xFFFFFFFF >> (32 - width));

            if (width + lsbit <= 32)
            {
                if (instr.name == Mnemonic::SBFX)
                {
                    result = arm_functions::sign_extend(result, width);
                }
                write_register_internal(instr.Rd, result);
            }
            else
            {
                m_return_code = ReturnCode::UNPREDICTABLE;
                return false;
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::BFI || instr.name == Mnemonic::BFC)
    {
        if (evaluate_condition(instr))
        {
            u32 lsb = instr.imm;
            u32 width = instr.imm2;
            u32 msb = lsb + width;

            u32 old = read_register_internal(instr.Rd);

            u32 low = 0;
            u32 high = 0;
            if (lsb > 0)
                low = old & (0xFFFFFFFF >> (32 - lsb));
            if (msb < 32)
                high = old & ((u32)0xFFFFFFFF << msb);
            u32 result = low | high;

            if (instr.name == Mnemonic::BFI)
            {
                u32 value = read_register_internal(instr.Rn) & (0xFFFFFFFF >> (32 - width));
                result |= value << lsb;
            }

            write_register_internal(instr.Rd, result);

            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::CLZ)
    {
        if (evaluate_condition(instr))
        {
            u32 result = 0;
            u32 val = read_register_internal(instr.Rm);
            // count leading zeros
            for (u32 i = 0; i < 32; ++i)
            {
                if (val & 0x80000000)
                {
                    break;
                }
                result++;
                val <<= 1;
            }
            write_register_internal(instr.Rd, result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::MLA || instr.name == Mnemonic::MLS)
    {
        if (evaluate_condition(instr))
        {
            u32 op1 = read_register_internal(instr.Rn);
            u32 op2 = read_register_internal(instr.Rm);
            u32 addend = read_register_internal(instr.Ra);
            u32 result = op1 * op2;
            if (instr.name == Mnemonic::MLA)
            {
                result += addend;
            }
            else
            {
                result = addend - result;
            }
            write_register_internal(instr.Rd, result);
            if (instr.flags.S)
            {
                cpu.psr.N = result >> 31;
                cpu.psr.Z = (result == 0);
                UPDATE_PSR
            }
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::SMULL)
    {
        if (evaluate_condition(instr))
        {
            u32 reg1 = read_register_internal(instr.Rn);
            u32 reg2 = read_register_internal(instr.Rm);
            i64 op1 = (i32)reg1;
            i64 op2 = (i32)reg2;
            i64 result = op1 * op2;
            write_register_internal(instr.Rd, (u32)(result >> 32));
            write_register_internal(instr.Ra, (u32)result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::UMULL)
    {
        if (evaluate_condition(instr))
        {
            u32 reg1 = read_register_internal(instr.Rn);
            u32 reg2 = read_register_internal(instr.Rm);
            u64 op1 = reg1;
            u64 op2 = reg2;
            u64 result = op1 * op2;
            write_register_internal(instr.Rd, (u32)(result >> 32));
            write_register_internal(instr.Ra, (u32)result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::SDIV)
    {
        if (evaluate_condition(instr))
        {
            u32 reg1 = read_register_internal(instr.Rn);
            u32 reg2 = read_register_internal(instr.Rm);
            i32 op1 = reg1;
            i32 op2 = reg2;
            i32 result = 0;
            bool div_by_zero_trap = _1BIT(cpu.CCR >> 4) == 1;
            if (op2 == 0)
            {
                if (div_by_zero_trap)
                {
                    m_return_code = ReturnCode::HARD_FAULT;
                    return false;
                }
            }
            else
            {
                result = (u32)std::llround((double)op1 / (double)op2);
            }
            write_register_internal(instr.Rd, (u32)result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::UDIV)
    {
        if (evaluate_condition(instr))
        {
            u32 op1 = read_register_internal(instr.Rn);
            u32 op2 = read_register_internal(instr.Rm);
            u32 result = 0;
            bool div_by_zero_trap = _1BIT(cpu.CCR >> 4) == 1;
            if (op2 == 0)
            {
                if (div_by_zero_trap)
                {
                    m_return_code = ReturnCode::HARD_FAULT;
                    return false;
                }
            }
            else
            {
                result = (u32)std::llround((double)op1 / (double)op2);
            }
            write_register_internal(instr.Rd, result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::SMLAL)
    {
        if (evaluate_condition(instr))
        {
            u32 reg1 = read_register_internal(instr.Rn);
            u32 reg2 = read_register_internal(instr.Rm);
            u32 reg3 = read_register_internal(instr.Rd);
            u32 reg4 = read_register_internal(instr.Ra);
            i64 op1 = (i32)reg1;
            i64 op2 = (i32)reg2;
            i64 add = ((i64)reg3 << 32) | reg4;
            i64 result = op1 * op2 + add;
            write_register_internal(instr.Rd, (u32)(result >> 32));
            write_register_internal(instr.Ra, (u32)result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::UMLAL)
    {
        if (evaluate_condition(instr))
        {
            u32 reg1 = read_register_internal(instr.Rn);
            u32 reg2 = read_register_internal(instr.Rm);
            u32 reg3 = read_register_internal(instr.Rd);
            u32 reg4 = read_register_internal(instr.Ra);
            u64 op1 = (i32)reg1;
            u64 op2 = (i32)reg2;
            u64 add = ((u64)reg3 << 32) | reg4;
            u64 result = op1 * op2 + add;
            write_register_internal(instr.Rd, (u32)(result >> 32));
            write_register_internal(instr.Ra, (u32)result);
            instruction_executed = true;
        }
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::CPS)
    {
        if (in_priviledged_mode())
        {
            bool enable = _1BIT(instr.imm >> 4) == 0;
            bool affectPRI = _1BIT(instr.imm >> 1) == 1;
            bool affectFAULT = _1BIT(instr.imm) == 1;
            if (enable)
            {
                if (affectPRI)
                    cpu.PRIMASK = 0;
                if (affectFAULT)
                    cpu.FAULTMASK = 0;
            }
            else
            {
                if (affectPRI)
                    cpu.PRIMASK = 1;
                if (affectFAULT && get_execution_priority() > -1)
                    cpu.FAULTMASK = 1;
            }
        }
        instruction_executed = true;
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::MRS)
    {
        if (evaluate_condition(instr))
        {
            auto psr = read_register_internal(Register::PSR);
            u32 value = 0;
            u8 tmp = _5BIT(instr.imm >> 3);
            if (tmp == 0b00000)
            {
                if (_1BIT(instr.imm) == 1)
                {
                    // IPSR
                }
                if (_1BIT(instr.imm >> 1) == 1)
                {
                    // EPSR reads as zero
                }
                if (_1BIT(instr.imm >> 2) == 0)
                {
                    value |= psr & 0xF8000000;
                    if (get_architecture() == Architecture::ARMv7EM)
                    {
                        // DSP Extension
                        value |= psr & 0x000F0000;
                    }
                }
            }
            else if (tmp == 0b00001)
            {
                tmp = _3BIT(instr.imm);
                if (in_priviledged_mode())
                {
                    if (tmp == 0b000)
                    {
                        // SP_MAIN
                        value = read_register_internal(Register::SP);
                    }
                    else if (tmp == 0b001)
                    {
                        // SP_PROCESS
                        value = read_register_internal(Register::SP);
                    }
                }
            }
            else if (tmp == 0b00010)
            {
                tmp = _3BIT(instr.imm);

                if (tmp == 0b000 && in_priviledged_mode())
                {
                    value = _1BIT(cpu.PRIMASK);
                }
                else if ((tmp == 0b001 || tmp == 0b010) && in_priviledged_mode())
                {
                    value = cpu.BASEPRI;
                }
                else if (tmp == 0b011 && in_priviledged_mode())
                {
                    value = _1BIT(cpu.FAULTMASK);
                }
                else if (tmp == 0b100)
                {
                    // FP Extension not supported
                    value = _2BIT(cpu.CONTROL);
                }
            }
            write_register_internal(instr.Rd, value);
        }
        instruction_executed = true;
        ADVANCE_PC
        return instruction_executed;
    }
    else if (instr.name == Mnemonic::MSR)
    {
        if (evaluate_condition(instr))
        {
            auto value = read_register_internal(instr.Rn);
            auto psr = read_register_internal(Register::PSR);
            u8 tmp = _5BIT(instr.imm >> 3);
            u8 mask = _2BIT(instr.imm >> 8);
            if (tmp == 0b00000)
            {
                if (_1BIT(instr.imm >> 2) == 0)
                {
                    if (_1BIT(mask) == 1)
                    {
                        if (get_architecture() != Architecture::ARMv7EM)
                        {
                            m_return_code = ReturnCode::UNPREDICTABLE;
                            return false;
                        }
                        else
                        {
                            psr = (psr & ~0x000F0000) | (value & 0x000F0000);
                        }
                    }

                    if (_1BIT(mask >> 1) == 1)
                    {
                        psr = (psr & ~0xF8000000) | (value & 0xF8000000);
                    }
                }
                write_register_internal(Register::PSR, psr);
            }
            else if (tmp == 0b00001)
            {
                tmp = _3BIT(instr.imm);
                if (in_priviledged_mode())
                {
                    if (tmp == 0b000)
                    {
                        // SP_MAIN
                        write_register_internal(Register::SP, value);
                    }
                    else if (tmp == 0b001)
                    {
                        // SP_PROCESS
                        write_register_internal(Register::SP, value);
                    }
                }
            }
            else if (tmp == 0b00010)
            {
                tmp = _3BIT(instr.imm);

                if (tmp == 0b000 && in_priviledged_mode())
                {
                    cpu.PRIMASK = _1BIT(value);
                }
                else if (tmp == 0b001 && in_priviledged_mode())
                {
                    cpu.BASEPRI = _8BIT(value);
                }
                else if (tmp == 0b010 && _8BIT(value) != 0 && (_8BIT(value) < cpu.BASEPRI || cpu.BASEPRI == 0) && in_priviledged_mode())
                {
                    cpu.BASEPRI = _8BIT(value);
                }
                else if (tmp == 0b011 && in_priviledged_mode() && get_execution_priority() > -1)
                {
                    cpu.FAULTMASK = _1BIT(value);
                }
                else if (tmp == 0b100 && in_priviledged_mode())
                {
                    cpu.CONTROL = _2BIT(value);
                    // FP Extension not supported
                }
            }
        }
        instruction_executed = true;
        ADVANCE_PC
        return instruction_executed;
    }

    m_return_code = ReturnCode::UNSUPPORTED;
    return false;
}
