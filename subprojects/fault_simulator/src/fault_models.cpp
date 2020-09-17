#include "fault_simulator/fault_models.h"

#include <cstring>

namespace fault_models
{
    using namespace armory;
    namespace instruction_faults
    {

        u32 return_1(const armory::InstructionFault&)
        {
            return 1;
        }

        u32 count_bytes(const armory::InstructionFault& f)
        {
            return f.instr_size;
        }

        u32 count_bits(const armory::InstructionFault& f)
        {
            return f.instr_size * 8;
        }

        bool return_true(const armory::InstructionFault&)
        {
            return true;
        }

        ///////////////////////////////////////////

        void instruction_skip(armory::InstructionFault& f)
        {
            if (f.instr_size == 4)
            {
                // 0x‭F3AF8000‬ is a 32-bit thumb nop
                f.manipulated_instruction[0] = 0xaf;
                f.manipulated_instruction[1] = 0xf3;
                f.manipulated_instruction[2] = 0x00;
                f.manipulated_instruction[3] = 0x80;
            }
            else if (f.instr_size == 2)
            {
                // 0xbf00 is a thumb nop
                f.manipulated_instruction[0] = 0x00;
                f.manipulated_instruction[1] = 0xbf;
            }
        }

        void byte_set(armory::InstructionFault& f)
        {
            std::memcpy(f.manipulated_instruction, f.original_instruction, f.instr_size);
            f.manipulated_instruction[f.fault_model_iteration] = 0xFF;
        }

        void byte_clear(armory::InstructionFault& f)
        {
            std::memcpy(f.manipulated_instruction, f.original_instruction, f.instr_size);
            f.manipulated_instruction[f.fault_model_iteration] = 0x00;
        }

        void bit_flip(armory::InstructionFault& f)
        {
            std::memcpy(f.manipulated_instruction, f.original_instruction, f.instr_size);
            f.manipulated_instruction[f.fault_model_iteration / 8] ^= 1 << (f.fault_model_iteration % 8);
        }
    } // namespace instruction_faults

    namespace register_faults
    {

        u32 return_1(const armory::RegisterFault&)
        {
            return 1;
        }

        u32 return_4(const armory::RegisterFault&)
        {
            return 4;
        }

        u32 return_32(const armory::RegisterFault&)
        {
            return 32;
        }

        bool register_tester(const armory::RegisterFault& f)
        {
            return f.reg != mulator::Register::SP && f.reg != mulator::Register::PC;
        }

        ///////////////////////////////////////////

        void clear_register(armory::RegisterFault& f)
        {
            f.manipulated_value = 0;
        }

        void fill_register(armory::RegisterFault& f)
        {
            f.manipulated_value = 0xFFFFFFFF;
        }

        void byte_set(armory::RegisterFault& f)
        {
            f.manipulated_value = f.original_value | ((u32)0xFF << (8 * f.fault_model_iteration));
        }

        void byte_clear(armory::RegisterFault& f)
        {
            f.manipulated_value = f.original_value & ~((u32)0xFF << (8 * f.fault_model_iteration));
        }

        void bit_set(armory::RegisterFault& f)
        {
            f.manipulated_value = f.original_value | ((u32)1 << f.fault_model_iteration);
        }
        void bit_clear(armory::RegisterFault& f)
        {
            f.manipulated_value = f.original_value & ~((u32)1 << f.fault_model_iteration);
        }
        void bit_flip(armory::RegisterFault& f)
        {
            f.manipulated_value = f.original_value ^ ((u32)1 << f.fault_model_iteration);
        }
    } // namespace register_faults

    const std::unique_ptr<InstructionFaultModel> instr_p_skip_owner = std::make_unique<InstructionFaultModel>("Permanent Instruction Skip", InstructionFaultModel::FaultType::PERMANENT, instruction_faults::return_1, instruction_faults::return_true, instruction_faults::instruction_skip);
    const InstructionFaultModel* instr_p_skip = instr_p_skip_owner.get();
    const std::unique_ptr<InstructionFaultModel> instr_p_setbyte_owner = std::make_unique<InstructionFaultModel>("Permanent Instruction Byte-Set", InstructionFaultModel::FaultType::PERMANENT, instruction_faults::count_bytes, instruction_faults::return_true, instruction_faults::byte_set);
    const InstructionFaultModel* instr_p_setbyte = instr_p_setbyte_owner.get();
    const std::unique_ptr<InstructionFaultModel> instr_p_clearbyte_owner = std::make_unique<InstructionFaultModel>("Permanent Instruction Byte-Clear", InstructionFaultModel::FaultType::PERMANENT, instruction_faults::count_bytes, instruction_faults::return_true, instruction_faults::byte_clear);
    const InstructionFaultModel* instr_p_clearbyte = instr_p_clearbyte_owner.get();
    const std::unique_ptr<InstructionFaultModel> instr_p_bitflip_owner = std::make_unique<InstructionFaultModel>("Permanent Instruction Bit-Flip", InstructionFaultModel::FaultType::PERMANENT, instruction_faults::count_bits, instruction_faults::return_true, instruction_faults::bit_flip);
    const InstructionFaultModel* instr_p_bitflip = instr_p_bitflip_owner.get();

