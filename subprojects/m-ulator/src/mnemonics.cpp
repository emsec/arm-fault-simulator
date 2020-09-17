#include "m-ulator/mnemonics.h"

namespace mulator
{

    std::string to_string(const Mnemonic& x)
    {
        switch (x)
        {
            case Mnemonic::ADC :   return "adc";
            case Mnemonic::ADD :   return "add";
            case Mnemonic::ADR :   return "adr";
            case Mnemonic::AND :   return "and";
            case Mnemonic::ASR :   return "asr";
            case Mnemonic::B :     return "b";
            case Mnemonic::BIC :   return "bic";
            case Mnemonic::BKPT :  return "bkpt";
            case Mnemonic::BL :    return "bl";
            case Mnemonic::BLX :   return "blx";
            case Mnemonic::BX :    return "bx";
            case Mnemonic::CMN :   return "cmn";
            case Mnemonic::CMP :   return "cmp";
            case Mnemonic::CPS :   return "cps";
            case Mnemonic::DMB :   return "dmb";
            case Mnemonic::DSB :   return "dsb";
            case Mnemonic::EOR :   return "eor";
            case Mnemonic::ISB :   return "isb";
            case Mnemonic::LDM :   return "ldm";
            case Mnemonic::LDR :   return "ldr";
            case Mnemonic::LDRB :  return "ldrb";
            case Mnemonic::LDRH :  return "ldrh";
            case Mnemonic::LDRSB : return "ldrsb";
            case Mnemonic::LDRSH : return "ldrsh";
            case Mnemonic::LSL :   return "lsl";
            case Mnemonic::LSR :   return "lsr";
            case Mnemonic::MOV :   return "mov";
            case Mnemonic::MRS :   return "mrs";
            case Mnemonic::MSR :   return "msr";
            case Mnemonic::MUL :   return "mul";
            case Mnemonic::MVN :   return "mvn";
            case Mnemonic::NOP :   return "nop";
            case Mnemonic::ORR :   return "orr";
            case Mnemonic::POP :   return "pop";
            case Mnemonic::PUSH :  return "push";
            case Mnemonic::REV :   return "rev";
            case Mnemonic::REV16 : return "rev16";
            case Mnemonic::REVSH : return "revsh";
            case Mnemonic::ROR :   return "ror";
            case Mnemonic::RSB :   return "rsb";
            case Mnemonic::SBC :   return "sbc";
            case Mnemonic::SEV :   return "sev";
            case Mnemonic::STM :   return "stm";
            case Mnemonic::STR :   return "str";
            case Mnemonic::STRB :  return "strb";
            case Mnemonic::STRH :  return "strh";
            case Mnemonic::SUB :   return "sub";
            case Mnemonic::SVC :   return "svc";
            case Mnemonic::SXTB :  return "sxtb";
            case Mnemonic::SXTH :  return "sxth";
            case Mnemonic::TST :   return "tst";
            case Mnemonic::UDF :   return "udf";
            case Mnemonic::UXTB :  return "uxtb";
            case Mnemonic::UXTH :  return "uxth";
            case Mnemonic::WFE :   return "wfe";
            case Mnemonic::WFI :   return "wfi";
            case Mnemonic::YIELD : return "yield";

            // armv7m only
            case Mnemonic::ADDW : return "addw";
            case Mnemonic::BFC : return "bfc";
            case Mnemonic::BFI : return "bfi";
            case Mnemonic::CBZ : return "cbz";
            case Mnemonic::CBNZ : return "cbnz";
            case Mnemonic::CLREX : return "clrex";
            case Mnemonic::CLZ : return "clz";
            case Mnemonic::CSDB : return "csdb";
            case Mnemonic::DBG : return "dbg";
            case Mnemonic::IT : return "it";
            case Mnemonic::LDMDB : return "ldmdb";
            case Mnemonic::LDRBT : return "ldrbt";
            case Mnemonic::LDRD : return "ldrd";
            case Mnemonic::LDREX : return "ldrex";
            case Mnemonic::LDREXB : return "ldrexb";
            case Mnemonic::LDREXH : return "ldrexh";
            case Mnemonic::LDRHT : return "ldrht";
            case Mnemonic::LDRSBT : return "ldrsbt";
            case Mnemonic::LDRSHT : return "ldrsht";
            case Mnemonic::LDRT : return "ldrt";
            case Mnemonic::MLA : return "mla";
            case Mnemonic::MLS : return "mls";
            case Mnemonic::MOVT : return "movt";
            case Mnemonic::MOVW : return "movw";
            case Mnemonic::ORN : return "orn";
            case Mnemonic::PLD : return "pld";
            case Mnemonic::PLI : return "pli";
            case Mnemonic::PSSBB : return "pssbb";
            case Mnemonic::RBIT : return "rbit";
            case Mnemonic::RRX : return "rrx";
            case Mnemonic::SBFX : return "sbfx";
            case Mnemonic::SDIV : return "sdiv";
            case Mnemonic::SMLAL : return "smlal";
            case Mnemonic::SMULL : return "smull";
            case Mnemonic::SSAT : return "ssat";
            case Mnemonic::SSBB : return "ssbb";
            case Mnemonic::STMDB : return "stmdb";
            case Mnemonic::STRBT : return "strbt";
            case Mnemonic::STRD : return "strd";
            case Mnemonic::STREX : return "strex";
            case Mnemonic::STREXB : return "strexb";
            case Mnemonic::STREXH : return "strexh";
            case Mnemonic::STRHT : return "strht";
            case Mnemonic::STRT : return "strt";
            case Mnemonic::SUBW : return "subw";
            case Mnemonic::TBB : return "tbb";
            case Mnemonic::TBH : return "tbh";
            case Mnemonic::TEQ : return "teq";
            case Mnemonic::UBFX : return "ubfx";
            case Mnemonic::UDIV : return "udiv";
            case Mnemonic::UMLAL : return "umlal";
            case Mnemonic::UMULL : return "umull";
            case Mnemonic::USAT : return "usat";
        }

        return "<unknown mnemonic " + std::to_string(static_cast<int>(x)) + ">";
    }

