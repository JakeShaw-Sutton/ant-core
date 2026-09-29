# Ant Core Pinout

This pinout is for the Ant Core PCB with a Seeed XIAO ESP32-S3 Sense fitted.

XIAO ESP32-C3 uses the same output pin assignments. Battery sensing is disabled and D8 is left undriven; keep the existing divider. See the [C3 capability and reset-time motor 3 issue](board-compatibility.md) before powering motors on C3.

| XIAO pin | Ant Core function | Notes |
| --- | --- | --- |
| D0 | Motor 1 side A | DRV8837 input. Positive drive PWM uses side A. |
| D1 | Motor 1 side B | DRV8837 input. Reverse drive PWM uses side B. |
| D2 | Motor 2 side A | Shares the XIAO Sense SD-card pin group, so SD support is disabled. |
| D3 | Motor 2 side B | DRV8837 input. |
| D4 | I2C SDA | Optional BMI270. |
| D5 | I2C SCL | Optional BMI270. |
| D6 | Servo 1 signal | Configure min, neutral, max, failsafe, and detach behavior before live use. |
| D7 | Servo 2 signal | Configure min, neutral, max, failsafe, and detach behavior before live use. |
| D8 | 2S LiPo sense | 20k top resistor to battery positive, 10k bottom resistor to ground. Firmware multiplier defaults to `3.0`. |
| D9 | Motor 3 side A | Defaults to weapon motor output when enabled. Shares the Sense SD-card pin group. |
| D10 | Motor 3 side B | Defaults to weapon motor output when enabled. Shares the Sense SD-card pin group. |

## Motor Polarity

Each motor output is controlled as a signed value from `-1.0` to `1.0`.

- Positive output: side A PWM, side B low.
- Negative output: side B PWM, side A low.
- Disarmed, failsafe, OTA, and pit mode: both sides low.

Use the Drive/Motors page to invert a motor instead of rewiring once the robot is assembled.

## PWM Channels

Motor PWM uses LEDC channels `0` through `5`: motor 1 uses timer `0`, motor 2 uses timer `1`, and motor 3 uses timer `2`. The camera XCLK uses LEDC channel `7` on timer `3`, so changing motor PWM setup must not reuse timer `3` or channel `7`.

## Sense Module Notes

The camera stays enabled with the XIAO ESP32-S3 Sense pin map. SD-card support is intentionally disabled because the Sense SD pins overlap Ant Core motor and battery pins.
