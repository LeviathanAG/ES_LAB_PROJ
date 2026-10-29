/*
 * Motion-activated corridor lamp for the LPC1768 trainer.
 * Build this file as the application's only source containing main().
 */
#include "LPC17xx.h"
#include <stdint.h>

/* Project settings. ADC thresholds are provisional and must be calibrated. */
#define ADC_SAMPLE_INTERVAL_MS  100u
#define PIR_WARMUP_MS           60000u
#define LAMP_HOLD_MS            10000u
#define FADE_OUT_MS             1000u
#define DARK_THRESHOLD          2200u
#define BRIGHT_THRESHOLD        1800u
#define PWM_FREQUENCY_HZ        2000u
#define PWM_MIN_DUTY_PERCENT    12u
#define PWM_MAX_DUTY_PERCENT    90u
#define ADC_CONVERSION_TIMEOUT  100000u

typedef enum {
    LAMP_OFF = 0,
    LAMP_MOTION,
    LAMP_HOLD,
    LAMP_FADE
} lamp_state_t;

/* Volatile values are convenient to watch in the Keil debugger. */
volatile uint32_t milliseconds;
volatile uint16_t adc_raw;
volatile uint8_t adc_error;
volatile uint8_t pir_level;
volatile uint8_t is_dark;
volatile uint8_t warmup_complete;
volatile uint8_t target_brightness;
volatile uint8_t current_duty;
volatile lamp_state_t lamp_state;

void SysTick_Handler(void)
{
    ++milliseconds;
}

static uint32_t get_milliseconds(void)
{
    return milliseconds;
}

/* Unsigned subtraction keeps elapsed-time checks safe across counter wrap. */
static uint8_t interval_elapsed(uint32_t now, uint32_t start, uint32_t interval)
{
    return (uint8_t)((uint32_t)(now - start) >= interval);
}

static void adc_init(void)
{
    /* P0.23 function 01 selects AD0.0. Preserve all other pin selections. */
    LPC_PINCON->PINSEL1 = (LPC_PINCON->PINSEL1 & ~(3u << 14)) | (1u << 14);
    LPC_PINCON->PINMODE1 = (LPC_PINCON->PINMODE1 & ~(3u << 14)) | (2u << 14);

    LPC_SC->PCONP |= (1u << 12);       /* Power ADC. */
    LPC_SC->PCLKSEL0 &= ~(3u << 24);   /* ADC PCLK = CCLK / 4. */

    /* ADC clock = PCLK / (CLKDIV + 1) = CCLK / 16; <= 6.25 MHz at 100 MHz. */
    LPC_ADC->ADCR = (1u << 0) | (3u << 8) | (1u << 21);
}