    std::ostream& operator<<(std::ostream& os, const Mnemonic& x)
    {
        return os << to_string(x);
    }

    bool has_narrow_encoding(const Mnemonic& x)
    {
        switch (x)
        {
            case Mnemonic::ADC:
            case Mnemonic::ADD:
            case Mnemonic::ADR:
            case Mnemonic::AND:
            case Mnemonic::ASR:
            case Mnemonic::B:
            case Mnemonic::BIC:
            case Mnemonic::BKPT:
            case Mnemonic::BLX:
            case Mnemonic::BX:
            case Mnemonic::CMN:
            case Mnemonic::CMP:
            case Mnemonic::CPS:
            case Mnemonic::EOR:
            case Mnemonic::LDM:
            case Mnemonic::LDR:
            case Mnemonic::LDRB:
            case Mnemonic::LDRH:
            case Mnemonic::LDRSB:
            case Mnemonic::LDRSH:
            case Mnemonic::LSL:
            case Mnemonic::LSR:
            case Mnemonic::MOV:
            case Mnemonic::MUL:
            case Mnemonic::MVN:
            case Mnemonic::NOP:
            case Mnemonic::ORR:
            case Mnemonic::POP:
            case Mnemonic::PUSH:
            case Mnemonic::REV16:
            case Mnemonic::REV:
            case Mnemonic::REVSH:
            case Mnemonic::ROR:
            case Mnemonic::RSB:
            case Mnemonic::SBC:
            case Mnemonic::SEV:
            case Mnemonic::STM:
            case Mnemonic::STR:
            case Mnemonic::STRB:
            case Mnemonic::STRH:
            case Mnemonic::SUB:
            case Mnemonic::SVC:
            case Mnemonic::SXTB:
            case Mnemonic::TST:
            case Mnemonic::UDF:
            case Mnemonic::UXTB:
            case Mnemonic::UXTH:
            case Mnemonic::WFE:
            case Mnemonic::WFI:
            case Mnemonic::YIELD: return true;
            default: return false;
        }
    }



