# SpeedRack Display — Project Notes

Speedometer test fixture: ESP32-C6 display device (Waveshare
ESP32-C6-LCD-1.47, **non-touch**) showing live speed + diagnostics, driven by
GPIO toggles/relays and MQTT. See [UI_INTEGRATION.md](UI_INTEGRATION.md) for
the UI/firmware contract (screen API, MQTT topics, GPIO truth table) — note
that doc still assumes touch navigation and needs updating (see below).

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
    `LCD_RST` = GPIO21, `LCD_BL` = GPIO22 (backlight control method — PWM vs.
    plain GPIO — not stated on the page, worth checking empirically).
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
- **UI contract conflict**: [UI_INTEGRATION.md](UI_INTEGRATION.md) assumes
  touch navigation ("tap anywhere to switch screens", `bsp_touch_init(NULL)`,
  AXS15206). That can't work on this hardware. Decided direction (not yet
  implemented): use the onboard **BOOT button (GPIO9)** to cycle between
  home/diagnostics screens instead of touch. `UI_INTEGRATION.md` and `ui/`
  still need updating to reflect this — not done yet.

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

## firmware/ is currently a non-buildable scaffold (historical wrong-board plan superseded)

`firmware/main/main.c` still assumes a generic BSP abstraction (`bsp/esp-bsp.h`
with `bsp_display_start()`, `bsp_touch_init(NULL)`, etc.) that doesn't exist —
leftover from the original wrong-board assumption (Touch-LCD-1.47 vs. the
actual non-touch LCD-1.47 on hand, see "Hardware on hand" above). The
previous plan here — vendor Waveshare's `esp_bsp` / `esp_lcd_jd9853` /
`esp_lcd_touch_axs5106` components from their Touch-LCD demo package and patch
them for IDF 6.1.0 — targeted that wrong board and is now obsolete. ST7789 is
a built-in ESP-IDF panel type and this board has no touch controller, so
**no BSP vendoring is needed at all**.

See "Full port plan (next session: do this)" below for the current,
confirmed-correct plan, based on the working ST7789 smoke test.

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

## Next steps (not yet done)

1. ~~Root-cause the blank-screen issue~~ — done, see above.
2. ~~Fix or work around the touch I2C error loop~~ — done: there's no touch
   hardware, so the fix is to not initialize a touch driver at all.
3. Full firmware port — see detailed plan below. Not started yet.

## Full port plan (next session: do this)

The confirmed-working ST7789 init sequence is saved in this repo at
[firmware/reference/st7789_confirmed_init.c](firmware/reference/st7789_confirmed_init.c)
(copied verbatim from the working `/tmp/st7789_smoketest` — that `/tmp` copy
may not survive a reboot, this in-repo copy is the durable source of truth).
It's a standalone raw `esp_lcd` program, not yet wired to LVGL — that wiring
is most of what's left. Concrete steps, file by file:

**1. `firmware/main/main.c`** — currently calls `bsp_display_start()`,
`bsp_display_set_brightness()`, `bsp_touch_init(NULL)` from a nonexistent
`bsp/esp-bsp.h` (leftover from the wrong-board assumption). Replace with:
   - The SPI bus init + panel IO + `esp_lcd_new_panel_st7789()` + reset/init/
     invert/gap/mirror/disp_on sequence from the reference file (pins:
     MOSI=6, SCLK=7, CS=14, DC=15, RST=21, BL=22 as plain GPIO output high;
     20MHz SPI clock was used for the safe first test — can likely go higher,
     untested how high).
   - Then, instead of the reference file's raw `draw_bitmap` test loop, call
     `lvgl_port_init()` + `lvgl_port_add_disp()` (pattern already visible in
     `/tmp/ws_smoketest/main/main.c`'s `app_lvgl_init()`, if that still
     exists — it also survived only in `/tmp`) passing the `io_handle` /
     `panel_handle` from the ST7789 init. hres=172, vres=320, no
     swap_xy/mirror (rotation 0), gap already applied via
     `esp_lcd_panel_set_gap(panel_handle, 34, 0)` before adding the display.
   - **Do not call `lvgl_port_add_touch()`** — no touch hardware exists.
   - No `bsp_touch_init` call at all.
   - Then `ui_init()` under `lvgl_port_lock()/unlock()` as it already does.

**2. `firmware/main/CMakeLists.txt`** — `REQUIRES` currently has `ui`,
`esp_wifi`, `esp_event`, `esp_netif`, `mqtt`, `driver`, `esp_lvgl_port`,
`nvs_flash`, `esp_timer`, `log`. Add: `esp_lcd`, `esp_driver_gpio`,
`esp_driver_spi` (needed for the panel/SPI bus calls now living directly in
`main.c` instead of a vendored BSP component).

**3. `firmware/main/idf_component.yml`** — delete the whole "BSP NOTE" comment
block; it's now false. No BSP dependency is needed at all — ST7789 is a
built-in ESP-IDF panel type. Keep the `lvgl/lvgl` and `espressif/esp_lvgl_port`
deps as-is.

**4. Backlight**: reference file drives GPIO22 as plain digital output HIGH.
Waveshare's docs don't say if it's PWM-capable; plain digital worked for the
smoke test. If dimming is wanted later, try LEDC PWM on GPIO22 (pattern in
old `/tmp/ws_smoketest/components/esp_bsp/bsp_display.c`'s
`bsp_display_brightness_init()`, also only in `/tmp` — not required for a
working display, just for brightness control).

**5. `ui/src/ui.c` + `ui/src/ui.h`** — screen switching is currently *only*
reachable via LVGL click events on the screen objects themselves
(`on_home_click`/`on_diag_click`, both `static`, wired to `LV_EVENT_CLICKED`
in `ui_init()` at [ui/src/ui.c:13-23](ui/src/ui.c:13-23)). There is **no
public function to switch screens externally** — this must be added. Add
something like `void ui_toggle_screen(void)` to `ui.h`, implemented in `ui.c`
by extracting the shared logic from `on_home_click`/`on_diag_click` (need to
track which screen is currently active, e.g. a static `bool on_diag` flag, or
check `lv_screen_active()` against `scr_home`/`scr_diag`) so it can be called
from a GPIO handler, not just an LVGL click event.

**6. BOOT button wiring** — GPIO9, active-low (internal pull-up, pressed =
LOW), standard ESP32 BOOT button behavior. `firmware/main/gpio_ctrl.c`
already has an established 10ms-poll/50ms-debounce pattern for the existing
toggle switches (see `gpio_ctrl.h`'s doc comment) — extend that same task (or
add a parallel one) to poll GPIO9, debounce, and call `ui_toggle_screen()`
(guarded by `lvgl_port_lock()/unlock()`) on a falling-edge press.

**7. `UI_INTEGRATION.md`** needs updating in several places once the above is
done:
   - Quick orientation table: `Touch | AXS15206 capacitive, via BSP` → replace
     with something like `Nav | BOOT button (GPIO9), no touch hardware`.
   - The `Initialization sequence` code sample: remove `bsp_touch_init(NULL)`.
   - The `Touch navigation` section: rewrite for BOOT-button nav, document
     `ui_toggle_screen()` as the new public function once added.
   - `App-layer punch list`: add "BOOT button debounce + screen-toggle call"
     alongside the existing toggle-switch debounce item.

**8. Build/flash/verify**: same commands as always
(`pio run`, `sg dialout -c "pio run --target upload --upload-port /dev/ttyACM0"`).
Confirm visually that `ui_init()`'s home screen renders correctly (not just a
solid color like the diagnostic test), and that pressing BOOT switches to the
diagnostics screen and back.
