#pragma once

#include "lvgl.h"

/*
 * §4 — Status bar icons.
 *
 * Real assets: convert Tabler outline SVGs to A8 (alpha-only) images:
 *
 *   python LVGLImage.py --color-format A8 --output-format C \
 *     --output-file ./ui/images \
 *     wifi.svg broadcast.svg
 *
 * Rename generated symbols to img_wifi / img_broadcast to match these
 * declarations, or adjust the LV_IMAGE_DECLARE names to match the generator's
 * output. Place the resulting .c files in ui/images/ and add them to
 * user_config.cmake.
 *
 * The placeholder images.c provides 1×1 transparent stubs so the project
 * builds immediately. Replace with real assets before flashing.
 */

LV_IMAGE_DECLARE(img_wifi);
LV_IMAGE_DECLARE(img_broadcast);
