# UI Integration Reference — Speedometer Test Fixture

This document covers everything the firmware layer needs to know to drive the LVGL display
UI defined in `ui/`. The UI handles presentation and touch only — GPIO, relay control,
MQTT client, and Wi-Fi provisioning are application-layer responsibilities.

---

## Quick orientation

| Item | Value |
|---|---|
| Hardware | Waveshare ESP32-C6-Touch-LCD-1.47 |
| Display | 320 × 172 px landscape, JD9853, RGB565 |
| Touch | AXS15206 capacitive, via BSP |
| LVGL version | 9.5 |
| Public header | `ui/src/ui.h` |
| Entry point | `ui_init()` |

---

## Build integration (ESP-IDF / PlatformIO)

The `ui/` directory is a self-contained CMake library target (`lib-ui`). Include it as
an ESP-IDF component by symlinking or copying `ui/` into your project's `components/`
directory and adding an `idf_component_register` wrapper, **or** include it directly via
`add_subdirectory` and link against it.

### Minimal `CMakeLists.txt` wrapper (components/ui/CMakeLists.txt)

```cmake
idf_component_register(
    SRCS
        # Hand-written UI sources
        "src/ui.c"
        "src/scr_home.c"
        "src/scr_diag.c"
        # IBM Plex Mono Bold fonts (lv_font_conv output)
        "fonts/font_mono_12.c"
        "fonts/font_mono_14.c"
        "fonts/font_mono_68.c"
        # Tabler icon images (LVGLImage.py A8 output)
        "images/img_wifi.c"
        "images/img_broadcast.c"
    INCLUDE_DIRS
        "src"
        "fonts"
        "images"
    REQUIRES
        lvgl
        esp_lcd          # or your BSP component name
)
```

> **PlatformIO note:** If using the PlatformIO ESP-IDF framework, place the `ui/`
> directory under `components/ui/` and add `"components/ui"` to `board_build.cmake_extra_args`
> or use `extra_scripts` to call `add_subdirectory`. The `user_config.cmake` /
> `CMakeLists.txt` in `ui/` is for the standalone LVGL Editor build; for ESP-IDF,
> the `idf_component_register` wrapper above takes precedence.

### Required LVGL config (`lv_conf.h`)

```c
#define LV_COLOR_DEPTH          16      /* RGB565 */
#define LV_DPI_DEF              250     /* 250 ppi at 1.47" */
#define LV_USE_DRAW_SW_COMPLEX_GRADIENTS 0
/* Fonts: built-ins NOT required (real IBM Plex Mono assets are compiled in).
 * If you ever set USE_PLACEHOLDER_FONTS=1, also enable: */
/* #define LV_FONT_MONTSERRAT_12  1 */
/* #define LV_FONT_MONTSERRAT_14  1 */
/* #define LV_FONT_MONTSERRAT_48  1 */
```

Do **not** call `lv_theme_default_init()`. `ui_init()` nulls out the active theme
so built-in color choices can't override the explicit per-widget styles.

---

## Initialization sequence

Call `ui_init()` **after** the BSP and LVGL are fully initialized:

```c
#include "bsp/esp-bsp.h"   // Waveshare BSP
#include "ui.h"

void app_main(void)
{
    /* 1. BSP: bring up display + touch */
    bsp_display_start();
    bsp_display_set_brightness(100);   // §1: backlight at 100%
    bsp_touch_init(NULL);

    /* 2. LVGL: the BSP's display driver and indev are now registered.
     *    If using lv_port_esp32 or esp_lvgl_port, call their init here. */

    /* 3. UI: build both screens, wire touch events, apply boot state */
    ui_init();

    /* 4. Start LVGL task (if not started by the port already) */
    // lv_task_handler() must be called periodically from a dedicated task
}
```

`ui_init()` applies the boot state automatically:
- Source → STANDBY, speed area dimmed
- Power → OFF
- Wi-Fi → disconnected (icons dim)
- MQTT → disconnected (icons dim)
- Units → MPH
- Speed → 0

---

## Thread safety

**LVGL is not thread-safe.** All `ui_set_*()` / `ui_diag_set_*()` calls must be made
from the same task that calls `lv_task_handler()`, **or** guarded with the LVGL mutex:

```c
// From any task:
if (lvgl_lock(-1)) {           // esp_lvgl_port helper, or your own mutex
    ui_set_speed(new_speed);
    lvgl_unlock();
}
```

If using `esp_lvgl_port` (recommended for ESP-IDF + LVGL 9):

