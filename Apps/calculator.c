#ifdef PORTABLE_NOVA_UI
#include "RiscRuntimeV1.h"
#include "calculator_nova.inc"
#else
#include "T5AppApi.h"
#include "calculator_core.h"
#include "daily_draw.h"
#include <stddef.h>

#define CALCULATOR_KEYS 19
#define CALCULATOR_GRID_TOP 88

typedef struct { int x, y, width, height; } calculator_rect;
static const char calculator_keys[CALCULATOR_KEYS] = {
    'C', 'S', 'B', '/', '7', '8', '9', '*', '4', '5', '6', '-',
    '1', '2', '3', '+', '0', '.', '='
};
static const char *const calculator_labels[CALCULATOR_KEYS] = {
    "C", "+/-", "DEL", "/", "7", "8", "9", "x", "4", "5", "6", "-",
    "1", "2", "3", "+", "0", ".", "="
};
static const t5_app_api_v1 *calculator_api;
static calculator calculator_state;
static int calculator_width, calculator_height, calculator_selected, calculator_pressed;

static calculator_rect calculator_key_rect(int index) {
    int row = index / 4, column = index % 4;
    int usable_width = calculator_width - 8;
    int usable_height = calculator_height - CALCULATOR_GRID_TOP - 4;
    int left = 4 + column * usable_width / 4;
    int right = 4 + (index == 18 ? 4 : column + 1) * usable_width / 4;
    int top = CALCULATOR_GRID_TOP + row * usable_height / 5;
    int bottom = CALCULATOR_GRID_TOP + (row + 1) * usable_height / 5;
    return (calculator_rect){left + 2, top + 2, right - left - 4, bottom - top - 4};
}

static bool calculator_inside(calculator_rect rect, int x, int y) {
    return x >= rect.x && y >= rect.y && x - rect.x < rect.width && y - rect.y < rect.height;
}

static int calculator_hit(int x, int y) {
    if (x < 0 || y < 0 || x >= calculator_width || y >= calculator_height) return -1;
    for (int i = 0; i < CALCULATOR_KEYS; ++i)
        if (calculator_inside(calculator_key_rect(i), x, y)) return i;
    return -1;
}

static void calculator_border(calculator_rect rect, bool black) {
    calculator_api->fill_rect(rect.x, rect.y, rect.width, 1, black);
    calculator_api->fill_rect(rect.x, rect.y + rect.height - 1, rect.width, 1, black);
    calculator_api->fill_rect(rect.x, rect.y, 1, rect.height, black);
    calculator_api->fill_rect(rect.x + rect.width - 1, rect.y, 1, rect.height, black);
}

static void calculator_render(void) {
    calculator_api->clear();
    daily_draw_text(calculator_api, 8, 15, "BACK", 1);
    daily_draw_text(calculator_api, 65, 11, "CALCULATOR", 2);
    calculator_api->fill_rect(4, 35, calculator_width - 8, 1, true);
    const char *display = calculator_state.error == CALC_DIV_ZERO ? "DIV BY ZERO" :
                          calculator_state.error == CALC_OVERFLOW ? "OVERFLOW" : calculator_state.text;
    int scale = 4;
    while (scale > 1 && daily_draw_width(display, scale) > calculator_width - 16) --scale;
    int width = daily_draw_width(display, scale);
    daily_draw_text(calculator_api, calculator_width - 8 - width,
                    42 + (28 - 7 * scale) / 2, display, scale);
    daily_draw_text(calculator_api, 8, 77, calculator_state.error ? "C OR DIGIT TO RESET" : "6 DP", 1);
    if (!calculator_state.error && calculator_state.pending) {
        char pending[] = {calculator_state.pending, 0};
        daily_draw_text(calculator_api, calculator_width - 16, 77, pending, 1);
    }
    for (int i = 0; i < CALCULATOR_KEYS; ++i) {
        calculator_rect rect = calculator_key_rect(i);
        bool pressed = i == calculator_pressed;
        if (pressed) calculator_api->fill_rect(rect.x, rect.y, rect.width, rect.height, true);
        else calculator_border(rect, true);
        if (i == calculator_selected) {
            calculator_rect inset = {rect.x + 2, rect.y + 2, rect.width - 4, rect.height - 4};
            calculator_border(inset, !pressed);
        }
        int key_scale = rect.height >= 32 ? 3 : 2;
        while (key_scale > 1 && daily_draw_width(calculator_labels[i], key_scale) > rect.width - 8) --key_scale;
        int label_width = daily_draw_width(calculator_labels[i], key_scale);
        daily_draw_text_tone(calculator_api, rect.x + (rect.width - label_width) / 2,
                             rect.y + (rect.height - 7 * key_scale) / 2,
                             calculator_labels[i], key_scale, !pressed);
    }
    calculator_api->present(false);
}

