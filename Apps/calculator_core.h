#ifndef CALCULATOR_CORE_H
#define CALCULATOR_CORE_H
/* Deterministic decimal calculator. Values are signed millionths. Arithmetic
 * rounds to nearest millionth, ties away from zero. No floating point/int128. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CALC_SCALE UINT64_C(1000000)
#define CALC_LIMIT UINT64_C(999999999999999)
#define CALC_TEXT_SIZE 20u

typedef enum { CALC_OK, CALC_DIV_ZERO, CALC_OVERFLOW } calc_error;
typedef struct {
    int64_t value, accumulator, repeat_value;
    char text[CALC_TEXT_SIZE];
    char pending, repeat;
    bool editing, waiting;
    calc_error error;
} calculator;

/* App-local form of NativeApps/UnsignedDivisionCompat.c's restoring divide.
 * Preserve the carry bit before subtraction so all uint64 divisors work. This
 * avoids compiler division/remainder helpers in the restricted portable ELF. */
static inline uint64_t calc_udivmod(uint64_t numerator, uint64_t denominator,
                                    uint64_t *remainder_out) {
    uint64_t quotient = 0, remainder = 0;
    if (!denominator) {
        if (remainder_out) *remainder_out = numerator;
        return 0; /* calc_apply reports zero divisors before reaching here. */
    }
    for (unsigned bit = 0; bit < 64; ++bit) {
        unsigned carry = (unsigned)(remainder >> 63);
        remainder = (remainder << 1) | (numerator >> 63);
        numerator <<= 1;
        quotient <<= 1;
        if (carry || remainder >= denominator) {
            remainder -= denominator;
            quotient |= 1;
        }
    }
    if (remainder_out) *remainder_out = remainder;
    return quotient;
}

static inline uint64_t calc_magnitude(int64_t value) {
    return value < 0 ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
}

static inline void calc_format(calculator *c) {
    uint64_t magnitude = calc_magnitude(c->value);
    uint64_t remainder;
    uint32_t whole = (uint32_t)calc_udivmod(magnitude, CALC_SCALE, &remainder);
    uint32_t fraction = (uint32_t)remainder;
    char reversed[12];
    size_t n = 0, out = 0;
    do { reversed[n++] = (char)('0' + whole % 10); whole /= 10; } while (whole);
    if (c->value < 0) c->text[out++] = '-';
    while (n) c->text[out++] = reversed[--n];
    if (fraction) {
        c->text[out++] = '.';
        for (uint32_t place = 100000; place; place /= 10)
            c->text[out++] = (char)('0' + (fraction / place) % 10);
        while (c->text[out - 1] == '0') --out;
    }
    c->text[out] = 0;
}

static inline void calc_reset(calculator *c) {
    memset(c, 0, sizeof(*c));
    c->text[0] = '0';
    c->editing = true;
}

static inline void calc_fail(calculator *c, calc_error error) {
    c->error = error;
    c->pending = c->repeat = 0;
    c->editing = c->waiting = false;
}

static inline bool calc_apply(int64_t a, char op, int64_t b,
                              int64_t *result, calc_error *error) {
    if (!result || !error) return false;
    *error = CALC_OK;
    if (calc_magnitude(a) > CALC_LIMIT || calc_magnitude(b) > CALC_LIMIT) {
        *error = CALC_OVERFLOW; return false;
    }
    if (op == '+' || op == '-') {
        int64_t sum = op == '+' ? a + b : a - b;
        if (calc_magnitude(sum) > CALC_LIMIT) { *error = CALC_OVERFLOW; return false; }
        *result = sum;
        return true;
    }
    uint64_t x = calc_magnitude(a), y = calc_magnitude(b), value = 0;
    if (op == '*') {
        uint64_t xf, yf;
        uint64_t xi = calc_udivmod(x, CALC_SCALE, &xf);
        uint64_t yi = calc_udivmod(y, CALC_SCALE, &yf);
        /* Each product fits uint64; range-check before multiplying by scale. */
        if (xi && yi > calc_udivmod(CALC_LIMIT / CALC_SCALE, xi, NULL)) goto overflow;
        value = xi * yi * CALC_SCALE;
        uint64_t parts[] = {xi * yf, yi * xf, calc_udivmod(xf * yf + CALC_SCALE / 2, CALC_SCALE, NULL)};
        for (unsigned i = 0; i < 3; ++i) {
            if (parts[i] > CALC_LIMIT - value) goto overflow;
            value += parts[i];
        }
    } else if (op == '/') {
        if (!y) { *error = CALC_DIV_ZERO; return false; }
        uint64_t remainder;
        value = calc_udivmod(x, y, &remainder);
        if (value > CALC_LIMIT / CALC_SCALE) goto overflow;
        for (unsigned i = 0; i < 6; ++i) {
            remainder *= 10;
            value = value * 10 + calc_udivmod(remainder, y, &remainder);
        }
        if (remainder * 2 >= y) ++value;
        if (value > CALC_LIMIT) goto overflow;
    } else {
        return false;
    }
    *result = (a < 0) != (b < 0) ? -(int64_t)value : (int64_t)value;
    return true;
overflow:
    *error = CALC_OVERFLOW;
    return false;
}

