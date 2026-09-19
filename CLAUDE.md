# SpeedRack Display — Project Notes

Speedometer test fixture: ESP32-C6 display device (Waveshare
ESP32-C6-LCD-1.47, **non-touch**) showing live speed + diagnostics, driven by
GPIO toggles/relays and MQTT. See [UI_INTEGRATION.md](UI_INTEGRATION.md) for
the UI/firmware contract (screen API, MQTT topics, GPIO truth table) — updated
for BOOT-button navigation (see "Full port plan" below).

## Hardware on hand

**Correction (2026-09-19): this is the non-touch board**, not the Touch-LCD
variant assumed earlier. Confirmed via docs.waveshare.com/ESP32-C6-LCD-1.47
(the product page for *this exact* board, distinct from the ESP32-C6-Touch-
LCD-1.47 wiki page used earlier — mixing the two up was the likely root cause
of the original blank-screen/touch-error smoke test).

- Board: **Waveshare ESP32-C6-LCD-1.47** (non-touch). Display driver is
  **ST7789**, not JD9853. **No touch controller on this board at all** — the
  earlier "continuous I2C read errors" from the AXS5106 touch driver weren't a
  driver bug, they were reads against a chip that doesn't exist on this board.
- Confirmed pin table (docs.waveshare.com/ESP32-C6-LCD-1.47):
  - `MOSI` = GPIO6, `SCLK` = GPIO7, `LCD_CS` = GPIO14, `LCD_DC` = GPIO15,
    `LCD_RST` = GPIO21, `LCD_BL` = GPIO22. Backlight is PWM-capable — driven
    via LEDC in `firmware/main/main.c`, hard-capped at 50% duty (see "Port
    status" below).
  - Also on board: RGB LED on GPIO8, BOOT button on GPIO9, RESET button, TF
    card slot (SPI), USB-C. No IMU, no battery monitor (contrast with what the
    Touch variant's demo package assumed).
  - Resolution still 172×320 — unaffected by the chip mixup.
  - ST7789 is a standard ESP-IDF panel type (`esp_lcd_new_panel_st7789()` in
    `esp_lcd_panel_vendor.h`, part of the core `esp_lcd` component) — **no
    need to vendor Waveshare's custom `esp_bsp` / `esp_lcd_jd9853` /
    `esp_lcd_touch_axs5106` components at all** for this board. Much simpler
    port than originally planned.
- **Unresolved discrepancy**: docs.waveshare.com reportedly lists this board's
  MCU as ESP32-C6FH4 (4MB flash), but the connected chip identifies via
  `esptool` as **ESP32-C6FH8 (QFN32), silicon revision v0.2, 8MB flash**, MAC
  `98:88:e0:6c:00:38`. Could be a batch/revision difference, could be an
  imprecise doc scrape — doesn't block the display port either way, but worth
  a sanity check later if flash partitioning ever gets tight.
- Shows up at `/dev/ttyACM0` (USB-JTAG/serial, `lsusb`: "Espressif USB JTAG/serial
  debug unit", VID:PID `303a:1001`).
- **UI contract conflict — resolved**: [UI_INTEGRATION.md](UI_INTEGRATION.md)
  previously assumed touch navigation; that can't work on this hardware. Now
  uses the onboard **BOOT button (GPIO9)** to cycle between home/diagnostics
  screens instead, via the new `ui_toggle_screen()` (see "Full port plan"
  below for implementation details).

## Toolchain setup (this machine, Fedora/Asahi aarch64)

No `idf.py`, PlatformIO, or `esptool` was preinstalled, and no `pip3`. Setup used:

```bash
sudo usermod -aG dialout matt      # serial port is root:dialout
sudo dnf install python3-pip
python3 -m pip install --user platformio
export PATH="$HOME/.local/bin:$PATH"   # pio lands in ~/.local/bin
```

Group membership doesn't apply to already-running shells. Rather than
re-login, wrap flash/monitor commands in `sg dialout -c "..."` to get serial
port access in the current session, e.g.:

```bash
sg dialout -c "pio run --target upload --upload-port /dev/ttyACM0"
```

**USB hot-plug quirk observed 2026-09-19**: after a flash + hard reset,
`/dev/ttyACM0` sometimes disappears from the bus entirely (not just the
device, `lsusb` shows *no* non-hub devices at all) and doesn't come back on
its own — needs a physical unplug/replug of the USB-C cable. Root cause
unclassified — could be Asahi Linux's USB stack, a general Linux CDC-ACM
quirk with a device that resets itself repeatedly/quickly, or something
else; the board itself was confirmed running fine while the port was gone,
but that alone doesn't rule out a firmware-side trigger for the disconnect.

## Build system: use PlatformIO, but the project needs one tweak

`firmware/` is a native ESP-IDF project (root `CMakeLists.txt` calls
`$ENV{IDF_PATH}/tools/cmake/project.cmake` directly, has its own `main/`
component). PlatformIO's `framework = espidf` still drives this fine, but its
own pre-build sanity check requires a `src/` folder to exist regardless of the
real ESP-IDF component layout. Fix (already applied in `firmware/platformio.ini`):

```ini
[platformio]
src_dir = main
```

Build/flash from `firmware/`:

```bash
export PATH="$HOME/.local/bin:$PATH"
pio run                                                  # build
sg dialout -c "pio run --target upload --upload-port /dev/ttyACM0"  # flash
```

## firmware/ non-buildable-scaffold blocker — resolved

`firmware/main/main.c` used to assume a generic BSP abstraction
(`bsp/esp-bsp.h` with `bsp_display_start()`, `bsp_touch_init(NULL)`, etc.)
that doesn't exist — leftover from the original wrong-board assumption
(Touch-LCD-1.47 vs. the actual non-touch LCD-1.47 on hand, see "Hardware on
hand" above). The previous plan here — vendor Waveshare's `esp_bsp` /
`esp_lcd_jd9853` / `esp_lcd_touch_axs5106` components from their Touch-LCD
demo package and patch them for IDF 6.1.0 — targeted that wrong board and was
scrapped. ST7789 is a built-in ESP-IDF panel type and this board has no touch
controller, so **no BSP vendoring was needed at all**. See "Port status"
below for the completed port.

## Smoke-test #1 results (stock Waveshare Touch-LCD demo — wrong board, historical)

Flashed Waveshare's `03_lvgl_example` (JD9853 + AXS5106 touch, with the 3 IDF
6.1.0 patches above) as a hardware/toolchain sanity check. In hindsight this
was firmware for the **wrong board variant** (Touch-LCD-1.47, not the
non-touch LCD-1.47 actually on hand) — explains both failures below:

- Toolchain, USB link, and flashing all worked end-to-end; chip booted
  cleanly, no crash loop.
- **Touch "errors"**: `esp_lcd_touch_axs5106_read_data` spammed continuous
  I2C read errors. Root cause (confirmed 2026-09-19): not a driver bug —
  there is no AXS5106 touch chip on this board to respond.
- **Screen stayed blank** even at 100% backlight. Root cause (confirmed
  2026-09-19): wrong display driver (JD9853 instead of ST7789) *and* wrong
  SPI pins (MOSI/SCLK/RST/BL all different between the two board variants).

## Smoke-test #2 results (ST7789, correct pins — confirmed working)

Built a minimal diagnostic at `/tmp/st7789_smoketest` (not yet in this repo):
ESP-IDF's built-in `esp_lcd_new_panel_st7789()` (no vendored components
needed), correct pins (MOSI=6, SCLK=7, CS=14, DC=15, RST=21, BL=22 digital
high), 20MHz SPI clock, `x_gap=34,y_gap=0`, raw `esp_lcd_panel_draw_bitmap()`
fill of solid red — **bypassing LVGL entirely** to isolate the panel bring-up
from the UI layer. Flashed and visually confirmed: **solid red screen,
display confirmed working.** Boot log had zero errors at every init step.

## Port status (2026-09-19): code complete, build succeeds, one flash pending re-verification

The full LVGL+GPIO+Wi-Fi+MQTT port described in the old "Full port plan" is
**done**. All 7 of its steps landed: `firmware/main/main.c` now does the raw
ST7789 bring-up (from
[firmware/reference/st7789_confirmed_init.c](firmware/reference/st7789_confirmed_init.c))
wired into `lvgl_port_init()`/`lvgl_port_add_disp()`, no BSP/touch calls
anywhere; `firmware/main/CMakeLists.txt` and `idf_component.yml` no longer
reference a BSP; `ui_toggle_screen()` exists in `ui/src/ui.h`/`ui.c` and is
called from a BOOT-button (GPIO9) debounce in `firmware/main/gpio_ctrl.c`'s
existing poll task; `UI_INTEGRATION.md` is updated throughout for BOOT-button
nav. A static three-dimension review pass (API-vs-real-headers, pin
conflicts, cross-file contracts) came back with zero findings before the
first build attempt.

**Backlight** ended up PWM, not plain-GPIO: `main.c` drives GPIO22 via LEDC
(`LEDC_TIMER_10_BIT`, 5kHz) and is **hard-capped at 50% duty**
(`BACKLIGHT_MAX_PERCENT`) — full brightness is not to be used on this
display, so there's deliberately no code path that can exceed the cap.

### Build blockers found beyond the original plan (all fixed)

None of these were anticipated in the original port plan — they only showed
up once a real `pio run` was attempted for the first time (previous
smoke tests were minimal raw-`esp_lcd` programs, not the full app):

1. **`mqtt` isn't a bundled ESP-IDF component in IDF 6.1.0** — it's a managed
   component now. Fix: added `espressif/mqtt: "^1.0.0"` to
   `firmware/main/idf_component.yml`.
2. **`ui/fonts/*.c` (lv_font_conv output) fail to compile**: they guard their
   LVGL include with `#ifdef LV_LVGL_H_INCLUDE_SIMPLE` / else
   `#include "lvgl/lvgl.h"`, but that flag was never defined, and the `lvgl/`
   subpath doesn't resolve against this component-manager LVGL package
   layout (root dir is `lvgl__lvgl/`, not `lvgl/`). Fix: added
   `target_compile_definitions(${COMPONENT_LIB} PUBLIC LV_LVGL_H_INCLUDE_SIMPLE)`
   to `firmware/components/ui/CMakeLists.txt`.
3. **Firmware image (~1.42MB) overflows the default 1MB app partition.**
   PlatformIO picks the partition CSV via its own `board_build.partitions`
   key — it does **not** respect the ESP-IDF `PARTITION_TABLE_*` Kconfig
   choice in `sdkconfig.defaults` even though both were set. Fix: set
   `board_build.partitions = partitions_singleapp_large.csv` in
   `firmware/platformio.ini` (1500K app partition) *and*
   `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y` /
   `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y` in `sdkconfig.defaults`,
   matching the confirmed 8MB chip (see "Unresolved discrepancy" above).
4. **`firmware/credentials.h` (gitignored, Wi-Fi SSID/password) didn't
   exist** — only the `.template` did. Now created locally with real
   credentials; nothing to do here in future sessions unless the file is
   lost (it's gitignored, so it won't survive a fresh clone).

### First flash (confirmed): boots clean, UI renders, wrong orientation

First full flash came up clean — no crash loop, Wi-Fi/MQTT/GPIO/UI all
initialize. Visually confirmed on hardware: **the UI renders, but rotated —
the landscape-designed screens were being drawn into the panel's native
portrait framebuffer** (panel is physically 172 wide × 320 tall; the UI
layout in `ui/src/scr_home.c`/`scr_diag.c` assumes 320×172 landscape).

Fix applied in `firmware/main/main.c`:
- `LCD_H_RES`/`LCD_V_RES` swapped to 320/172 (these now describe the
  LVGL-logical/landscape resolution, not the physical panel).
- `lvgl_port_display_cfg_t.rotation.swap_xy = true`.
- `esp_lcd_panel_set_gap()` gap moved from `(34, 0)` to `(0, 34)` — the
  34px silicon offset is a fixed property of the panel's physical short
  axis; with `swap_xy` on, that axis is addressed via RASET (y) instead of
  CASET (x), confirmed by reading `esp_lcd_panel_st7789.c`'s
  `draw_bitmap`/gap-application code directly.

## Root cause found (2026-09-19): the "USB hot-plug quirk" was a GPIO pin conflict, not a Linux/Asahi issue

The recurring `/dev/ttyACM0` dropout blamed earlier on "Asahi Linux's USB
stack" had a real, confirmed cause: **`gpio_ctrl.c`'s 3-position toggle
switch inputs were assigned to GPIO12/GPIO13**, which are this chip's
**native USB Serial/JTAG D-/D+ pins** (confirmed against Espressif's
ESP32-C6 datasheet — enabled by default, 40 mA drive strength, distinct from
the other pins already checked against docs.waveshare.com). `gpio_ctrl_init()`
reconfigured them as plain digital inputs with pull-down on every boot,
which fights the USB PHY signaling on those same physical pads — this is
almost certainly why the serial link kept vanishing after every reset,
independent of anything Linux-side.

**Fix**: toggle switch inputs moved to **GPIO18/GPIO19** in
`firmware/main/gpio_ctrl.c` (confirmed safe: not strapping pins, not used by
this board's display/LED/TF-card/USB, and not part of the "GPIO10/11 don't
exist on SiP-flash variants" caveat for this chip). Caught before the
toggle switch was physically wired into the fixture, so no rework needed.

**Confirmed fixed**: after this change, a plain RST button press left
`/dev/ttyACM0` enumerated and stable — previously *every* RST press (and
most flashes) dropped the port entirely. `lsusb` before the fix: port gone,
no non-hub devices at all. After: `Bus 001 Device XXX: ID 303a:1001
Espressif USB JTAG/serial debug unit` stays present and stable across
resets.

**Toolchain note**: getting `/dev/ttyACM0` to reappear once it *has*
dropped still requires **holding BOOT+RST, plugging in (or reconnecting) the
USB-C cable, then releasing RST first and BOOT after** — a plain
unplug/replug is not reliable on this machine. Also: opening the port with
`pyserial` and explicitly setting `.rts`/`.dtr` (even to `False`) reliably
resets the chip into ROM download mode (`waiting for download`) — this is
the native USB-Serial-JTAG's built-in auto-reset-to-bootloader watching for
control-line transitions, same trick `esptool` uses, and it fires for *any*
client that touches those lines, not just esptool. Plain `cat
/dev/ttyACM0` (no control-line manipulation) does **not** trigger it and is
the safe way to capture a live boot/console log.

## Root cause found (2026-09-19): mirrored display was a transpose, not a rotation

`swap_xy = true` alone (MADCTL `MV` bit) is a diagonal transpose/reflection,
not a 90° rotation — confirmed by reading `panel_st7789_mirror()` /
`panel_st7789_swap_xy()` in `esp_lcd_panel_st7789.c` (`MX`=mirror_x,
`MY`=mirror_y, `MV`=swap_xy, matching standard ST7789 MADCTL semantics). A
transpose alone produces exactly the "correct up/down, but mirrored
left-right like viewing from the back" symptom seen on hardware. **Fix**:
added `mirror_x = true` in `firmware/main/main.c`'s `lvgl_display_init()`
(now `swap_xy=true, mirror_x=true, mirror_y=false`) — confirmed correct on
hardware, landscape and right-side-up with no mirroring.

## Root cause found (2026-09-19): BOOT button crashed the firmware on every press

Confirmed via a captured boot/panic log (see toolchain note above for how):
pressing BOOT correctly logged `BOOT button pressed, screen toggled`, then
immediately hit a **Guru Meditation "Stack protection fault"** and rebooted
— the `gpio_poll` FreeRTOS task's stack (3072 B) wasn't enough headroom for
`ui_toggle_screen()`'s call into `lv_screen_load_anim()` on top of the
poll loop's own frame. The panic dump's task name field was itself
corrupted (printed as garbage bytes), a classic stack-overflow signature.
This is *why* the BOOT button looked like it did "nothing" on a single
press (crash+reboot happens fast enough to be easy to miss) and why a fast
double-press produced a visible white flash + Wi-Fi reconnect (that was the
reboot, not a hardware BOOT/RST coupling quirk as first suspected).

**Fix**: bumped the task's stack from 3072 to 6144 bytes in
`firmware/main/gpio_ctrl.c`'s `xTaskCreate()` call. Confirmed via serial log
that the crash/panic no longer occurs — the toggle now completes without a
reboot (USB device stayed enumerated across repeated presses, whereas every
prior crash caused a fresh `lsusb` re-enumeration).

## Root cause found (2026-09-19): MQTT connection was rejected, not misconfigured

Boot log showed `mqtt_client: Connection refused, not authorized` — a
distinct CONNACK code (`MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED`, 0x5) from
"bad username or password" (0x4). The firmware never sent *any*
username/password at all (`esp_mqtt_client_config_t` only set
`broker.address.uri` and `credentials.client_id`), so the broker was
rejecting the anonymous/unauthenticated connection outright.

**Fix**: added `MQTT_USERNAME`/`MQTT_PASSWORD` to `firmware/credentials.h`
(gitignored, alongside `WIFI_SSID`/`WIFI_PASSWORD`) and
`firmware/credentials.h.template`, wired into `firmware/main/mqtt.c`'s
`esp_mqtt_client_config_t` via `.credentials.username` and
`.credentials.authentication.password`. Broker-side ACL/user was set up to
match by the user. **Confirmed connected**: boot log now shows `mqtt:
Connected to broker` and `mqtt: HA discovery published`; Wi-Fi and MQTT
status icons on the home screen show green on hardware.

Full topic/credential surface for reference (see `firmware/main/mqtt.c`):
broker `mqtt://192.168.2.9:1883`, client id `speedo-bench`, LWT
`speedo-bench/state/online` = `offline`. Publishes:
`speedo-bench/state/online`, `speedo-bench/cmd/source`,
`speedo-bench/cmd/power`, `speedo/target`, and four
`homeassistant/.../config` discovery topics. Subscribes: `speedo/speed`,
`speedo-bench/cfg/units`.

## Displayed speed temporarily sourced from speedo/target, not speedo/speed (2026-09-19)

Verified the display pipeline is correct by subscribing to the broker
directly with the same credentials the firmware uses (`python3` +
`paho-mqtt`, see `/tmp/mqtt_watch.py`, not checked into this repo) and
comparing live traffic against the on-device numeral — they matched
exactly. The user expected ~20 but both the wire and the display showed
26–30, meaning **the upstream controller firmware that publishes
`speedo/speed` is producing incorrect values** — this is a bug outside this
repo, not in the ESP32 display firmware.

**Stopgap** (`firmware/main/mqtt.c`): added `TOPIC_SPEED_DISPLAY`, currently
aliased to `TOPIC_SPEED_ZERO` (`speedo/target`, the commanded value) instead
of `TOPIC_STATE_SPEED` (`speedo/speed`, the buggy actual-value feed). The
Home Assistant "Speed" sensor discovery config is untouched and still
tracks `speedo/speed`. **TODO: revert `TOPIC_SPEED_DISPLAY` back to
`TOPIC_STATE_SPEED`** once the upstream controller's speed calculation bug
is fixed — there's a comment marking this at the `#define` site in
`mqtt.c`.

## Open items (as of 2026-09-19, next session pick up here)

1. **Screen-toggle visual tearing.** BOOT button no longer crashes (see
   above), but on hardware the transition shows a **diagonal screen-tearing
   artifact** during the fade, and the Wi-Fi/MQTT status icons take a few
   seconds to return to green afterward (looks like a brief real
   reconnect, not just a repaint). Not yet root-caused — likely an LVGL
   double-buffer/redraw timing issue or something in how `scr_diag.c`
   initializes/refreshes its icon state on load, but unconfirmed. Not a
   crash and not blocking; needs a fresh investigation pass.
2. **Confirm the 3-position toggle switch + relay interlock** behave
   correctly on real wiring. Inputs were moved to **GPIO18/GPIO19** this
   session (see above) — **not yet wired into the physical fixture**, so
   this is still fully untested on hardware. Wire it up and re-verify.
