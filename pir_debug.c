/* Standalone live PIR probe. Inspect pir_level and pir_edges in Keil Watch. */
#include "LPC17xx.h"
#include <stdint.h>

volatile uint32_t pir_edges;
volatile uint8_t pir_level;

int main(void)
{
    uint8_t previous_level;

    SystemInit();
    SystemCoreClockUpdate();

    /* P2.0 function 00 is GPIO. Disable internal bias; HC-SR501 drives OUT. */
    LPC_PINCON->PINSEL4 &= ~(3u << 0);
    LPC_PINCON->PINMODE4 = (LPC_PINCON->PINMODE4 & ~(3u << 0)) | (2u << 0);
    LPC_GPIO2->FIODIR &= ~(1u << 0);

    previous_level = ((LPC_GPIO2->FIOPIN & (1u << 0)) != 0u) ? 1u : 0u;

    for (;;) {
        pir_level = ((LPC_GPIO2->FIOPIN & (1u << 0)) != 0u) ? 1u : 0u;

        if (pir_level != previous_level) {
            ++pir_edges;
            previous_level = pir_level;
        }
    }
}
