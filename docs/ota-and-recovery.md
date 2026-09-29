# OTA And Recovery

OTA is useful once Ant Core is soldered inside a robot, but it is still a high-risk operation because the robot is powered while firmware is changing.

## Before OTA

- Remove the weapon belt or otherwise make the weapon safe.
- Put the robot on a stand.
- Confirm the battery is not near the critical threshold.
- Log in with the admin PIN.
- Leave the browser open until the upload completes.

Starting OTA disarms the robot. Motor and servo outputs remain safe during the upload.

## Firmware OTA

The commands below use the default S3 Sense environment. For C3, add `-e seeed_xiao_esp32c3` to build/upload commands and choose images from `.pio/build/seeed_xiao_esp32c3/`. Never mix board images. Initial C3 setup requires USB upload to install its 4 MB partition table; see [board compatibility](board-compatibility.md).

1. Run `pio run` locally.
2. Open the OTA page.
3. Choose `.pio/build/seeed_xiao_esp32s3/firmware.bin`.
4. Upload firmware.
5. Wait for the success message and automatic reboot.
6. Reconnect to `http://antcore.local/`, the LAN IP, or the AP captive portal.

## Filesystem OTA

1. Run `pio run --target buildfs`.
2. Open the OTA page.
3. Choose `.pio/build/seeed_xiao_esp32s3/littlefs.bin`.
4. Upload filesystem.
5. Wait for reboot, then hard refresh the browser.

Upload firmware and filesystem as separate operations. If web assets look stale after a filesystem update, refresh the browser cache.

## Serial Recovery

If OTA fails or the web UI is unavailable:

```powershell
pio run --target upload
pio run --target uploadfs
```

PlatformIO normally auto-detects the XIAO ESP32-S3 USB serial port. If more than one serial device is connected, pass an explicit upload port on the command line, for example `pio run --target upload --upload-port COM7`.

## Factory Reset

Use Diagnostics -> Factory Reset when configuration is bad but the web UI is available. Factory reset disarms the robot, restores default config, and keeps firmware intact.

Before reset, export the config if possible. After reset, rerun the commissioning flow and save a new named profile.

## Fault Triage

- Dashboard reset reason shows why the ESP rebooted.
- Diagnostics can export the persistent blackbox log.
- Capture logs after controller disconnects, battery warnings, OTA attempts, or unexpected disarms.