static bool calculator_has_contact(void) {
    return calculator_api->struct_size >= offsetof(t5_app_api_v1, touch_contact) +
                                           sizeof(calculator_api->touch_contact) && calculator_api->touch_contact;
}

void app_main(void) {
    calculator_api = t5_app_get_api(T5_APP_ABI_VERSION);
    if (!calculator_api || calculator_api->abi_version != T5_APP_ABI_VERSION ||
        calculator_api->struct_size < offsetof(t5_app_api_v1, poll) + sizeof(calculator_api->poll) ||
        !calculator_api->screen_width || !calculator_api->screen_height || !calculator_api->clear ||
        !calculator_api->fill_rect || !calculator_api->present || !calculator_api->poll) return;
    calculator_width = calculator_api->screen_width();
    calculator_height = calculator_api->screen_height();
    /* Explicit bounds keep all layout arithmetic and int16 touch coordinates safe.
     * A 240px watch retains five 25px-high buttons with four-pixel gutters. */
    if (calculator_width < 200 || calculator_height < 240 ||
        calculator_width > 4096 || calculator_height > 4096) return;
    calc_reset(&calculator_state);
    calculator_selected = 0;
    calculator_pressed = -1;
    bool contact_was_down = false, contact_cancelled = false;
    calculator_render();
    for (;;) {
        t5_app_input_t input = {0};
        if (!calculator_api->poll(&input, 30) || input.exit_requested ||
            (input.buttons & T5_APP_BUTTON_BACK)) break;
        if (input.tapped && input.touch_x >= 0 && input.touch_x < 56 &&
            input.touch_y >= 0 && input.touch_y < 40) break;
        bool redraw = false;
        if (calculator_has_contact()) {
            t5_app_contact_t contact = {0};
            bool valid = calculator_api->touch_contact(&contact);
            int pressed = -1;
            if (valid && contact.down) {
                int hit = calculator_hit(contact.x, contact.y);
                if (!contact_was_down) contact_cancelled = hit < 0;
                else if (hit != calculator_pressed) contact_cancelled = true;
                if (!contact_cancelled) pressed = hit;
            } else contact_cancelled = false;
            contact_was_down = valid && contact.down;
            if (pressed != calculator_pressed) { calculator_pressed = pressed; redraw = true; }
        }
        if (input.buttons & (T5_APP_BUTTON_LEFT | T5_APP_BUTTON_RIGHT | T5_APP_BUTTON_UP | T5_APP_BUTTON_DOWN)) {
            int selected = calculator_selected;
            if (input.buttons & T5_APP_BUTTON_LEFT) selected = (selected + CALCULATOR_KEYS - 1) % CALCULATOR_KEYS;
            else if (input.buttons & T5_APP_BUTTON_RIGHT) selected = (selected + 1) % CALCULATOR_KEYS;
            else if (input.buttons & T5_APP_BUTTON_UP) selected = selected < 4 ? 16 + selected : selected - 4;
            else if (input.buttons & T5_APP_BUTTON_DOWN) selected = selected >= 16 ? selected - 16 : selected + 4;
            calculator_selected = selected >= CALCULATOR_KEYS ? 18 : selected;
            redraw = true;
        }
        int activate = input.tapped ? calculator_hit(input.touch_x, input.touch_y) :
                       (input.buttons & T5_APP_BUTTON_CONFIRM) ? calculator_selected : -1;
        if (activate >= 0) {
            calculator_selected = activate;
            calc_key(&calculator_state, calculator_keys[activate]);
            redraw = true;
        }
        if (redraw) calculator_render();
    }
}

#endif
