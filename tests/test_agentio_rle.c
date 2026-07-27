#include <string.h>
#include "agentio/agentio_rle.h"
#include "test_util.h"

/* Reference decoder — deliberately an independent implementation of the format
 * documented in agentio_proto.h, so a matched pair of encoder/decoder bugs
 * cannot pass. Returns units decoded, or (size_t)-1 on malformed input. */
static size_t decode(const uint8_t *src, size_t n, uint16_t *dst, size_t cap)
{
    size_t si = 0, di = 0;
    while (si < n) {
        int ctrl = (int)(signed char)src[si++];
        if (ctrl >= 0) {
            size_t count = (size_t)ctrl + 1;
            if (si + count * 2 > n || di + count > cap) return (size_t)-1;
            for (size_t k = 0; k < count; k++) {
                dst[di++] = (uint16_t)((src[si] << 8) | src[si + 1]);
                si += 2;
            }
        } else if (ctrl != -128) {
            size_t count = (size_t)(1 - ctrl);
            if (si + 2 > n || di + count > cap) return (size_t)-1;
            uint16_t v = (uint16_t)((src[si] << 8) | src[si + 1]);
            si += 2;
            for (size_t k = 0; k < count; k++) dst[di++] = v;
        } else {
            return (size_t)-1;
        }
    }
    return di;
}

static void roundtrip(const uint16_t *src, size_t units)
{
    uint8_t enc[8192];
    uint16_t dec[1024];
    size_t n = agentio_rle_encode(src, units, enc, sizeof enc);
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(n <= agentio_rle_bound(units));
    ASSERT_EQ(decode(enc, n, dec, 1024), units);
    ASSERT_EQ(memcmp(dec, src, units * 2), 0);
}

int main(void)
{
    /* all one colour: the best case must actually compress hard */
    uint16_t flat[480];
    for (int i = 0; i < 480; i++) flat[i] = 0xF800;
    roundtrip(flat, 480);
    uint8_t enc[8192];
    ASSERT_TRUE(agentio_rle_encode(flat, 480, enc, sizeof enc) <= 32);

    /* incompressible: bounded expansion, never worse than the stated bound */
    uint16_t noise[480];
    for (int i = 0; i < 480; i++) noise[i] = (uint16_t)(i * 2654435761u);
    roundtrip(noise, 480);
    ASSERT_TRUE(agentio_rle_encode(noise, 480, enc, sizeof enc) <= 480 * 2 + 8);

    /* mixed runs and literals, including a run longer than one control byte */
    uint16_t mixed[600];
    for (int i = 0; i < 200; i++) mixed[i] = 0x0000;      /* > 128, must split */
    for (int i = 200; i < 400; i++) mixed[i] = (uint16_t)i;
    for (int i = 400; i < 600; i++) mixed[i] = 0x07E0;
    roundtrip(mixed, 600);

    /* boundary sizes around the 128-unit control limit */
    roundtrip(flat, 1);
    roundtrip(flat, 128);
    roundtrip(flat, 129);
    roundtrip(noise, 128);
    roundtrip(noise, 129);

    /* zero units encodes to zero bytes */
    ASSERT_EQ(agentio_rle_encode(flat, 0, enc, sizeof enc), 0);

    /* refuses to overflow a short destination */
    ASSERT_EQ(agentio_rle_encode(noise, 480, enc, 4), 0);

    TEST_RETURN();
}
