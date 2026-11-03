/* Private freestanding 64-bit division. The established Xtensa compiler ships
 * non-PIC libgcc division objects which cannot be linked into this ELF DSO.
 * All arithmetic below is unsigned; no signed overflow or division recursion. */
#include <stdint.h>
uint64_t __udivdi3(uint64_t numerator, uint64_t denominator) {
    uint64_t quotient = 0;
    if (!denominator) return 0; /* Callers validate scale and sample counts. */
    for (unsigned bit = 64; bit-- > 0;) {
        if ((numerator >> bit) >= denominator) {
            numerator -= denominator << bit;
            quotient |= UINT64_C(1) << bit;
        }
    }
    return quotient;
}
int64_t __divdi3(int64_t numerator, int64_t denominator) {
    const int negative = (numerator < 0) != (denominator < 0);
    uint64_t a = (uint64_t)numerator, b = (uint64_t)denominator;
    if (numerator < 0) a = UINT64_C(0) - a;
    if (denominator < 0) b = UINT64_C(0) - b;
    uint64_t q = __udivdi3(a, b);
    if (negative && q) return -(int64_t)(q - 1) - 1;
    return q > INT64_MAX ? INT64_MIN : (int64_t)q;
}
