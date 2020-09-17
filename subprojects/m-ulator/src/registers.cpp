#include "m-ulator/registers.h"

namespace mulator
{

    std::string to_string(const Register& x)
    {
        if (x == SP)
        {
            return "sp";
        }
        else if (x == LR)
        {
            return "lr";
        }
        else if (x == PC)
        {
            return "pc";
        }
        else if (x == PSR)
        {
            return "xpsr";
        }

        else if (x == 9) // optional
        {
            return "sb";
        }
        else if (x == 10)  // optional
        {
            return "sl";
        }
        else if (x == 11)  // optional
        {
            return "fp";
        }
        else if (x == 12) // optional
        {
            return "ip";
        }

        else if (x >= 0 && x <= 12)
        {
            return "r" + std::to_string(x);
        }

        return "<unknown register " + std::to_string(static_cast<int>(x)) + ">";
    }


    std::ostream& operator<<(std::ostream& os, const Register& x)
    {
        return os << to_string(x);
    }
}
