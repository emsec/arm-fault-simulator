#include "fault_simulator/fault_printing.h"

#include "fault_simulator/termcolor.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>

namespace fault_simulator
{
    static std::string to_hex(u32 x, u32 padding)
    {
        std::stringstream stream;
        stream << std::hex << std::setw(padding) << std::setfill('0') << x;
        return stream.str();
    }

    static void print_differences(const std::string& a, const std::string& b)
    {
        for (u32 i = 0; i < b.size(); ++i)
        {
            if (i < a.size() && a[i] != b[i])
            {
                std::cout << termcolor::bright_red << b[i] << termcolor::reset;
            }
            else
            {
                std::cout << b[i];
            }
        }
    }

    void print_instruction_fault(const armory::InstructionFault& f, const Configuration& ctx, mulator::Disassembler& disasm)
    {
        const CodeSection* text_segment = nullptr;
        for (const auto& segment : ctx.binary)
        {
            if (segment.offset <= f.address && segment.offset + segment.bytes.size() >= f.address)
            {
                text_segment = &segment;
            }
        }

        if (text_segment == nullptr)
        {
            std::cout << "ERROR: " << std::hex << f.address << " is not within a defined segment of the binary" << std::endl;
            return;
        }

        u8 bytes[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        std::memcpy(bytes, text_segment->bytes.data() + f.address - text_segment->offset, std::min((u32)4, (u32)text_segment->bytes.size() - (f.address - text_segment->offset)));
        std::string original_code;
        std::string original_string;

        {
            disasm.reset();
            auto success = disasm.disassemble(f.address, bytes, f.instr_size);
            auto instr   = disasm.get_instruction();
            if (success == armory::ReturnCode::OK)
            {
                original_code   = disasm.get_string();
                original_string = to_hex(instr.encoding, instr.size * 2);
            }
            else
            {
                original_code   = "<not a valid instruction>";
                original_string = to_hex(instr.encoding >> 16, 4);
            }
        }

        std::memcpy(bytes, f.manipulated_instruction, f.instr_size);
        disasm.reset();
        disasm.disassemble(f.address, bytes, 4);
        auto instr            = disasm.get_instruction();
        auto manipulated_code = disasm.get_string();

        std::string manip_str = to_hex(instr.encoding, instr.size * 2);

        std::cout << std::hex << f.address << ": ";
        std::cout << original_string;
        std::cout << " --> ";
        for (u32 i = 0; i < manip_str.size(); ++i)
        {
            if (i < original_string.size() && original_string[i] != manip_str[i])
            {
                std::cout << termcolor::bright_red << manip_str[i] << termcolor::reset;
            }
            else
            {
                std::cout << manip_str[i];
            }
        }

        std::istringstream stream_a(original_code);
        std::vector<std::string> parts_a(std::istream_iterator<std::string>{stream_a}, std::istream_iterator<std::string>());
        std::istringstream stream_b(manipulated_code);
        std::vector<std::string> parts_b(std::istream_iterator<std::string>{stream_b}, std::istream_iterator<std::string>());

        std::cout << "  [" << original_code << " -->";
        for (u32 i = 0; i < parts_a.size() && i < parts_b.size(); ++i)
        {
            if (parts_a.at(i) != parts_b.at(i))
            {
                std::cout << " " << termcolor::bright_red << parts_b.at(i) << termcolor::reset;
            }
            else
            {
                std::cout << " " << parts_b.at(i);
            }
        }
        for (u32 i = parts_a.size(); i < parts_b.size(); ++i)
        {
            std::cout << " " << termcolor::bright_red << parts_b.at(i) << termcolor::reset;
        }

        std::cout << "]";

        std::cout << " | " << f.model->get_name() << std::dec;

        if (f.model->get_number_of_iterations(f) > 1)
        {
            std::cout << " | position=" << f.fault_model_iteration;
        }

        if (!f.model->is_permanent())
        {
            std::cout << " | time=" << std::dec << f.time;
        }

        std::cout << std::endl;
    }

    void print_register_fault(const armory::RegisterFault& f)
    {
        auto orig_str  = to_hex(f.original_value, 8);
        auto manip_str = to_hex(f.manipulated_value, 8);
        std::cout << f.reg;
        if (!f.model->is_permanent())
        {
            std::cout << ": " << orig_str << " --> ";
            print_differences(orig_str, manip_str);
        }

        std::cout << " | " << f.model->get_name() << std::dec;

        if (f.model->get_number_of_iterations(f) > 1)
        {
            std::cout << " | position=" << f.fault_model_iteration;
        }

        if (!f.model->is_permanent())
        {
            std::cout << " | time=" << std::dec << f.time;
        }

        std::cout << std::endl;
    }
}    // namespace fault_simulator
