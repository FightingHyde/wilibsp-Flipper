#include "agentio/agentio_rle.h"

#define RUN_MAX 128

size_t agentio_rle_bound(size_t units)
{
    if (units == 0) return 0;
    return units * 2 + (units + RUN_MAX - 1) / RUN_MAX;
}

/* Length of the repeat run starting at i (>= 1), capped at RUN_MAX. */
static size_t run_len(const uint16_t *src, size_t units, size_t i)
{
    size_t n = 1;
    while (i + n < units && src[i + n] == src[i] && n < RUN_MAX) n++;
    return n;
}

size_t agentio_rle_encode(const uint16_t *src, size_t units,
                          uint8_t *dst, size_t cap)
{
    size_t di = 0, i = 0;

    while (i < units) {
        size_t run = run_len(src, units, i);

        if (run >= 2) {                         /* repeat run */
            if (di + 3 > cap) return 0;
            dst[di++] = (uint8_t)(int8_t)(1 - (int)run);
            dst[di++] = (uint8_t)(src[i] >> 8);
            dst[di++] = (uint8_t)(src[i] & 0xFF);
            i += run;
            continue;
        }

        /* Literal run: gather units until a repeat of >= 2 starts, or the
         * 128-unit control-byte limit is reached. */
        size_t start = i, lit = 0;
        while (i < units && lit < RUN_MAX && run_len(src, units, i) < 2) {
            i++;
            lit++;
        }
        if (di + 1 + lit * 2 > cap) return 0;
        dst[di++] = (uint8_t)(int8_t)(lit - 1);
        for (size_t k = 0; k < lit; k++) {
            dst[di++] = (uint8_t)(src[start + k] >> 8);
            dst[di++] = (uint8_t)(src[start + k] & 0xFF);
        }
    }
    return di;
}
