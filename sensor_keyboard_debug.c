/*
 * Live sensor diagnostic with a matrix-key override for a bright lab.
 * Manipal ES manual: rows P2.10..P2.13, columns P1.23..P1.26.
 * Any detected key sets test_dark; PIR must still assert P2.0.
 */
#include "LPC17xx.h"
#include <stdint.h>

#define ADC_TIMEOUT_COUNT  100000u
#define SAMPLE_INTERVAL_MS 100u
#define DARK_THRESHOLD     2200u /* Provisional; calibrate using adc_raw. */
#define BRIGHT_THRESHOLD   1800u

#define PIR_PIN            (1u << 0)
#define KEYPAD_ROW_MASK    (0x0Fu << 10)
#define KEYPAD_COLUMN_MASK (0x0Fu << 23)

volatile uint32_t milliseconds;
volatile uint32_t pir_edges;
volatile uint16_t adc_raw;
volatile uint8_t adc_error;
volatile uint8_t pir_level;
volatile uint8_t key_pressed;
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

static void gpio_init(void)
{
    /* P2.0 PIR GPIO input, P2.10..13 keypad row outputs. */
    LPC_PINCON->PINSEL4 &= ~((3u << 0) | (0xFFu << 20));
    LPC_PINCON->PINMODE4 = (LPC_PINCON->PINMODE4 &
                            ~((3u << 0) | (0xFFu << 20))) |
                           (2u << 0); /* No bias on driven PIR input. */

    /* P1.23..26 are GPIO inputs; use pull-downs for idle keypad columns. */
    LPC_PINCON->PINSEL3 &= ~(0xFFu << 14);
    LPC_PINCON->PINMODE3 = (LPC_PINCON->PINMODE3 & ~(0xFFu << 14)) |
                           (0xFFu << 14);

    LPC_GPIO2->FIODIR = (LPC_GPIO2->FIODIR & ~PIR_PIN) | KEYPAD_ROW_MASK;
    LPC_GPIO2->FIOCLR = KEYPAD_ROW_MASK;
    LPC_GPIO1->FIODIR &= ~KEYPAD_COLUMN_MASK;
}

static uint8_t scan_keypad(void)
{
    uint32_t row;
    uint32_t column;

    for (row = 0u; row < 4u; ++row) {
        LPC_GPIO2->FIOCLR = KEYPAD_ROW_MASK;
        LPC_GPIO2->FIOSET = (1u << (10u + row));

        for (column = 0u; column < 4u; ++column) {
            if ((LPC_GPIO1->FIOPIN & (1u << (23u + column))) != 0u) {
                return 1u;
            }
        }
    }

    LPC_GPIO2->FIOCLR = KEYPAD_ROW_MASK;
    return 0u;
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
    gpio_init();

    last_sample = milliseconds - SAMPLE_INTERVAL_MS;
    previous_pir = ((LPC_GPIO2->FIOPIN & PIR_PIN) != 0u) ? 1u : 0u;

    for (;;) {
        uint32_t now = milliseconds;

        pir_level = ((LPC_GPIO2->FIOPIN & PIR_PIN) != 0u) ? 1u : 0u;
        if (pir_level != previous_pir) {
            ++pir_edges;
            previous_pir = pir_level;
        }

        key_pressed = scan_keypad();

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

        test_dark = (uint8_t)(ldr_dark || key_pressed);
    }
}
