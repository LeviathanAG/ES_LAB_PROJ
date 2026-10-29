/* Light LD10 at fixed PWM brightness when the LDR is dark OR PIR OUT is HIGH. */
#include "LPC17xx.h"
#include <stdint.h>

#define DARK_THRESHOLD   2200u
#define PWM_PERIOD_TICKS 30000u
#define PWM_ON_TICKS     24000u /* 80% brightness. */
#define ADC_TIMEOUT      100000u

/* Inspect these in Keil Watch to see which input turned the LED on. */
volatile uint16_t adc_raw;
volatile uint8_t adc_error;
volatile uint8_t ldr_dark;
volatile uint8_t pir_high;
volatile uint8_t lamp_on;

static void init_ldr(void)
{
    /* CND pin 1: P0.23 / AD0.0, with no internal pull resistor. */
    LPC_PINCON->PINSEL1 = (LPC_PINCON->PINSEL1 & ~(3u << 14)) | (1u << 14);
    LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~(3u << 14)) | (2u << 14);
    LPC_SC->PCONP |= (1u << 12);
    LPC_SC->PCLKSEL0 &= ~(3u << 24);
    LPC_ADC->ADCR = (1u << 0) | (3u << 8) | (1u << 21);
}

static uint8_t read_ldr(uint16_t *reading)
{
    uint32_t remaining = ADC_TIMEOUT;

    LPC_ADC->ADCR &= ~(7u << 24);
    LPC_ADC->ADCR |= (1u << 24);

    while (remaining > 0u) {
        uint32_t result = LPC_ADC->ADDR0;
        if ((result & (1u << 31)) != 0u) {
            *reading = (uint16_t)((result >> 4) & 0x0FFFu);
            LPC_ADC->ADCR &= ~(7u << 24);
            return 1u;
        }
        --remaining;
    }

    LPC_ADC->ADCR &= ~(7u << 24);
    return 0u;
}

static void init_pir(void)
{
    /* CND pin 7: P2.0 as a digital input. */
    LPC_PINCON->PINSEL4 &= ~3u;
    LPC_PINCON->PINMODE4 = (LPC_PINCON->PINMODE4 & ~3u) | 2u;
    LPC_GPIO2->FIODIR &= ~1u;
}

static void init_ld10(void)
{
    /* CNB pin 1 -> CNB1 -> LD10: P1.23 / PWM1.4. */
    LPC_SC->PCONP |= (1u << 6);
    LPC_SC->PCLKSEL0 = (LPC_SC->PCLKSEL0 & ~(3u << 12)) | (1u << 12);
    LPC_PINCON->PINSEL3 = (LPC_PINCON->PINSEL3 & ~(3u << 14)) | (2u << 14);

    LPC_PWM1->TCR = 2u;
    LPC_PWM1->PR = 0u;
    LPC_PWM1->MR0 = PWM_PERIOD_TICKS;
    LPC_PWM1->MR4 = 0u;
    LPC_PWM1->MCR = (1u << 1);
    LPC_PWM1->PCR = (1u << 12);
    LPC_PWM1->LER = (1u << 0) | (1u << 4);
    LPC_PWM1->TCR = (1u << 0) | (1u << 3);
}

int main(void)
{
    uint16_t reading;

    SystemInit();
    init_ldr();
    init_pir();
    init_ld10();

    for (;;) {
        if (read_ldr(&reading)) {
            adc_raw = reading;
            adc_error = 0u;
            ldr_dark = (reading >= DARK_THRESHOLD) ? 1u : 0u;
        } else {
            adc_error = 1u;
            ldr_dark = 0u;
        }

        pir_high = ((LPC_GPIO2->FIOPIN & 1u) != 0u) ? 1u : 0u;

        if ((ldr_dark != 0u) || (pir_high != 0u)) {
            if (lamp_on == 0u) {
                LPC_PWM1->MR4 = PWM_ON_TICKS;
                LPC_PWM1->LER = (1u << 4);
                lamp_on = 1u;
            }
        } else if (lamp_on != 0u) {
            LPC_PWM1->MR4 = 0u;
            LPC_PWM1->LER = (1u << 4);
            lamp_on = 0u;
        }
    }
}
