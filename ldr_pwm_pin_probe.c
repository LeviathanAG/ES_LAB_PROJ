/* LDR-only probe: cover the sensor to light the board's PWM LED LD10. */
#include "LPC17xx.h"
#include <stdint.h>

#define LDR_ON_DELTA     400u
#define LDR_OFF_DELTA    200u
#define BASELINE_SAMPLES 16u
#define ADC_TIMEOUT      100000u
#define PWM_PERIOD_TICKS 30000u
#define PWM_ON_TICKS     24000u /* 80% duty. */

/* Watch these in Keil: adc_raw should rise when the LDR is covered. */
volatile uint16_t adc_raw;
volatile uint16_t baseline_adc;
volatile uint8_t adc_error;
volatile uint8_t calibrated;
volatile uint8_t ldr_covered;

static void init_adc(void)
{
    /* CND pin 1 is P0.23 / AD0.0. */
    LPC_PINCON->PINSEL1 = (LPC_PINCON->PINSEL1 & ~(3u << 14)) | (1u << 14);
    LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~(3u << 14)) | (2u << 14);
    LPC_SC->PCONP |= (1u << 12);
    LPC_SC->PCLKSEL0 &= ~(3u << 24);
    LPC_ADC->ADCR = (1u << 0) | (3u << 8) | (1u << 21);
}

static uint8_t read_adc(uint16_t *reading)
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

static void init_pwm_led(void)
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
    uint32_t total = 0u;
    uint16_t reading;
    uint8_t samples = 0u;

    SystemInit();
    init_adc();
    init_pwm_led();

    /* Leave the LDR uncovered at reset so this is a room-light baseline. */
    while (samples < BASELINE_SAMPLES) {
        if (read_adc(&reading)) {
            total += reading;
            ++samples;
            adc_error = 0u;
        } else {
            adc_error = 1u;
            total = 0u;
            samples = 0u;
        }
    }

    baseline_adc = (uint16_t)(total / BASELINE_SAMPLES);
    calibrated = 1u;

    for (;;) {
        if (read_adc(&reading)) {
            adc_raw = reading;
            adc_error = 0u;

            if ((ldr_covered == 0u) &&
                ((uint32_t)reading >= (uint32_t)baseline_adc + LDR_ON_DELTA)) {
                ldr_covered = 1u;
                LPC_PWM1->MR4 = PWM_ON_TICKS;
                LPC_PWM1->LER = (1u << 4);
            } else if ((ldr_covered != 0u) &&
                       ((uint32_t)reading <= (uint32_t)baseline_adc + LDR_OFF_DELTA)) {
                ldr_covered = 0u;
                LPC_PWM1->MR4 = 0u;
                LPC_PWM1->LER = (1u << 4);
            }
        } else {
            adc_error = 1u;
            ldr_covered = 0u;
            LPC_PWM1->MR4 = 0u;
            LPC_PWM1->LER = (1u << 4);
        }
    }
}
