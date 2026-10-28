/* Hold LD10 at 50% PWM through P1.23. No sensors, ADC, or SysTick. */
#include "LPC17xx.h"

#define PWM_PERIOD_TICKS 30000u
#define PWM_DUTY_TICKS   15000u

int main(void)
{
    SystemInit();

    LPC_SC->PCONP |= (1u << 6); /* Power PWM1. */
    LPC_SC->PCLKSEL0 = (LPC_SC->PCLKSEL0 & ~(3u << 12)) | (1u << 12);
    LPC_PINCON->PINSEL3 = (LPC_PINCON->PINSEL3 & ~(3u << 14)) |
                           (2u << 14); /* P1.23 = PWM1.4. */

    LPC_PWM1->TCR = 2u; /* Reset the PWM counter. */
    LPC_PWM1->PR = 0u;
    LPC_PWM1->MR0 = PWM_PERIOD_TICKS;
    LPC_PWM1->MR4 = PWM_DUTY_TICKS;
    LPC_PWM1->MCR = (1u << 1); /* Reset counter at MR0. */
    LPC_PWM1->PCR = (1u << 12); /* Enable PWM1.4 output. */
    LPC_PWM1->LER = (1u << 0) | (1u << 4);
    LPC_PWM1->TCR = (1u << 0) | (1u << 3); /* Run counter and PWM. */

    for (;;) {
        /* PWM hardware keeps LD10 at 50% without any software timing loop. */
    }
}
