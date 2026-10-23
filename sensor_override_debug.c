/*
 * Live sensor diagnostic for a bright lab.
 * A switch from P2.12 to GND forces test_dark; it does not fake PIR activity.
 * Inspect adc_raw, switch_pressed, test_dark, pir_level, and pir_edges.
 */
#include "LPC17xx.h"
#include <stdint.h>

#define ADC_TIMEOUT_COUNT  100000u
#define SAMPLE_INTERVAL_MS 100u
#define DARK_THRESHOLD     2200u /* Provisional; calibrate using adc_raw. */
#define BRIGHT_THRESHOLD   1800u

volatile uint32_t milliseconds;
volatile uint32_t pir_edges;
volatile uint16_t adc_raw;
volatile uint8_t adc_error;
volatile uint8_t pir_level;
volatile uint8_t switch_pressed;
volatile uint8_t ldr_dark;
volatile uint8_t test_dark;

void SysTick_Handler(void)
{
    ++milliseconds;
}

static void adc_init(void)
{
    LPC_PINCON->PINSEL1 = (LPC_PINCON->PINSEL1 & ~(3u << 14)) | (1u << 14);
    LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~(3u << 14)) | (2u << 14);
    LPC_SC->PCONP |= (1u << 12);
    LPC_SC->PCLKSEL0 &= ~(3u << 24);
    LPC_ADC->ADCR = (1u << 0) | (3u << 8) | (1u << 21);
}

static uint8_t adc_read(uint16_t *value)
{
    uint32_t remaining = ADC_TIMEOUT_COUNT;

    LPC_ADC->ADCR &= ~(7u << 24);
    LPC_ADC->ADCR |= (1u << 24);

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

static void sensor_gpio_init(void)
{
    /* P2.0 is PIR input; P2.12 is active-low switch input. */
    LPC_PINCON->PINSEL4 &= ~((3u << 0) | (3u << 24));
    LPC_PINCON->PINMODE4 = (LPC_PINCON->PINMODE4 &
                            ~((3u << 0) | (3u << 24))) |
                           (2u << 0); /* No bias for driven PIR; pull-up on switch. */
    LPC_GPIO2->FIODIR &= ~((1u << 0) | (1u << 12));
}

int main(void)
{
    uint16_t sample = 0u;
    uint32_t last_sample;
    uint8_t previous_pir;

    SystemInit();
    SystemCoreClockUpdate();
    milliseconds = 0u;
    SysTick_Config(SystemCoreClock / 1000u);
    adc_init();
    sensor_gpio_init();

    last_sample = milliseconds - SAMPLE_INTERVAL_MS;
    previous_pir = ((LPC_GPIO2->FIOPIN & (1u << 0)) != 0u) ? 1u : 0u;

    for (;;) {
        uint32_t now = milliseconds;

        pir_level = ((LPC_GPIO2->FIOPIN & (1u << 0)) != 0u) ? 1u : 0u;
        if (pir_level != previous_pir) {
            ++pir_edges;
            previous_pir = pir_level;
        }

        /* The referenced Manipal question uses a switch on P2.12. */
        switch_pressed = ((LPC_GPIO2->FIOPIN & (1u << 12)) == 0u) ? 1u : 0u;

        if ((uint32_t)(now - last_sample) >= SAMPLE_INTERVAL_MS) {
            last_sample = now;

            if (adc_read(&sample)) {
                adc_raw = sample;
                adc_error = 0u;
            } else {
                adc_error = 1u;
            }

            if (!ldr_dark && adc_raw >= DARK_THRESHOLD) {
                ldr_dark = 1u;
            } else if (ldr_dark && adc_raw <= BRIGHT_THRESHOLD) {
                ldr_dark = 0u;
            }
        }

        test_dark = (uint8_t)(ldr_dark || switch_pressed);
    }
}
