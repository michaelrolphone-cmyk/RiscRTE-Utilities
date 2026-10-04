#ifndef DAILY_DRAW_H
#define DAILY_DRAW_H
/* Small source-owned 5x7 text primitives for numeric displays. No font service,
 * heap, icons or runtime ABI extension. Coordinates are clipped by fill_rect. */
#include "T5AppApi.h"
#include <limits.h>
#include <stdint.h>

static inline const uint8_t *daily_draw_glyph(char c) {
    static const uint8_t digits[10][5] = {
        {62,81,73,69,62}, {0,66,127,64,0}, {98,81,73,73,70},
        {34,65,73,73,54}, {24,20,18,127,16}, {39,69,69,69,57},
        {60,74,73,73,48}, {1,113,9,5,3}, {54,73,73,73,54}, {6,73,73,41,30}
    };
    static const uint8_t letters[26][5] = {
        {126,9,9,9,126}, {127,73,73,73,54}, {62,65,65,65,34},
        {127,65,65,34,28}, {127,73,73,73,65}, {127,9,9,9,1},
        {62,65,73,73,58}, {127,8,8,8,127}, {0,65,127,65,0},
        {32,64,65,63,1}, {127,8,20,34,65}, {127,64,64,64,64},
        {127,2,12,2,127}, {127,4,8,16,127}, {62,65,65,65,62},
        {127,9,9,9,6}, {62,65,81,33,94}, {127,9,25,41,70},
        {38,73,73,73,50}, {1,1,127,1,1}, {63,64,64,64,63},
        {31,32,64,32,31}, {63,64,56,64,63}, {99,20,8,20,99},
        {7,8,112,8,7}, {97,81,73,69,67}
    };
    static const uint8_t blank[5] = {0}, unknown[5] = {2,1,81,9,6};
    static const uint8_t dot[5] = {0,96,96,0,0}, colon[5] = {0,54,54,0,0};
    static const uint8_t plus[5] = {8,8,62,8,8}, minus[5] = {8,8,8,8,8};
    static const uint8_t slash[5] = {64,32,16,8,4}, equals[5] = {20,20,20,20,20};
    if (c >= '0' && c <= '9') return digits[(unsigned)(c - '0')];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return letters[(unsigned)(c - 'A')];
    switch (c) {
        case ' ': return blank;
        case '.': return dot;
        case ':': return colon;
        case '+': return plus;
        case '-': return minus;
        case '/': return slash;
        case '=': return equals;
        case '*': return letters['X' - 'A'];
        default: return unknown;
    }
}

static inline int daily_draw_width(const char *text, int scale) {
    if (!text || !*text || scale < 1 || scale > 64) return 0;
    int count = 0;
    while (*text++) {
        if (count >= (INT_MAX / scale - 5) / 6) return INT_MAX;
        ++count;
    }
    return (count * 6 - 1) * scale;
}

static inline void daily_draw_text_tone(const t5_app_api_v1 *api, int x, int y,
                                        const char *text, int scale, bool black) {
    if (!api || !api->fill_rect || !text || scale < 1 || scale > 64 ||
        x > INT_MAX - 6 * scale || y > INT_MAX - 7 * scale) return;
    for (; *text; ++text) {
        const uint8_t *glyph = daily_draw_glyph(*text);
        for (int column = 0; column < 5; ++column)
            for (int row = 0; row < 7; ++row)
                if (glyph[column] & (1u << row))
                    api->fill_rect(x + column * scale, y + row * scale, scale, scale, black);
        if (x > INT_MAX - 12 * scale) break;
        x += 6 * scale;
    }
}

static inline void daily_draw_text(const t5_app_api_v1 *api, int x, int y,
                                   const char *text, int scale) {
    daily_draw_text_tone(api, x, y, text, scale, true);
}
#endif
