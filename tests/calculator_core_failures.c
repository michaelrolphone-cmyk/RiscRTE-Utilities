#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include "../Apps/calculator_core.h"

static void keys(calculator *c, const char *s) { for (; *s; ++s) calc_key(c, *s); }
static void example(const char *sequence, const char *expected) {
    calculator c; calc_reset(&c); keys(&c, sequence);
    assert(c.error == CALC_OK);
    if (strcmp(c.text, expected)) { fprintf(stderr, "%s -> %s, expected %s\n", sequence, c.text, expected); assert(0); }
}
static uint64_t random_state = UINT64_C(0x431b54c8ad);
static uint64_t random_value(void) {
    random_state ^= random_state << 13; random_state ^= random_state >> 7;
    random_state ^= random_state << 17; return random_state;
}

int main(void) {
    example("0.1+0.2=", "0.3");
    example("2+3==", "8");
    example("2+3*4=", "20"); /* Immediate execution, not precedence. */
    example("12+-3=", "9"); /* Operator replacement. */
    example("2+=", "4");
    example("2+S3=", "-1");
    example("S0.5*4=", "-2");
    example("1/3=", "0.333333");
    example("2/3=", "0.666667");
    example("S2/3=", "-0.666667");
    example("0.000001/2=", "0.000001");
    example("S0.000001/2=", "-0.000001");
    example("0.000001*0.5=", "0.000001");
    example("0.000001*0.49=", "0");
    example("999999999.999999*1=", "999999999.999999");
    example("999999999.999999/1=", "999999999.999999");
    example("999999999.999999+S999999999.999999=", "0");
    example("12.30BB", "12.");
    example("S1B", "0");
    example("2+3=7", "7");
    example("2+3=BC", "0");
    example("0000000007..1234567", "7.123456");
    example("1/0=C8", "8");
    example("1/0=B", "0");
    example("1/0=9", "9");
    example("12+BB3=", "15");
    example("9*9===", "6561");
    example("5SS", "5");
    example("5+0.25=BS", "-5.2");
    calculator c; calc_reset(&c);
    keys(&c,"1/0="); assert(c.error==CALC_DIV_ZERO && !c.pending && !c.repeat);
    keys(&c,"=S+/"); assert(c.error==CALC_DIV_ZERO);
    keys(&c,".5"); assert(!c.error && !strcmp(c.text,"0.5"));
    keys(&c,"C999999999.999999+0.000001="); assert(c.error==CALC_OVERFLOW);
    keys(&c,"C999999999*2="); assert(c.error==CALC_OVERFLOW);
    keys(&c,"C999999999/0.000001="); assert(c.error==CALC_OVERFLOW);
    keys(&c,"C1000000000"); assert(c.error==CALC_OVERFLOW);
    keys(&c,"8"); assert(!c.error && !strcmp(c.text,"8"));
    keys(&c,"CS999999999.999999-0.000001="); assert(c.error==CALC_OVERFLOW);
    int64_t out=123; calc_error error=CALC_OK;
    assert(!calc_apply(INT64_MIN,'+',0,&out,&error) && error==CALC_OVERFLOW && out==123);
    assert(!calc_apply(INT64_MAX,'*',1,&out,&error) && error==CALC_OVERFLOW);
    assert(!calc_apply(1,'+',1,NULL,&error));
    assert(!calc_apply(1,'+',1,&out,NULL));
    assert(!calc_apply(1,'?',1,&out,&error));

    /* Check the app-local divider across full uint64, including carry cases
     * larger than the calculator itself ever supplies. Native / and % here
     * belong solely to the independent host test, not the shipped app. */
    const uint64_t edges[] = {0, 1, 2, 3, 10, CALC_SCALE, CALC_LIMIT,
        UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000000),
        UINT64_C(0x8000000000000001), UINT64_MAX};
    for (unsigned i=0;i<sizeof(edges)/sizeof(edges[0]);++i) {
        for (unsigned j=0;j<sizeof(edges)/sizeof(edges[0]);++j) {
            uint64_t remainder=0, numerator=edges[i], denominator=edges[j];
            uint64_t quotient=calc_udivmod(numerator,denominator,&remainder);
            assert(quotient==(denominator?numerator/denominator:0));
            assert(remainder==(denominator?numerator%denominator:numerator));
            assert(calc_udivmod(numerator,denominator,NULL)==quotient);
        }
    }
    for (unsigned i=0;i<30000;++i) {
        uint64_t numerator=random_value(), denominator=random_value()|1u, remainder;
        assert(calc_udivmod(numerator,denominator,&remainder)==numerator/denominator);
        assert(remainder==numerator%denominator);
    }

    /* Independent high-precision host oracle, never shipped in the app. */
    const char operators[] = "+-*/";
    for (unsigned i=0;i<60000;++i) {
        int64_t a=(int64_t)(random_value()%CALC_LIMIT), b=(int64_t)(random_value()%CALC_LIMIT);
        if (i%3==0) { a %= UINT64_C(1000000000); b %= UINT64_C(1000000000); }
        if (i%17==0) b=0;
        if (i&1) a=-a;
        if (i&2) b=-b;
        char op=operators[(i/4)%4];
        __int128 expected=0;
        bool zero=op=='/' && b==0;
        if (op=='+') expected=(__int128)a+b;
        else if (op=='-') expected=(__int128)a-b;
        else if (!zero) {
            __int128 n=op=='*'?(__int128)calc_magnitude(a)*calc_magnitude(b):(__int128)calc_magnitude(a)*CALC_SCALE;
            uint64_t d=op=='*'?CALC_SCALE:calc_magnitude(b);
            expected=(n+d/2)/d;
            if ((a<0)!=(b<0)) expected=-expected;
        }
        bool overflow=expected>(__int128)CALC_LIMIT || expected<-(__int128)CALC_LIMIT;
        bool ok=calc_apply(a,op,b,&out,&error);
        assert(ok==(!zero && !overflow));
        assert(error==(zero?CALC_DIV_ZERO:overflow?CALC_OVERFLOW:CALC_OK));
        if (ok) assert(out==(int64_t)expected);
    }
    puts("calculator core: 60,000 arithmetic + 30,000 divider oracle cases and decimal/recovery sequences passed");
    return 0;
}