static uint8_t adc_read_channel_zero(uint16_t *value)
{
    uint32_t remaining = ADC_CONVERSION_TIMEOUT;

    LPC_ADC->ADCR &= ~(7u << 24);       /* Clear any previous START value. */
    LPC_ADC->ADCR |= (1u << 24);        /* Software-start channel 0. */

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

static uint32_t pwm_period_ticks(void)
{
    uint32_t ticks = SystemCoreClock / PWM_FREQUENCY_HZ;

    return (ticks == 0u) ? 1u : ticks;
}

static void pwm_init(void)
{
    LPC_SC->PCONP |= (1u << 6);

    LPC_SC->PCLKSEL0 = (LPC_SC->PCLKSEL0 & ~(3u << 12)) | (1u << 12);

    LPC_PINCON->PINSEL3 = (LPC_PINCON->PINSEL3 & ~(3u << 14)) | (2u << 14);

    LPC_PWM1->TCR = 2u;                 /* Reset counter and prescale counter. */
    LPC_PWM1->PR = 0u;
    LPC_PWM1->MR0 = pwm_period_ticks();
    LPC_PWM1->MR4 = 0u;
    LPC_PWM1->MCR = (1u << 1);          /* Reset counter on MR0 match. */
    LPC_PWM1->PCR = (1u << 12);         /* Enable single-edge PWM1.4 output. */
    LPC_PWM1->LER = (1u << 0) | (1u << 4);
    LPC_PWM1->TCR = (1u << 0) | (1u << 3); /* Enable PWM and counter. */
}

static void set_pwm_duty(uint8_t duty_percent)
{
    uint32_t period = pwm_period_ticks();

    if (duty_percent > 100u) {
        duty_percent = 100u;
    }

    if (duty_percent == current_duty) {
        return;
    }

    current_duty = duty_percent;
    LPC_PWM1->MR4 = (period * duty_percent) / 100u;
    LPC_PWM1->LER = (1u << 4);          /* Latch new MR4 on next PWM cycle. */
}

static void gpio_init(void)
{
    /* P2.0 GPIO input for the HC-SR501 output. */
    LPC_PINCON->PINSEL4 &= ~(3u << 0);
    LPC_GPIO2->FIODIR &= ~(1u << 0);
}

static uint8_t brightness_for_adc(uint16_t adc_value)
{
    uint32_t darkness_range;
    uint32_t brightness_range;
    uint32_t scaled_brightness;

    if (adc_value <= DARK_THRESHOLD) {
        return PWM_MIN_DUTY_PERCENT;
    }

    darkness_range = 4095u - DARK_THRESHOLD;
    brightness_range = PWM_MAX_DUTY_PERCENT - PWM_MIN_DUTY_PERCENT;
    scaled_brightness = PWM_MIN_DUTY_PERCENT +
        (((uint32_t)(adc_value - DARK_THRESHOLD) * brightness_range) /
         darkness_range);

    return (uint8_t)scaled_brightness;
}

int main(void)
{
    uint32_t start_time;
    uint32_t last_adc_sample;
    uint32_t motion_end_time = 0u;
    uint32_t fade_start_time = 0u;
    uint8_t fade_start_duty = 0u;
    uint8_t previous_pir_level = 0u;
    uint8_t pir_armed = 0u;
    uint8_t motion_active = 0u;
    uint16_t adc_sample = 0u;

    SystemInit();
    SystemCoreClockUpdate();
    milliseconds = 0u;
    SysTick_Config(SystemCoreClock / 1000u);

    adc_init();
    gpio_init();
    current_duty = 0xFFu;               /* Force initial 0% write to PWM. */
    pwm_init();
    set_pwm_duty(0u);

    start_time = get_milliseconds();
    last_adc_sample = start_time - ADC_SAMPLE_INTERVAL_MS;

    for (;;) {
        uint32_t now = get_milliseconds();

        if (interval_elapsed(now, last_adc_sample, ADC_SAMPLE_INTERVAL_MS)) {
            last_adc_sample = now;

            if (adc_read_channel_zero(&adc_sample)) {
                adc_raw = adc_sample;
                adc_error = 0u;
            } else {
                adc_error = 1u;
            }

            /* Hysteresis: enter dark at the upper threshold, exit at lower. */
            if (!is_dark && adc_raw >= DARK_THRESHOLD) {
                is_dark = 1u;
            } else if (is_dark && adc_raw <= BRIGHT_THRESHOLD) {
                is_dark = 0u;
            }
        }

        pir_level = ((LPC_GPIO2->FIOPIN & (1u << 0)) != 0u) ? 1u : 0u;
        warmup_complete = interval_elapsed(now, start_time, PIR_WARMUP_MS);

        if (!warmup_complete) {
            /* Ignore startup HIGHs. Require a LOW after warm-up to arm. */
            previous_pir_level = pir_level;
            pir_armed = 0u;
            motion_active = 0u;
            lamp_state = LAMP_OFF;
        } else if (!pir_armed) {
            if (pir_level == 0u) {
                pir_armed = 1u;
            }
            previous_pir_level = pir_level;
        } else {
            if ((pir_level != 0u) && (previous_pir_level == 0u)) {
                motion_active = 1u;
                lamp_state = LAMP_MOTION;
            }

            if ((pir_level == 0u) && (previous_pir_level != 0u) && motion_active) {
                motion_active = 0u;
                motion_end_time = now;
                lamp_state = LAMP_HOLD;
            }

            previous_pir_level = pir_level;

            /* A sustained HIGH keeps the lamp active. */
            if (pir_level != 0u) {
                motion_active = 1u;
                lamp_state = LAMP_MOTION;
            }
        }

        if (!is_dark) {
            /* Bright environment always clears hold/fade and switches off. */
            lamp_state = LAMP_OFF;
            motion_active = 0u;
            set_pwm_duty(0u);
        } else {
            target_brightness = brightness_for_adc(adc_raw);

            if (motion_active || (pir_level != 0u && pir_armed)) {
                lamp_state = LAMP_MOTION;
                set_pwm_duty(target_brightness);
            } else if (lamp_state == LAMP_HOLD) {
                if (interval_elapsed(now, motion_end_time, LAMP_HOLD_MS)) {
                    lamp_state = LAMP_FADE;
                    fade_start_time = now;
                    fade_start_duty = current_duty;
                } else {
                    set_pwm_duty(target_brightness);
                }
            } else if (lamp_state == LAMP_FADE) {
                uint32_t fade_elapsed = (uint32_t)(now - fade_start_time);

                if (fade_elapsed >= FADE_OUT_MS) {
                    lamp_state = LAMP_OFF;
                    set_pwm_duty(0u);
                } else {
                    uint32_t fade_duty =
                        ((uint32_t)fade_start_duty * (FADE_OUT_MS - fade_elapsed)) /
                        FADE_OUT_MS;
                    set_pwm_duty((uint8_t)fade_duty);
                }
            } else {
                set_pwm_duty(0u);
            }
        }
    }
}