```c
#include "esp_lvgl_port.h"

lvgl_port_lock(0);
ui_set_mqtt_connected(true);
lvgl_port_unlock();
```

---

## Public API (`ui/src/ui.h`)

```c
#include "ui.h"
```

### Enumerations

```c
typedef enum { SRC_SIG_GEN, SRC_MOTOR, SRC_STANDBY } src_t;
typedef enum { UNITS_MPH, UNITS_KPH }                units_t;
```

### Functions

#### `void ui_init(void)`
Build both screens, register touch handlers, load home screen, apply boot state.
Call exactly once after BSP + LVGL init.

---

#### `void ui_set_speed(int speed)`
Update the large speed numeral. Integer only, no padding, no leading zeros.
Caller does no unit conversion — display shows exactly what the source publishes.

```c
ui_set_speed(0);    // shows "0"
ui_set_speed(87);   // shows "87"
ui_set_speed(200);  // shows "200"
```

---

#### `void ui_set_units(units_t u)`
Flip the unit label next to the numeral.

| `u` | Label |
|---|---|
| `UNITS_MPH` | "MPH" |
| `UNITS_KPH` | "KPH" |

---

#### `void ui_set_source(src_t s)`
Drive the SOURCE indicator dot + label AND dim/undim the speed area as a side-effect.
The caller does not need to manage speed area dimming separately.

| `s` | Dot color | Label | Speed area |
|---|---|---|---|
| `SRC_SIG_GEN` | Green | "SIG GEN" | bright |
| `SRC_MOTOR` | Green | "MOTOR" | bright |
| `SRC_STANDBY` | Amber | "STANDBY" | dimmed |

---

#### `void ui_set_power(bool on)`
Drive the POWER indicator dot + label.

| `on` | Dot color | Label |
|---|---|---|
| `true` | Green | "ENABLED" |
| `false` | Red | "OFF" |

---

#### `void ui_set_wifi_connected(bool conn)`
Recolor the Wi-Fi icon on **both** screens simultaneously.
- `true` → phosphor green (connected)
- `false` → dark muted color (disconnected)

---

#### `void ui_set_mqtt_connected(bool conn)`
Same as above for the MQTT/broadcast icon on both screens.

---

#### `void ui_diag_set_ip(const char *ip)`
Update the IP row value on the diagnostics screen. Example: `"192.168.2.144"`.

#### `void ui_diag_set_hostname(const char *hn)`
Update the HOST row. Example: `"speedo-bench"`.

#### `void ui_diag_set_mqtt_broker(const char *brokerport)`
Update the MQTT row. Example: `"192.168.2.9:1883"`.

#### `void ui_diag_set_rssi(int dbm)`
Update the RSSI row. Formats as `"-52 dBm"` automatically.
Pass the raw dBm integer (typically negative).

#### `void ui_diag_set_fw(const char *ver)`
Update the FW row. Example: `"v0.1.0"`.

---

## MQTT topic → UI function mapping

### Subscribed by the device (react on receive)

| Topic | Payload | Call |
|---|---|---|
| `speedo-bench/state/speed` | integer string, e.g. `"87"` | `ui_set_speed(atoi(payload))` |
| `speedo-bench/cfg/units` | `"mph"` \| `"kph"` | `ui_set_units(UNITS_MPH or UNITS_KPH)` |

### Published by the device (toggle GPIO → publish → no UI call needed)

| Topic | Payload | Trigger |
|---|---|---|
| `speedo-bench/cmd/source` | `"sig_gen"` \| `"motor"` \| `"standby"` | 3-position toggle change |
| `speedo-bench/cmd/power` | `"on"` \| `"off"` | power toggle change |
| `speedo-bench/cmd/speed` | `"0"` | toggle moves to center (motor zero) |
| `speedo-bench/state/online` | `"online"` (LWT: `"offline"`) | connect / disconnect |

### Connection state callbacks → UI

```c
// In your MQTT event handler:
case MQTT_EVENT_CONNECTED:
    ui_set_mqtt_connected(true);
    // publish LWT / subscribe to topics
    break;
case MQTT_EVENT_DISCONNECTED:
    ui_set_mqtt_connected(false);
    break;

// In your Wi-Fi event handler:
case IP_EVENT_STA_GOT_IP:
    ui_set_wifi_connected(true);
    ui_diag_set_ip(ip_str);
    break;
case WIFI_EVENT_STA_DISCONNECTED:
    ui_set_wifi_connected(false);
    break;
```

### Diagnostics population (call once after connection)

