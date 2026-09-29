# C3 BLE task stack allocation

BLE-Gamepad-Client 0.12.1 hardcodes 10,000-byte FreeRTOS stacks at five source locations. One Xbox controller instantiates the value receiver twice, so the application allocates six such task stacks before normal startup: 60,000 bytes total. This competes with Wi-Fi, HTTP responses, JSON documents and queued WebSocket telemetry on the C3, which has no PSRAM.

The C3 build sets `ANTCORE_BLE_TASK_STACK_SIZE=4096`. Six task stacks then use 24,576 bytes, releasing 35,424 bytes for the application. The S3 build retains upstream stack sizes.

`tools/pio_ble_task_stack.py` is registered as a **pre** PlatformIO extra script. Its build middleware validates the installed library name/version and SHA256 hashes of the five pinned source files. It generates copies under the environment's build directory, replaces only the hardcoded task-stack argument with the configuration macro, and compiles those copies with the library's existing flags and include paths. The generated files preserve a 10,000-byte macro fallback. The downloaded library files remain untouched; deleting `.pio` and reinstalling dependencies reproduces the adaptation.

The middleware must run before PlatformIO constructs the compilation graph, so a post extra script is unsuitable. Unexpected dependency versions or source contents fail the build and require explicit review of the adaptation.

The C3 environment requires:

```ini
extra_scripts = pre:tools/pio_ble_task_stack.py
build_flags =
  ${antcore.build_flags}
  ; Retain any other existing C3 flags.
  -D ANTCORE_BLE_TASK_STACK_SIZE=4096
```

Check the adaptation without compiling or touching hardware:

```powershell
python tools/pio_ble_task_stack.py --check .pio/libdeps/seeed_xiao_esp32c3/BLE-Gamepad-Client --self-test
```

These checks verify all five substitutions, the upstream fallback, rejection of changed sources/versions, C3-only scope, repeatable generation, and unchanged downloaded sources. They do not establish that 4096 bytes is sufficient for every pairing, reconnection or controller event path. Connected-controller stress and task-stack high-water measurements remain necessary to validate that margin on hardware.
