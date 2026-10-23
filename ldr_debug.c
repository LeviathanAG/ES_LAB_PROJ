/* Standalone AD0.0 probe. Inspect adc_raw and adc_error in the debugger. */
#include "LPC17xx.h"
#include <stdint.h>

#define ADC_TIMEOUT_COUNT  100000u
#define SAMPLE_INTERVAL_MS 100u

volatile uint32_t milliseconds;
volatile uint16_t adc_raw;
volatile uint8_t adc_error;

void SysTick_Handler(void)
{
    ++milliseconds;
}

static void adc_init(void)
{
    /* P0.23 function 01 is AD0.0; disable internal pull on analog input. */
    LPC_PINCON->PINSEL1 = (LPC_PINCON->PINSEL1 & ~(3u << 14)) | (1u << 14);
    LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~(3u << 14)) | (2u << 14);

    LPC_SC->PCONP |= (1u << 12);
    LPC_SC->PCLKSEL0 &= ~(3u << 24);   /* ADC PCLK = CCLK / 4. */
    LPC_ADC->ADCR = (1u << 0) | (3u << 8) | (1u << 21); /* AD0.0, /4, on. */
}

static uint8_t adc_read(uint16_t *value)
{
    uint32_t remaining = ADC_TIMEOUT_COUNT;

    LPC_ADC->ADCR &= ~(7u << 24);
    LPC_ADC->ADCR |= (1u << 24);       /* Start software conversion. */

    while (remaining > 0u) {
        uint32_t result = LPC_ADC->ADDR0;

        if ((result & (1u << 31)) != 0u) {
            *value = (uint16_t)((result >> 4) & 0x0FFFu);
            LPC_ADC->ADCR &= ~(7u << 24);
            return 1u;
        }
        --remaining;
    }

    LPC_ADC->ADCR &= ~(7u << 24);
    return 0u;
}

int main(void)
{
    uint16_t sample;
    uint32_t last_sample;

    SystemInit();
    SystemCoreClockUpdate();
    milliseconds = 0u;
    SysTick_Config(SystemCoreClock / 1000u);
    adc_init();

    last_sample = milliseconds - SAMPLE_INTERVAL_MS;

    for (;;) {
        uint32_t now = milliseconds;

        if ((uint32_t)(now - last_sample) >= SAMPLE_INTERVAL_MS) {
            last_sample = now;

            if (adc_read(&sample)) {
                adc_raw = sample;
                adc_error = 0u;
            } else {
                adc_error = 1u;
            }
        }
    }
}
