# Board compatibility on the existing PCB

Both PlatformIO targets now preserve every existing PCB pin assignment. The earlier C3 proposal to swap D1/D8 and change the divider has been removed.

| Feature | S3 Sense | C3 |
| --- | --- | --- |
| Motor 1 A/B | D0/D1 | D0/D1 |
| Motor 2 A/B | D2/D3 | D2/D3 |
| Motor 3 A/B | D9/D10 | D9/D10; see reset issue below |
| Two servo outputs | D6/D7 | D6/D7 |
| BMI270 I2C | D4/D5 | D4/D5 |
| BLE Xbox, Wi-Fi, browser driving | Yes | Yes |
| Profiles, logs, output tests, OTA | Yes | Yes |
| Camera / FPV | Yes | Compiled out |
| Analog battery measurement | D8, existing 20k/10k divider | Disabled; D8 left undriven and unread |
| Low-voltage warning, disarm, derating | Available | Unavailable |
| USB bench-mode interlock | Yes | Yes |
| Programmable onboard status LED | Yes | No |
| Flash / PSRAM | 8 MB / 8 MB | 4 MB / none |

No motor pin swap or divider resistor change is required by the firmware. C3 provides all three motor outputs and both servos at runtime. Importing an S3 profile cannot enable its unsupported hardware: battery protection/derating are forced off during configuration normalization, and ADC access is compiled out. Control-loss disarm, pit mode, authentication and OTA interlocks remain.

The UI shows battery readings as unavailable, disables battery/camera settings, and removes unavailable measurement steps from commissioning/readiness checks. It warns that the battery must be checked externally. Bench mode and its Save button remain usable. Saved pack notes remain available; live voltage/sag measurements do not.

## Findings from the supplied Rev 3.0 schematic

The supplied Complete Combat Bot Design, Rev 3.0, dated 2023-10-27, shows:

- R6 (20k) and R4 (10k) connect the switched Link rail to BatterySense/D8. C3 GPIO8 has no ADC. Disabling ADC reads allows that connection to remain as drawn.
- U4, U5 and U7 are DRV8837C drivers. Their nSLEEP pins are wired to 3V3, so firmware has no independent driver-enable signal.
- No external motor-input pull-downs are shown. The DRV8837C itself has approximately 100k input pull-downs.
- D9/GPIO9 reaches one of U7's inputs. Normal C3 flash boot requires GPIO9 high. With the other input low and nSLEEP high, U7 commands a motor direction. Motor 3 can therefore be powered during reset/boot before application software makes both inputs low.

This is a reset-time electrical conflict, not a shortage of PWM channels. Compiling out motor 3 or writing the pin low in setup cannot prevent the interval before software executes. Holding GPIO9 low instead prevents normal flash boot. Reset, watchdog recovery and upload must all be considered, not just initial power-on.

The software can use the existing PCB connections, but a safe C3 drop-in with motor 3 connected and powered cannot be claimed from this schematic. Retaining these manufactured boards may be possible with a small reset-time driver-inhibit/isolation modification or an adapter; that circuit still needs design and validation. Keep motor loads disconnected for initial firmware evaluation. Disabling motor 3 in configuration does not fix the electrical issue.

D8/GPIO8 must be high for forced download mode. The existing divider follows Link, so USB-only flashing with Link off can hold D8 low and prevent bootloader entry. Program the C3 off the carrier for that case; do not enable motor power merely to make flashing work. Normal flash boot permits either GPIO8 level. D0/GPIO2 also has startup/glitch considerations. These reset behaviors are determined before firmware runs.

## PWM without changing PCB pins

S3 retains six LEDC motor channels, MCPWM servos and its camera clock. C3 uses three hardware PWM channels at 20 kHz, one per motor, each routed to the currently driven input of its bridge. The other input stays low. Reversal disconnects and drives the old input low before attaching the other at zero duty; stop leaves both inputs low. Servo allocation is restricted to timer 2/channels 4-5 at 50 Hz. Channel 3 is unused. Software PWM is unnecessary.

## Build and upload

```powershell
pio run -e seeed_xiao_esp32s3
pio run -e seeed_xiao_esp32c3
pio run -e seeed_xiao_esp32c3 -t upload
pio run -e seeed_xiao_esp32c3 -t uploadfs
```

Choose firmware and filesystem images from `.pio/build/<matching environment>/`. C3 uses two 1728 KiB firmware slots and 512 KiB LittleFS in 4 MB flash. Initial installation needs USB to install the partition table; OTA does not migrate it. S3's partition layout is unchanged. Back up profiles before filesystem uploads.

Builds, simulated output tests and browser audits establish software behavior only. Physical C3 timing, BLE/Wi-Fi coexistence, memory margin and reset safety still need bench validation. Successful compilation does not validate carrier boot behavior.

References: [Espressif C3 boot requirements](https://documentation.espressif.com/esp32-c3_datasheet_en.html), [TI DRV8837C logic and input pull-downs](https://www.ti.com/lit/ds/symlink/drv8837c.pdf), [Seeed C3 pinout](https://wiki.seeedstudio.com/XIAO_ESP32C3_Getting_Started/), [C3 LEDC channels](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32c3/api-reference/peripherals/ledc.html).
