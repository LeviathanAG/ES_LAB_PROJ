/* Blink only P2.0 through CND -> CNA1 to check CND pin 7 on the board LEDs. */
#include "LPC17xx.h"
#include <stdint.h>

#define P2_0_PIN    (1u << 0)
#define DELAY_COUNT 10000000u

static void visible_pause(void)
{
    volatile uint32_t count;

    for (count = 0u; count < DELAY_COUNT; ++count) {
        __NOP();
    }
}

int main(void)
{
    SystemInit();

    LPC_SC->PCONP |= (1u << 15); /* Power GPIO. */
    LPC_PINCON->PINSEL4 &= ~3u; /* P2.0 as GPIO. */
    LPC_GPIO2->FIOCLR = P2_0_PIN;
    LPC_GPIO2->FIODIR |= P2_0_PIN;

    for (;;) {
        LPC_GPIO2->FIOSET = P2_0_PIN;
        visible_pause();
        LPC_GPIO2->FIOCLR = P2_0_PIN;
        visible_pause();
    }
}
