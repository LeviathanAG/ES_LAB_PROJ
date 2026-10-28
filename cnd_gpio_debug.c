/* Blink every CND signal pin as GPIO; leave CND pins 9 and 10 untouched. */
#include "LPC17xx.h"
#include <stdint.h>

#define CND_PORT0_PINS (0x3Fu << 23) /* P0.23 through P0.28: CND 1-6. */
#define CND_PORT2_PINS (0x03u << 0)  /* P2.0 and P2.1: CND 7-8. */
#define DELAY_COUNT    10000000u

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
    LPC_PINCON->PINSEL1 &= ~(0xFFFu << 14); /* P0.23-28 as GPIO. */
    LPC_PINCON->PINSEL4 &= ~0xFu;          /* P2.0-1 as GPIO. */
    LPC_GPIO0->FIOCLR = CND_PORT0_PINS;
    LPC_GPIO2->FIOCLR = CND_PORT2_PINS;
    LPC_GPIO0->FIODIR |= CND_PORT0_PINS;
    LPC_GPIO2->FIODIR |= CND_PORT2_PINS;

    for (;;) {
        LPC_GPIO0->FIOSET = CND_PORT0_PINS;
        LPC_GPIO2->FIOSET = CND_PORT2_PINS;
        visible_pause();

        LPC_GPIO0->FIOCLR = CND_PORT0_PINS;
        LPC_GPIO2->FIOCLR = CND_PORT2_PINS;
        visible_pause();
    }
}