    const std::unique_ptr<InstructionFaultModel> instr_t_skip_owner = std::make_unique<InstructionFaultModel>("Transient Instruction Skip", InstructionFaultModel::FaultType::TRANSIENT, instruction_faults::return_1, instruction_faults::return_true, instruction_faults::instruction_skip);
    const InstructionFaultModel* instr_t_skip = instr_t_skip_owner.get();
    const std::unique_ptr<InstructionFaultModel> instr_t_setbyte_owner = std::make_unique<InstructionFaultModel>("Transient Instruction Byte-Set", InstructionFaultModel::FaultType::TRANSIENT, instruction_faults::count_bytes, instruction_faults::return_true, instruction_faults::byte_set);
    const InstructionFaultModel* instr_t_setbyte = instr_t_setbyte_owner.get();
    const std::unique_ptr<InstructionFaultModel> instr_t_clearbyte_owner = std::make_unique<InstructionFaultModel>("Transient Instruction Byte-Clear", InstructionFaultModel::FaultType::TRANSIENT, instruction_faults::count_bytes, instruction_faults::return_true, instruction_faults::byte_clear);
    const InstructionFaultModel* instr_t_clearbyte = instr_t_clearbyte_owner.get();
    const std::unique_ptr<InstructionFaultModel> instr_t_bitflip_owner = std::make_unique<InstructionFaultModel>("Transient Instruction Bit-Flip", InstructionFaultModel::FaultType::TRANSIENT, instruction_faults::count_bits, instruction_faults::return_true, instruction_faults::bit_flip);
    const InstructionFaultModel* instr_t_bitflip = instr_t_bitflip_owner.get();

    const std::unique_ptr<RegisterFaultModel> reg_p_clear_owner = std::make_unique<RegisterFaultModel>("Permanent Register Clear", RegisterFaultModel::FaultType::PERMANENT, register_faults::return_1, register_faults::register_tester, register_faults::clear_register);
    const RegisterFaultModel* reg_p_clear = reg_p_clear_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_p_fill_owner = std::make_unique<RegisterFaultModel>("Permanent Register Fill", RegisterFaultModel::FaultType::PERMANENT, register_faults::return_1, register_faults::register_tester, register_faults::fill_register);
    const RegisterFaultModel* reg_p_fill = reg_p_fill_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_p_setbyte_owner = std::make_unique<RegisterFaultModel>("Permanent Register Byte-Set", RegisterFaultModel::FaultType::PERMANENT, register_faults::return_4, register_faults::register_tester, register_faults::byte_set);
    const RegisterFaultModel* reg_p_setbyte = reg_p_setbyte_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_p_clearbyte_owner = std::make_unique<RegisterFaultModel>("Permanent Register Byte-Clear", RegisterFaultModel::FaultType::PERMANENT, register_faults::return_4, register_faults::register_tester, register_faults::byte_clear);
    const RegisterFaultModel* reg_p_clearbyte = reg_p_clearbyte_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_p_bitset_owner = std::make_unique<RegisterFaultModel>("Permanent Register Bit-Set", RegisterFaultModel::FaultType::PERMANENT, register_faults::return_32, register_faults::register_tester, register_faults::bit_set);
    const RegisterFaultModel* reg_p_bitset = reg_p_bitset_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_p_bitclear_owner = std::make_unique<RegisterFaultModel>("Permanent Register Bit-Clear", RegisterFaultModel::FaultType::PERMANENT, register_faults::return_32, register_faults::register_tester, register_faults::bit_clear);
    const RegisterFaultModel* reg_p_bitclear = reg_p_bitclear_owner.get();

    const std::unique_ptr<RegisterFaultModel> reg_o_clear_owner = std::make_unique<RegisterFaultModel>("Until-Overwrite Register Clear", RegisterFaultModel::FaultType::UNTIL_OVERWRITE, register_faults::return_1, register_faults::register_tester, register_faults::clear_register);
    const RegisterFaultModel* reg_o_clear = reg_o_clear_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_o_fill_owner = std::make_unique<RegisterFaultModel>("Until-Overwrite Register Fill", RegisterFaultModel::FaultType::UNTIL_OVERWRITE, register_faults::return_1, register_faults::register_tester, register_faults::fill_register);
    const RegisterFaultModel* reg_o_fill = reg_o_fill_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_o_setbyte_owner = std::make_unique<RegisterFaultModel>("Until-Overwrite Register Byte-Set", RegisterFaultModel::FaultType::UNTIL_OVERWRITE, register_faults::return_4, register_faults::register_tester, register_faults::byte_set);
    const RegisterFaultModel* reg_o_setbyte = reg_o_setbyte_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_o_clearbyte_owner = std::make_unique<RegisterFaultModel>("Until-Overwrite Register Byte-Clear", RegisterFaultModel::FaultType::UNTIL_OVERWRITE, register_faults::return_4, register_faults::register_tester, register_faults::byte_clear);
    const RegisterFaultModel* reg_o_clearbyte = reg_o_clearbyte_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_o_bitflip_owner = std::make_unique<RegisterFaultModel>("Until-Overwrite Register Bit-Flip", RegisterFaultModel::FaultType::UNTIL_OVERWRITE, register_faults::return_32, register_faults::register_tester, register_faults::bit_flip);
    const RegisterFaultModel* reg_o_bitflip = reg_o_bitflip_owner.get();

