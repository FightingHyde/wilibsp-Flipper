#include <string.h>
#include "input/picpwr_frame.h"
#include "test_util.h"

/* Frame construction against the documented worked example:
 * zones 1-17 awake, sleep and wake fields zero. */
static void test_frame_worked_example(void)
{
    const uint8_t expect[PICPWR_FRAME_LEN] = {
        0xB0, 0x00, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAF,
    };
    picpwr_cfg_t cfg = { .awake = 0x01FFFFu, .sleep = 0, .wake = 0,
                         .wake2 = 0 };
    uint8_t out[PICPWR_FRAME_LEN];
    ASSERT_EQ(picpwr_frame_build(out, &cfg), PICPWR_FRAME_LEN);
    ASSERT_TRUE(memcmp(out, expect, sizeof expect) == 0);
}

/* Masks serialize most-significant byte first, independently per field. */
static void test_frame_mask_placement(void)
{
    picpwr_cfg_t cfg = { .awake = 0x013456u, .sleep = 0x01CDEFu,
                         .wake = 0x5A, .wake2 = 0xA5 };
    uint8_t out[PICPWR_FRAME_LEN];
    picpwr_frame_build(out, &cfg);
    ASSERT_EQ(out[2], 0x01); ASSERT_EQ(out[3], 0x34); ASSERT_EQ(out[4], 0x56);
    ASSERT_EQ(out[5], 0x01); ASSERT_EQ(out[6], 0xCD); ASSERT_EQ(out[7], 0xEF);
    ASSERT_EQ(out[8], 0x5A); ASSERT_EQ(out[9], 0xA5);
    uint8_t sum = 0;
    for (int i = 0; i < PICPWR_FRAME_LEN - 1; i++)
        sum = (uint8_t)(sum + out[i]);
    ASSERT_EQ(out[PICPWR_FRAME_LEN - 1], sum);
}

/* Reserved mask bits (above zone 17) can never reach the wire: the
 * builder strips them from both masks regardless of the caller's cfg
 * (docs/drivers/power.md). This test pins that guarantee. */
static void test_frame_reserved_bits_stripped(void)
{
    picpwr_cfg_t cfg = { .awake = 0xFFFFFFu, .sleep = 0xFE0000u,
                         .wake = 0, .wake2 = 0 };
    uint8_t out[PICPWR_FRAME_LEN];
    picpwr_frame_build(out, &cfg);
    ASSERT_EQ(out[2], 0x01);   /* awake byte 0: only zone 17 survives */
    ASSERT_EQ(out[3], 0xFF);
    ASSERT_EQ(out[4], 0xFF);
    ASSERT_EQ(out[5], 0x00);   /* sleep byte 0: reserved-only mask -> 0 */
    ASSERT_EQ(out[6], 0x00);
    ASSERT_EQ(out[7], 0x00);
    ASSERT_EQ(PICPWR_ZONE_MASK_ALL, 0x01FFFFu);
}

/* Zone bit helper: zone N = bit N-1. */
static void test_zone_bits(void)
{
    ASSERT_EQ(picpwr_zone_bit(PICPWR_ZONE_SENSORS), 0x000001u);
    ASSERT_EQ(picpwr_zone_bit(PICPWR_ZONE_DISPLAY), 0x000002u);
    ASSERT_EQ(picpwr_zone_bit(PICPWR_ZONE_AUDIO), 0x000004u);
    ASSERT_EQ(picpwr_zone_bit(PICPWR_ZONE_STATUS_LED), 0x000100u);
    ASSERT_EQ(picpwr_zone_bit(PICPWR_ZONE_CAN), 0x004000u);
    ASSERT_EQ(picpwr_zone_bit(PICPWR_ZONE_DEBUG_PROBE), 0x008000u);
    ASSERT_EQ(picpwr_zone_bit(PICPWR_ZONE_COMPUTE), 0x010000u);
}

/* Rail-state extraction from a synthetic status payload. */
static void test_rails_decode(void)
{
    uint8_t p[20];
    memset(p, 0, sizeof p);
    ASSERT_EQ(picpwr_rails_decode(p), 0);

    p[3] = 0x28;                 /* zones 1 (0x08) + 3 (0x20) */
    p[6] = 0x04;                 /* zone 15 */
    ASSERT_EQ(picpwr_rails_decode(p),
              picpwr_zone_bit(1) | picpwr_zone_bit(3)
                  | picpwr_zone_bit(15));

    /* All payload bits set: exactly zones 1-17, nothing above leaks in. */
    memset(p, 0xFF, sizeof p);
    ASSERT_EQ(picpwr_rails_decode(p), 0x01FFFFu);
}

int main(void)
{
    test_frame_worked_example();
    test_frame_mask_placement();
    test_frame_reserved_bits_stripped();
    test_zone_bits();
    test_rails_decode();
    if (g_failures == 0) printf("test_picpwr_frame: all passed\n");
    TEST_RETURN();
}
