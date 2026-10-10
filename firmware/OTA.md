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

## LCD5110 backlight behavior

The firmware profile uses GPIO4 (contact 28 in the supplied preliminary map) as a
backlight control signal. The first button event while the display is asleep
wakes the backlight for five minutes and is consumed, so it does not change the
selected meter. Further button activity extends the five-minute timeout.

**Hardware check required:** if the LCD5110 backlight input is still tied directly
to 3.3 V, firmware cannot turn it off. Route the backlight through a suitable
transistor/MOSFET controlled by GPIO4; do not drive the LED current directly from
the ESP32 pin. GPIO4 must not also be connected to another active peripheral.

The display module exposes an alarm-blink interface, but alarm sources and
thresholds are not yet configured. No battery, stale-packet, or reading threshold
is assumed by the current firmware.

## Alarm defaults and configuration

The web page has persistent alarm settings (saved in ESP32 Preferences):
- low battery: **20% or less**;
- missing BLE packets: **15 minutes**;
- reading lower/upper limits: **disabled by default**.

Only meters with both **Active** and **Alarms** enabled participate. Battery and missing-packet alarms can be configured for all meters. Optional reading limits currently apply to the primary numeric `reading` field on all meters; enable them only when those meters use compatible units and scales. For mixed gas/water/electricity meters, leave numeric limits disabled until per-meter limits are implemented. Any active alarm blinks the display backlight.

These are provisional commissioning defaults, not validated safety limits. Verify battery reporting and BLE advertisement intervals on the actual meter models before relying on them.