    const std::unique_ptr<RegisterFaultModel> reg_t_clear_owner = std::make_unique<RegisterFaultModel>("Transient Register Clear", RegisterFaultModel::FaultType::TRANSIENT, register_faults::return_1, register_faults::register_tester, register_faults::clear_register);
    const RegisterFaultModel* reg_t_clear = reg_t_clear_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_t_fill_owner = std::make_unique<RegisterFaultModel>("Transient Register Fill", RegisterFaultModel::FaultType::TRANSIENT, register_faults::return_1, register_faults::register_tester, register_faults::fill_register);
    const RegisterFaultModel* reg_t_fill = reg_t_fill_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_t_setbyte_owner = std::make_unique<RegisterFaultModel>("Transient Register Byte-Set", RegisterFaultModel::FaultType::TRANSIENT, register_faults::return_4, register_faults::register_tester, register_faults::byte_set);
    const RegisterFaultModel* reg_t_setbyte = reg_t_setbyte_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_t_clearbyte_owner = std::make_unique<RegisterFaultModel>("Transient Register Byte-Clear", RegisterFaultModel::FaultType::TRANSIENT, register_faults::return_4, register_faults::register_tester, register_faults::byte_clear);
    const RegisterFaultModel* reg_t_clearbyte = reg_t_clearbyte_owner.get();
    const std::unique_ptr<RegisterFaultModel> reg_t_bitflip_owner = std::make_unique<RegisterFaultModel>("Transient Register Bit-Flip", RegisterFaultModel::FaultType::TRANSIENT, register_faults::return_32, register_faults::register_tester, register_faults::bit_flip);
    const RegisterFaultModel* reg_t_bitflip = reg_t_bitflip_owner.get();

    const std::vector<const FaultModel*> all_fault_models = {instr_p_skip, instr_p_setbyte, instr_p_clearbyte, instr_p_bitflip, instr_t_skip, instr_t_setbyte, instr_t_clearbyte, instr_t_bitflip, reg_p_clear, reg_p_fill, reg_p_setbyte, reg_p_clearbyte, reg_p_bitset, reg_p_bitclear, reg_o_clear, reg_o_fill, reg_o_setbyte, reg_o_clearbyte, reg_o_bitflip, reg_t_clear, reg_t_fill, reg_t_setbyte, reg_t_clearbyte, reg_t_bitflip};
    const std::vector<const FaultModel*> non_permanent_fault_models = {instr_t_skip, instr_t_setbyte, instr_t_clearbyte, instr_t_bitflip, reg_o_clear, reg_o_fill, reg_o_setbyte, reg_o_clearbyte, reg_o_bitflip, reg_t_clear, reg_t_fill, reg_t_setbyte, reg_t_clearbyte, reg_t_bitflip};
    const std::vector<const FaultModel*> instruction_fault_models = {instr_p_skip, instr_p_setbyte, instr_p_clearbyte, instr_p_bitflip, instr_t_skip, instr_t_setbyte, instr_t_clearbyte, instr_t_bitflip};
    const std::vector<const FaultModel*> register_fault_models = {reg_p_clear, reg_p_fill, reg_p_setbyte, reg_p_clearbyte, reg_p_bitset, reg_p_bitclear, reg_o_clear, reg_o_fill, reg_o_setbyte, reg_o_clearbyte, reg_o_bitflip, reg_t_clear, reg_t_fill, reg_t_setbyte, reg_t_clearbyte, reg_t_bitflip};
    const std::vector<const FaultModel*> non_single_bit_fault_models = {instr_p_skip, instr_p_setbyte, instr_p_clearbyte, instr_t_skip, instr_t_setbyte, instr_t_clearbyte, reg_p_clear, reg_p_fill, reg_p_setbyte, reg_p_clearbyte, reg_o_clear, reg_o_fill, reg_o_setbyte, reg_o_clearbyte, reg_t_clear, reg_t_fill, reg_t_setbyte, reg_t_clearbyte};

} // namespace fault_models
