#include <stdbool.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

void __attribute__((noinline)) super_secret_function()
{
    while (true)
    {
    }
}

void interesting_to_fault(u8 array[32])
{
    u8 comparison_data[32] = {0x17, 0xad, 0xa6, 0xf7, 0x91, 0xad, 0xab, 0x07, 0xa6, 0x03, 0x45, 0x84, 0x6a, 0x27, 0x5b, 0xf3,
                              0x09, 0xdd, 0xde, 0x18, 0x9f, 0xa9, 0xd4, 0x23, 0x7b, 0x7b, 0x92, 0x2a, 0xe1, 0x4a, 0xef, 0x27};

    bool equal = true;
    for (u32 i = 0; i < 32; ++i)
    {
        if (array[i] != comparison_data[i])
        {
            equal = false;
            break;
        }
    }

    if (equal)
    {
        super_secret_function();
    }
}
