# ES_LAB_PROJ

Motion-activated smart corridor night lamp for the LPC1768 ES lab trainer. The project uses an LDR to detect darkness, an HC-SR501 PIR sensor to detect motion, and LD10 as the PWM-controlled lamp.

## Source files

| File | Purpose |
| --- | --- |
| `smart_corridor_lamp.c` | Complete lamp program: ADC sampling, PIR warm-up, brightness control, hold time, and fade-out. |
| `ldr_debug.c` | Read and inspect the raw LDR ADC value. |
| `pir_debug.c` | Read the PIR signal and count transitions. |
| `ld10_gpio_debug.c` | Blink LD10 as ordinary GPIO, without PWM or sensors. |
| `ld10_pwm_debug.c` | Hold LD10 at 50% PWM, without sensors or thresholds. |
| `cnd_gpio_debug.c` | Blink the board LED bank through the CND → CNA1 ribbon. |
| `cnd_p2_0_locator.c` | Blink only the board LED driven by P2.0 through CND → CNA1. |
| `sensor_override_debug.c` | Observe both sensors with an active-low switch on P2.12 to force the software dark condition. |
| `sensor_keyboard_debug.c` | Observe both sensors with the trainer keypad to force the software dark condition. |

Build **one** C file at a time. Each file defines its own `main()` and is a separate firmware program. Use the LPC17xx CMSIS device header and the board startup and clock files in your Keil project.

## Connections

| Circuit point | LPC1768 trainer connection |
| --- | --- |
| LDR divider junction | P0.23 / AD0.0, CND pin 1 |
| PIR OUT | P2.0, CND pin 7 |
| LD10 PWM | P1.23 / PWM1.4, using the CNB → CNB1 FRC cable |
| Switch diagnostic | P2.12, CNB pin 7; active-low switch to GND |

Wire the LDR divider as **3.3 V → 10 kΩ → ADC junction → LDR → GND**. Power the HC-SR501 from **5 V and the same GND**. Its OUT signal must stay within the LPC1768 input range used here (3.3 V logic). Confirm the test-point labels and connector orientation on the physical trainer before applying power.

For the keyboard diagnostic, connect **CNB → CNB3** and set **JP4 to 1–2**. Use the keyboard and switch diagnostics separately because they use overlapping board connections.

## Check LD10 before testing the sensors

With power off, connect the 10-pin FRC cable **CNB → CNB1**. Flash `ld10_gpio_debug.c` by itself: LD10 should blink. Then flash `ld10_pwm_debug.c` by itself: LD10 should stay visibly lit at 50% brightness. These programs do not use CND pin 7, the PIR, or the LDR. If the GPIO program works but the PWM program does not, investigate PWM1.4. If neither works, check the CNB-to-CNB1 cable, LD10, and board power.

**CND pin 7 is P2.0, the PIR input; it is not the LED output.** On a 2-by-5 box header, find the pin-1 mark first. Pins are numbered by pairs from that end:

```text
1   2
3   4
5   6
7   8   ← P2.0 is pin 7
9  10
```

The drawing can be rotated to match your board; do not assume which physical corner is pin 1 without its mark.

## Check CND with the board LEDs

Power off and disconnect PIR OUT and the LDR divider from CND. These probe programs drive the sensor pins as outputs. Connect a 10-pin FRC ribbon **CND → CNA1**. The board LED bank is connected to CNA1; the manual's LED exercise uses **CNA → CNA1**, and this diagnostic substitutes CND to check its eight GPIO signals. Align the ribbon's keyed ends with both headers.

Flash `cnd_gpio_debug.c` alone: the eight board LEDs connected to CNA1 should blink together. Then flash `cnd_p2_0_locator.c` alone: only the board LED reached through CND pin 7 (P2.0) should blink. The exact printed LD number depends on the CNA1 LED order; the lab manual does not specify that order. CND pin 9 is not connected and pin 10 is GND; neither is configured as an output by these programs. This checks the P2.0 route, but the blinking LED alone does not identify the physical pin-7 hole after removing the ribbon. Locate pin 1 using the connector's key or ribbon stripe and count down its side to pin 7, or check continuity with power off. Restore the PIR and LDR connections before running the lamp program.

For the separate PWM lamp LED LD10, use **CNB → CNB1** and the LD10 programs above.

The ADC thresholds in the source are starting values. Run `ldr_debug.c`, measure `adc_raw` in normal room light and with the LDR covered, then adjust `DARK_THRESHOLD` and `BRIGHT_THRESHOLD` in the programs. With the divider above, a higher ADC reading means darker light.
