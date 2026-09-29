# Battery Calibration

On S3 Sense, Ant Core measures a 2S LiPo through the existing 20k/10k divider into D8 (multiplier `3.0`). C3 leaves D8 unused and disables measurement, voltage warnings, low-voltage disarm and derating. Keep the existing PCB divider and check the battery externally. USB bench mode remains available on both boards. See [board compatibility](board-compatibility.md). The calibration procedure below applies only to S3 Sense.

## Divider

```text
2S LiPo positive ---- 20k ---- D8 ---- 10k ---- GND
```

Firmware calculates:

```text
pack voltage = adc voltage * divider multiplier * calibration
cell voltage = pack voltage / 2
```

## Calibration Steps

1. Charge or storage-charge a 2S pack.
2. Measure pack voltage at the battery connector with a trusted multimeter.
3. Open Diagnostics.
4. Read the displayed pack voltage and raw ADC voltage.
5. Set calibration to:

```text
new calibration = old calibration * measured voltage / displayed voltage
```

6. Save Battery.
7. Confirm the displayed voltage now matches the multimeter.

Do not calibrate while USB is the only power source unless bench mode is intentionally enabled.

## Thresholds

Recommended defaults:

| Setting | Default | Behavior |
| --- | --- | --- |
| Warning | 7.0 V | Shows warning banners and log events. |
| Derate | 6.8 V | Optional output scaling before critical disarm. |
| Critical | 6.4 V | Blocks arming and disarms the robot. |

For event use, leave critical disarm enabled. Use USB bench mode only when the robot has no LiPo fitted and outputs are physically safe.

## Common Faults

- `0.00 V`: no battery, divider not fitted, broken ground, or bench setup.
- Reading too high: divider multiplier or calibration too high.
- Reading too low: divider multiplier or calibration too low, weak pack, or wiring resistance.
- Voltage moves when motors run: battery sag or inadequate wiring. Raise warning threshold or enable derating for testing.
