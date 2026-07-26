#include <string.h>
#include "agentio/agentio_shadow.h"
#include "test_util.h"

#define W 8
#define H 4

static uint8_t g_fb[W * H * 2];
static agentio_shadow_t g_s;

static void reset(void)
{
    memset(g_fb, 0, sizeof g_fb);
    agentio_shadow_init(&g_s, g_fb, W, H);
}

/* Pixel at (x,y) as the big-endian value stored in the shadow. */
static uint16_t px(int x, int y)
{
    const uint8_t *p = &g_fb[((size_t)y * W + x) * 2];
    return (uint16_t)((p[0] << 8) | p[1]);
}

static void write_px(uint16_t v)
{
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
    agentio_shadow_write(&g_s, b, 2);
}

int main(void)
{
    /* a 2x2 window fills left-to-right, top-to-bottom */
    reset();
    agentio_shadow_set_window(&g_s, 1, 1, 2, 2);
    write_px(0x1111); write_px(0x2222);
    write_px(0x3333); write_px(0x4444);
    ASSERT_EQ(px(1, 1), 0x1111);
    ASSERT_EQ(px(2, 1), 0x2222);
    ASSERT_EQ(px(1, 2), 0x3333);
    ASSERT_EQ(px(2, 2), 0x4444);
    ASSERT_EQ(px(0, 0), 0x0000);          /* nothing outside the window */

    /* bytes split across calls, including a split mid-pixel */
    reset();
    agentio_shadow_set_window(&g_s, 0, 0, 1, 0);
    uint8_t half[1] = { 0xAB };
    agentio_shadow_write(&g_s, half, 1);   /* high byte only */
    uint8_t rest[3] = { 0xCD, 0x12, 0x34 };
    agentio_shadow_write(&g_s, rest, 3);
    ASSERT_EQ(px(0, 0), 0xABCD);
    ASSERT_EQ(px(1, 0), 0x1234);

    /* writing past the end of the window is discarded, not wrapped */
    reset();
    agentio_shadow_set_window(&g_s, 0, 0, 0, 0);
    write_px(0x5555);
    write_px(0x6666);
    ASSERT_EQ(px(0, 0), 0x5555);
    ASSERT_EQ(px(1, 0), 0x0000);

    /* a window overhanging the right/bottom edge clips; in-bounds pixels of
     * each row still land in the right place */
    reset();
    agentio_shadow_set_window(&g_s, W - 2, H - 1, W + 4, H + 4);
    write_px(0x7777); write_px(0x8888);    /* the two in-bounds pixels */
    write_px(0x9999);                      /* off the right edge: discarded */
    ASSERT_EQ(px(W - 2, H - 1), 0x7777);
    ASSERT_EQ(px(W - 1, H - 1), 0x8888);

    /* a fully out-of-bounds window swallows everything without corrupting */
    reset();
    agentio_shadow_set_window(&g_s, 100, 100, 110, 110);
    write_px(0xFFFF);
    for (size_t i = 0; i < sizeof g_fb; i++) ASSERT_EQ(g_fb[i], 0);

    /* a full-screen window accepts exactly W*H pixels */
    reset();
    agentio_shadow_set_window(&g_s, 0, 0, W - 1, H - 1);
    for (int i = 0; i < W * H; i++) write_px((uint16_t)(0x0100 + i));
    ASSERT_EQ(px(0, 0), 0x0100);
    ASSERT_EQ(px(W - 1, H - 1), (uint16_t)(0x0100 + W * H - 1));

    /* set_window resets the cursor, discarding any half-written pixel */
    reset();
    agentio_shadow_set_window(&g_s, 0, 0, W - 1, H - 1);
    agentio_shadow_write(&g_s, half, 1);
    agentio_shadow_set_window(&g_s, 3, 2, 3, 2);
    write_px(0xEEEE);
    ASSERT_EQ(px(3, 2), 0xEEEE);
    ASSERT_EQ(px(0, 0), 0x0000);

    TEST_RETURN();
}
