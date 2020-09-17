#pragma once

#include "armory/instruction_fault_model.h"
#include "armory/register_fault_model.h"

namespace fault_models
{
    using namespace armory;

    extern const InstructionFaultModel* instr_p_skip;
    extern const InstructionFaultModel* instr_p_setbyte;
    extern const InstructionFaultModel* instr_p_clearbyte;
    extern const InstructionFaultModel* instr_p_bitflip;

    extern const InstructionFaultModel* instr_t_skip;
    extern const InstructionFaultModel* instr_t_setbyte;
    extern const InstructionFaultModel* instr_t_clearbyte;
    extern const InstructionFaultModel* instr_t_bitflip;

    extern const RegisterFaultModel* reg_p_clear;
    extern const RegisterFaultModel* reg_p_fill;
    extern const RegisterFaultModel* reg_p_setbyte;
    extern const RegisterFaultModel* reg_p_clearbyte;
    extern const RegisterFaultModel* reg_p_bitset;
    extern const RegisterFaultModel* reg_p_bitclear;

    extern const RegisterFaultModel* reg_o_clear;
    extern const RegisterFaultModel* reg_o_fill;
    extern const RegisterFaultModel* reg_o_setbyte;
    extern const RegisterFaultModel* reg_o_clearbyte;
    extern const RegisterFaultModel* reg_o_bitflip;

    extern const RegisterFaultModel* reg_t_clear;
    extern const RegisterFaultModel* reg_t_fill;
    extern const RegisterFaultModel* reg_t_setbyte;
    extern const RegisterFaultModel* reg_t_clearbyte;
    extern const RegisterFaultModel* reg_t_bitflip;

    extern const std::vector<const FaultModel*> all_fault_models;
    extern const std::vector<const FaultModel*> non_permanent_fault_models;
    extern const std::vector<const FaultModel*> instruction_fault_models;
    extern const std::vector<const FaultModel*> register_fault_models;
    extern const std::vector<const FaultModel*> non_single_bit_fault_models;
} // namespace fault_models