static inline void calc_parse_entry(calculator *c) {
    const char *p = c->text;
    bool negative = *p == '-';
    if (negative) ++p;
    uint64_t whole = 0, fraction = 0;
    uint32_t place = (uint32_t)CALC_SCALE;
    bool decimal = false;
    for (; *p; ++p) {
        if (*p == '.') { decimal = true; continue; }
        if (decimal) { place /= 10; fraction += (unsigned)(*p - '0') * place; }
        else whole = whole * 10 + (unsigned)(*p - '0');
    }
    c->value = (int64_t)(whole * CALC_SCALE + fraction);
    if (negative) c->value = -c->value;
}

static inline void calc_begin_entry(calculator *c) {
    if (c->error) calc_reset(c);
    if (!c->editing || c->waiting) {
        c->text[0] = '0'; c->text[1] = 0;
        c->value = 0;
        c->editing = true; c->waiting = false;
        c->repeat = 0;
    }
}

/* Key vocabulary: digits, '.', '+', '-', '*', '/', '=', C (all clear),
 * S (sign), B (delete). A seventh fractional digit is ignored. */
static inline void calc_key(calculator *c, char key) {
    if (key == 'C') { calc_reset(c); return; }
    if ((key >= '0' && key <= '9') || key == '.') {
        calc_begin_entry(c);
        size_t length = strlen(c->text);
        char *decimal = NULL;
        for(size_t i=0;i<length;i++)if(c->text[i]=='.'){decimal=c->text+i;break;}
        if (key == '.') {
            if (decimal) return;
        } else if (decimal) {
            if (strlen(decimal + 1) >= 6) return;
        } else {
            size_t sign = c->text[0] == '-';
            if (length - sign == 1 && c->text[sign] == '0') {
                c->text[sign] = key;
                calc_parse_entry(c); return;
            }
            if (length - sign >= 9) { calc_fail(c, CALC_OVERFLOW); return; }
        }
        if (length + 1 >= sizeof(c->text)) { calc_fail(c, CALC_OVERFLOW); return; }
        c->text[length] = key; c->text[length + 1] = 0;
        calc_parse_entry(c);
        return;
    }
    if (key == 'B') {
        if (c->error) { calc_reset(c); return; }
        if (c->waiting) return;
        if (!c->editing) { c->editing = true; c->repeat = 0; }
        size_t length = strlen(c->text);
        if (length) c->text[--length] = 0;
        if (!length || (length == 1 && c->text[0] == '-')) strcpy(c->text, "0");
        calc_parse_entry(c);
        return;
    }
    if (key == 'S') {
        if (c->error) return;
        if (c->waiting) calc_begin_entry(c);
        if (c->text[0] == '-') {
            size_t length=strlen(c->text);
            for(size_t i=0;i<length;i++)c->text[i]=c->text[i+1];
        }
        else {
            size_t length = strlen(c->text);
            for(size_t i=length+1;i>0;i--)c->text[i]=c->text[i-1];
            c->text[0] = '-';
        }
        c->value = -c->value;
        c->repeat = 0;
        return;
    }
    if (c->error) return;
    if (key == '+' || key == '-' || key == '*' || key == '/') {
        if (c->pending && !c->waiting) {
            if (!calc_apply(c->accumulator, c->pending, c->value, &c->value, &c->error)) {
                calc_fail(c, c->error); return;
            }
            calc_format(c);
        }
        c->accumulator = c->value;
        c->pending = key; c->repeat = 0;
        c->editing = false; c->waiting = true;
    } else if (key == '=') {
        char op = c->pending ? c->pending : c->repeat;
        if (!op) return;
        int64_t operand = c->pending ? c->value : c->repeat_value;
        int64_t left = c->pending ? c->accumulator : c->value;
        if (!calc_apply(left, op, operand, &c->value, &c->error)) {
            calc_fail(c, c->error); return;
        }
        c->repeat = op; c->repeat_value = operand;
        c->pending = 0; c->editing = false; c->waiting = false;
        calc_format(c);
    }
}
#endif
