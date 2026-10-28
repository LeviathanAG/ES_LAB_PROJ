/* Blink LD10 through P1.23 as GPIO. No sensors, ADC, SysTick, or PWM. */
#include "LPC17xx.h"
#include <stdint.h>

#define LD10_PIN    (1u << 23)
#define DELAY_COUNT 10000000u /* Approximate; only the visible blink matters. */

static void pause_between_changes(void)
{
    volatile uint32_t count;

    for (count = 0u; count < DELAY_COUNT; ++count) {
        __NOP();
    }
}

int main(void)
{
    SystemInit();

    /* P1.23 function 00 is GPIO. */
    LPC_SC->PCONP |= (1u << 15);
    LPC_PINCON->PINSEL3 &= ~(3u << 14);
    LPC_GPIO1->FIODIR |= LD10_PIN;

    for (;;) {
        LPC_GPIO1->FIOSET = LD10_PIN;
        pause_between_changes();
        LPC_GPIO1->FIOCLR = LD10_PIN;
        pause_between_changes();
    }
}
