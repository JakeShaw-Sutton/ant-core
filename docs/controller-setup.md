# Controller Setup

Ant Core accepts control from an Xbox BLE controller or from the browser FPV gamepad. The safety model treats these as competing driver sources, so only one source should actively drive the robot at a time.

## Pair An Xbox Controller

1. Open Controller Pairing.
2. Log in with the admin PIN.
3. Press BLE scan.
4. Put the Xbox controller into pairing mode.
5. Wait for the Controller Inspector to show live axes and buttons.

If the controller has unreliable BLE behavior, update the controller firmware from an Xbox console or Windows Xbox Accessories app before event use.

## Learn Mappings

Most mapping fields have a Learn button.

1. Press Learn next to the field.
2. Move the target axis or press the target button.
3. Confirm the field updates.
4. Save the page.
5. Use mapping test mode to inspect inputs while arming and outputs stay locked.

Recommended default mappings:

| Robot action | Suggested input |
| --- | --- |
| Robot arm toggle | Menu/Start |
| Drive throttle | Left stick Y |
| Drive turn | Left stick X |
| Precision mode | Left stick click |
| Turbo mode | Right stick click |
| Drive invert | View/Back |
| Weapon arm | X |
| Weapon input | Right trigger |
| Self-right macro | Y or a guarded action slot |

## Drive Presets

- Use the Drive page preset buttons first, then adjust individual fields if the robot needs a custom setup.
- Arcade: one stick for throttle and turn. Good default for 2WD skid steer.
- Tank: one axis per side. Useful for careful testing and unusual drivetrains.
- Invertible: map a drive invert button for robots that can run upside down.
- Gyro assist: enable only when the BMI270 is fitted and mounted firmly.

## Browser FPV Controls

The FPV page has video in the center and virtual controls on both sides. Press `TAKE` to claim the web driver lock. If another browser has control, release it from that browser or use Diagnostics to release web control.

The virtual pad sends both sticks, bumpers, D-pad, face buttons, View/Menu, stick-click buttons, and the two trigger sliders. Trigger sliders are momentary and return to zero on release so a weapon or servo trigger cannot remain set accidentally.

Browser controls stale quickly:

- Control frames older than 250 ms command neutral output.
- A web driver missing for 750 ms forces disarm.

## Weapon And Servo Setup

- Keep robot arm and weapon arm separate.
- Use weapon presets to start from disabled, brushed, reversible, spinner, lifter/flipper, or toggle output behavior.
- Use spinner ramp-up and ramp-down for high inertia weapons.
- Use the servo preset buttons to start from joystick proportional, trigger proportional, button positions, toggle positions, failsafe hold, or detach-on-disarm behavior.
- Use momentary servo positions for flippers and lifters unless toggle behavior is deliberate.
- Set a failsafe servo pulse that cannot bind the mechanism.