```c
ui_diag_set_hostname("speedo-bench");
ui_diag_set_mqtt_broker("192.168.2.9:1883");
ui_diag_set_ip("192.168.2.144");    // from got-ip event
ui_diag_set_rssi(-52);              // from esp_wifi_sta_get_rssi()
ui_diag_set_fw("v0.1.0");           // from your version constant
```

---

## Toggle switch wiring (app layer, not UI)

The 3-position SPDT center-off toggle requires two GPIO inputs:

```
Toggle position → GPIO A | GPIO B → src_t         → publish
SIG GEN         →   0    |   1   → SRC_SIG_GEN   → "sig_gen"
CENTER (standby)→   0    |   0   → SRC_STANDBY   → "standby" + speed "0"
MOTOR           →   1    |   0   → SRC_MOTOR     → "motor"
```

After debounce, call `ui_set_source(resolved_src)` **and** publish the command topic.
When moving to center, also publish `speedo-bench/cmd/speed` = `"0"`.

The POWER toggle is a single GPIO (on/off). After debounce, call `ui_set_power(on)`
and publish `speedo-bench/cmd/power`.

**Interlock:** the power relay must only be energized when the signal relay is also
energized. Enforce this in the GPIO write logic, not in the UI.

---

## Touch navigation

Tap anywhere on either screen to switch screens. No swipe, no long-press.
200 ms FADE_IN transition. This is fully handled inside `ui_init()` — the firmware
layer does not need to wire any touch callbacks.

---

## Asset details

### Fonts (`ui/fonts/`)

| File | Size | Glyphs | Use |
|---|---|---|---|
| `font_mono_12.c` | ~19 KB | 0x20–0x5A (A–Z, 0–9, space) | captions, labels |
| `font_mono_14.c` | ~35 KB | 0x20–0x7A (full printable ASCII) | values, diagnostic data |
| `font_mono_68.c` | ~58 KB | 0x30–0x39 (digits only) | large speed numeral |

Typeface: IBM Plex Mono Bold, 4 bpp antialiased. Total flash cost ≈ 112 KB.

`fonts/fonts.h` controls the active font set. Default is `USE_PLACEHOLDER_FONTS 0`
(real assets). Set to `1` at compile time to fall back to LVGL built-in Montserrat
during bringup (numeral offsets will look wrong — expected).

### Icons (`ui/images/`)

| File | Size | Source | Format |
|---|---|---|---|
| `img_wifi.c` | ~1.8 KB | Tabler `wifi` outline | 14×14 px, A8 alpha-only |
| `img_broadcast.c` | ~1.8 KB | Tabler `broadcast` outline | 14×14 px, A8 alpha-only |

Icons are recolored at runtime via `lv_obj_set_style_image_recolor()`. The C arrays
contain only alpha data — no RGB. The UI applies:
- Connected → `COL_PRIMARY` (phosphor green `#4ADE80`)
- Disconnected → `COL_DIM_ICON` (dark muted `#2D3530`)

---

## App-layer punch list (out of UI scope)

These items are flagged in the spec as firmware responsibilities:

- [ ] 3-position toggle debounce and GPIO read (two GPIOs for SPDT center-off)
- [ ] Two relay GPIO outputs with interlock (power relay only when signal relay energized)
- [ ] MQTT client connect / reconnect / LWT (`speedo-bench/state/online`)
- [ ] Wi-Fi provisioning (hard-coded SSID/pass, or ESP-IDF Improv / BLE provisioning)
- [ ] TF card — logging or leave unused
- [ ] QMI8658 IMU — leave uninitialized (unused for this fixture)

---

## File structure reference

```
ui/
├── src/
│   ├── ui.h          ← include this in firmware code
│   ├── ui.c          ← ui_init() + all ui_set_*() bodies
│   ├── scr_home.h/c  ← home screen build + widget pointer externs
│   ├── scr_diag.h/c  ← diagnostic screen build + widget pointer externs
│   └── theme.h       ← COL_* color macros (do not use inline hex elsewhere)
├── fonts/
│   ├── fonts.h               ← font macro/extern declarations
│   ├── font_mono_12.c        ← generated by lv_font_conv
│   ├── font_mono_14.c
│   └── font_mono_68.c
└── images/
    ├── images.h              ← LV_IMAGE_DECLARE for both icons
    ├── img_wifi.c            ← generated by LVGLImage.py
    └── img_broadcast.c
```

The firmware only ever needs to `#include "ui.h"`. All other headers are internal
to the UI module.
