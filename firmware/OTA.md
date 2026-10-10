# ESP32-C3 OTA update

## Build

Build the `esp32-c3` PlatformIO environment. Upload the application image
`.pio/build/esp32-c3/firmware.bin` (not a bootloader or merged factory image).

The ESP32-C3 environment uses `partitions.csv`: two 1.5 MiB OTA app slots and
approximately 956 KiB of LittleFS. The current CI build before the OTA handler
was added used about 1.10 MiB of the 1.50 MiB app slot; verify the size again
after firmware changes.

## Update over the device access point

1. Power on the monitor and open Serial Monitor at 115200 baud.
2. Read the OTA URL and generated `admin` password from the serial output.
   The password is generated once and saved in Preferences.
3. Connect to the monitor's Wi-Fi access point and open
   `http://192.168.4.1/update`.
4. Authenticate as `admin`, select `firmware.bin`, and start the update.
5. Do not disconnect power while the upload is running. The device restarts
   after a successful write.

The OTA route accepts requests only from the monitor's `192.168.4.0/24`
access-point subnet and requires HTTP Basic authentication. It uses HTTP, not
TLS; perform updates only while connected directly to the trusted device AP.
Keep a copy of the known-good serial build and test recovery on the actual
board before relying on OTA in production.

OTA support is firmware groundwork and has not yet been physically tested on
the target board. Never flash a binary built for a different board profile.
