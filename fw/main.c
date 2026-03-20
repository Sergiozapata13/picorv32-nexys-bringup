#include <stdint.h>

#define LEDS (*(volatile uint32_t*)0x10000000)

static void delay(volatile uint32_t n)
{
    while (n--) {
        asm volatile ("nop");
    }
}

int main()
{
    uint32_t v = 1;

    while (1) {
        LEDS = v;
        v <<= 1;

        if (v == 0x00010000)
            v = 1;

        delay(2000000);
    }
}
