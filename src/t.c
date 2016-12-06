#include <stdint.h>
#include <stdio.h>

uint8_t alpha = 1;

uint8_t good(uint8_t n)
{
    return n * alpha / 255;
}

uint8_t maybe(uint8_t n)
{
    // return n * alpha >> 8;

    // return n * (alpha + 1) >> 8;

    // uint32_t a = alpha << 16 | alpha << 8 | alpha << 0;
    // return n * a >> 24;

    // return (uint32_t)n * (uint32_t)alpha * 0x8081 >> 16 >> 7;
    return (uint32_t)n * (uint32_t)alpha * 0x8081 >> (16 + 7);
}

void zzzz(void)
{
    uint16_t K = 255;
    uint32_t a = ((uint64_t)1 << 32) / K;
    printf("K = %u\n", K);
    printf("a = %#010x\n", a);
    uint32_t b = a;
    int S = 0;
    while (!(b & 0x80000000)) {
        b <<= 1;
        S++;
    }
    printf("b = %#x\n", b);
    printf("S = %d\n", S);
    uint32_t c = ((b >> 15) + 1) >> 1;
    printf("c = %#x\n", c);
    uint32_t M = a & 0xFFFF;
    printf("M = %#06x\n", M);
    uint32_t Q = (((uint32_t)b * (uint32_t)M) >> 16) >> S;
    printf("Q = %#06x\n", Q);
}

int main(void)
{
    int i, j, k, ii;

    if (1)
        zzzz();

    for (ii = 0; ii < 256; ii++) {
        alpha = ii;
        for (i = 0; i < 256; i++) {
            j = good(i);
            k = maybe(i);
            // if (j >> 2 != k >> 2) {
            if (j != k) {
                printf("alpha = %d, x(%d) => %d, not %d\n", alpha, i, k, j);
            }
        }
    }
    return 0;
}
