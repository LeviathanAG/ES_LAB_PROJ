# ES_LAB_PROJ

Motion-activated smart corridor night lamp for the LPC1768 ES lab trainer. The project uses an LDR to detect darkness, an HC-SR501 PIR sensor to detect motion, and LD10 as the PWM-controlled lamp.

## Source files

| File | Purpose |
| --- | --- |
| `smart_corridor_lamp.c` | Complete lamp program: ADC sampling, PIR warm-up, brightness control, hold time, and fade-out. |
| `ldr_debug.c` | Read and inspect the raw LDR ADC value. |
| `pir_debug.c` | Read the PIR signal and count transitions. |
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

The ADC thresholds in the source are starting values. Run `ldr_debug.c`, measure `adc_raw` in normal room light and with the LDR covered, then adjust `DARK_THRESHOLD` and `BRIGHT_THRESHOLD` in the programs. With the divider above, a higher ADC reading means darker light.
