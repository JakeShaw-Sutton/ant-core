# Wiring And Safety Checklist

Antweight robots can move violently on a bench. Work through this list before enabling live outputs.

## Power Wiring

- Fit the safety link in series with the 2S LiPo positive lead so the whole robot can be made electrically dead.
- Keep the XIAO USB cable connected only for setup, logs, and firmware upload. Do not rely on USB as the output safety system.
- Check battery polarity at the Ant Core connector before plugging the board in.
- Keep the PCB divider: 20k from Link to D8 and 10k to ground. S3 reads it; C3 leaves D8 unused and provides no battery-voltage protection. Check the C3 battery externally.
- Use battery calibration before trusting low-voltage warnings.

## Motor Wiring

- Lift the robot so wheels or weapon cannot touch the bench.
- Keep the safety link removed until firmware boots and shows `DISARMED`.
- Fit external pull-down resistors on motor driver inputs and other live-output control lines so attached hardware stays off during reset, bootloader, or firmware upload windows before firmware configures the pins.
- On C3, D0 and D9 are motor signals with boot-strapping roles; D8 is the unused divider input and also a strap. The supplied Rev 3.0 schematic ties U7 nSLEEP high: D9's required boot-high state can drive motor 3 before firmware runs. Do not solve this by pulling D9 low, which prevents normal boot. See the [schematic findings](board-compatibility.md); motor 3 needs reset-time hardware inhibition or must remain disconnected during evaluation.
- Configure motor direction on the Drive/Motors page using low power commands first.
- Leave live motor tests disabled except during a deliberate bench test.
- Use pit mode whenever the robot is being configured at an event.

## Servo Wiring

- Power servos from a supply that can handle stall current.
- Set servo min, neutral, max, and failsafe pulse widths before connecting mechanisms.
- Test servo travel with linkages disconnected first.
- Enable detach-on-disarm only when the mechanism is safe without holding torque.

## First Power-Up

1. Remove the safety link.
2. Connect USB.
3. Upload firmware and filesystem.
4. Join `AntCore-XXXX` or open `http://antcore.local/` on the LAN.
5. If the captive portal does not open, scan the setup page QR code or browse to `http://192.168.4.1/`.
6. Change the admin PIN and AP password.
7. Confirm battery voltage and calibration.
8. Pair the Xbox controller or use the FPV web controls.
9. Complete drive direction and servo limit checks.
10. Save a named profile and export the config.
11. Fit the safety link only when the robot is restrained and ready for a controlled live test.

## Event Checklist

- Admin PIN changed from default.
- AP password changed from default.
- Battery voltage above warning threshold.
- Pit mode on while in the pits.
- Weapon safe until the arena is closed.
- Blackbox log exported after any unexplained reset, failsafe, or control loss.