    bool has_wide_encoding(const Mnemonic& x)
    {
        switch (x)
        {
            case Mnemonic::ADC:
            case Mnemonic::ADD:
            case Mnemonic::ADDW:
            case Mnemonic::ADR:
            case Mnemonic::AND:
            case Mnemonic::ASR:
            case Mnemonic::B:
            case Mnemonic::BFC:
            case Mnemonic::BFI:
            case Mnemonic::BIC:
            case Mnemonic::BL:
            case Mnemonic::CLREX:
            case Mnemonic::CLZ:
            case Mnemonic::CMN:
            case Mnemonic::CMP:
            case Mnemonic::CSDB:
            case Mnemonic::DBG:
            case Mnemonic::DMB:
            case Mnemonic::DSB:
            case Mnemonic::EOR:
            case Mnemonic::ISB:
            case Mnemonic::LDM:
            case Mnemonic::LDMDB:
            case Mnemonic::LDR:
            case Mnemonic::LDRB:
            case Mnemonic::LDRBT:
            case Mnemonic::LDRD:
            case Mnemonic::LDREX:
            case Mnemonic::LDREXB:
            case Mnemonic::LDREXH:
            case Mnemonic::LDRH:
            case Mnemonic::LDRHT:
            case Mnemonic::LDRSB:
            case Mnemonic::LDRSBT:
            case Mnemonic::LDRSH:
            case Mnemonic::LDRSHT:
            case Mnemonic::LDRT:
            case Mnemonic::LSL:
            case Mnemonic::LSR:
            case Mnemonic::MLA:
            case Mnemonic::MLS:
            case Mnemonic::MOV:
            case Mnemonic::MOVT:
            case Mnemonic::MOVW:
            case Mnemonic::MRS:
            case Mnemonic::MSR:
            case Mnemonic::MUL:
            case Mnemonic::MVN:
            case Mnemonic::NOP:
            case Mnemonic::ORN:
            case Mnemonic::ORR:
            case Mnemonic::PLD:
            case Mnemonic::PLI:
            case Mnemonic::POP:
            case Mnemonic::PSSBB:
            case Mnemonic::PUSH:
            case Mnemonic::RBIT:
            case Mnemonic::REV16:
            case Mnemonic::REV:
            case Mnemonic::REVSH:
            case Mnemonic::ROR:
            case Mnemonic::RRX:
            case Mnemonic::RSB:
            case Mnemonic::SBC:
            case Mnemonic::SBFX:
            case Mnemonic::SDIV:
            case Mnemonic::SEV:
            case Mnemonic::SMLAL:
            case Mnemonic::SMULL:
            case Mnemonic::SSAT:
            case Mnemonic::SSBB:
            case Mnemonic::STM:
            case Mnemonic::STMDB:
            case Mnemonic::STR:
            case Mnemonic::STRB:
            case Mnemonic::STRBT:
            case Mnemonic::STRD:
            case Mnemonic::STREX:
            case Mnemonic::STREXB:
            case Mnemonic::STREXH:
            case Mnemonic::STRH:
            case Mnemonic::STRHT:
            case Mnemonic::STRT:
            case Mnemonic::SUB:
            case Mnemonic::SUBW:
            case Mnemonic::SXTB:
            case Mnemonic::SXTH:
            case Mnemonic::TEQ:
            case Mnemonic::TST:
            case Mnemonic::UBFX:
            case Mnemonic::UDF:
            case Mnemonic::UDIV:
            case Mnemonic::UMLAL:
            case Mnemonic::UMULL:
            case Mnemonic::USAT:
            case Mnemonic::UXTB:
            case Mnemonic::UXTH:
            case Mnemonic::WFE:
            case Mnemonic::WFI:
            case Mnemonic::YIELD: return true;
            default: return false;
        }
    }
}
