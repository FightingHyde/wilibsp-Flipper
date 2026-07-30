/* picpwr_frame — see picpwr_frame.h. */
#include "picpwr_frame.h"

size_t picpwr_frame_build(uint8_t out[PICPWR_FRAME_LEN],
                          const picpwr_cfg_t *cfg)
{
    /* Mask bits above zone 17 are reserved and must be zero on the wire
     * (docs/drivers/power.md). Enforced here so no caller can emit
     * them. */
    uint32_t awake = cfg->awake & PICPWR_ZONE_MASK_ALL;
    uint32_t sleep = cfg->sleep & PICPWR_ZONE_MASK_ALL;
    out[0]  = 0xB0;                              /* command sync */
    out[1]  = 0x00;                              /* power-zone id */
    out[2]  = (uint8_t)(awake >> 16);            /* masks go MSB first */
    out[3]  = (uint8_t)(awake >> 8);
    out[4]  = (uint8_t)(awake);
    out[5]  = (uint8_t)(sleep >> 16);
    out[6]  = (uint8_t)(sleep >> 8);
    out[7]  = (uint8_t)(sleep);
    out[8]  = cfg->wake;
    out[9]  = cfg->wake2;
    uint8_t sum = 0;
    for (int i = 0; i < PICPWR_FRAME_LEN - 1; i++)
        sum = (uint8_t)(sum + out[i]);
    out[PICPWR_FRAME_LEN - 1] = sum;             /* additive 8-bit checksum */
    return PICPWR_FRAME_LEN;
}

/* Rail-state positions inside the status payload: zone N reads from
 * payload[byte] & mask, high = powered. */
typedef struct { uint8_t byte, mask; } rail_pos_t;
static const rail_pos_t RAILS[17] = {
    { 3, 0x08 }, { 3, 0x10 }, { 3, 0x20 }, { 3, 0x40 }, { 3, 0x80 },
    { 4, 0x01 }, { 4, 0x02 },
    { 5, 0x02 }, { 5, 0x04 }, { 5, 0x08 }, { 5, 0x10 }, { 5, 0x20 },
    { 6, 0x01 }, { 6, 0x02 }, { 6, 0x04 }, { 6, 0x10 },
    { 7, 0x08 },
};

uint32_t picpwr_rails_decode(const uint8_t payload[20])
{
    uint32_t bits = 0;
    for (int z = 0; z < 17; z++)
        if (payload[RAILS[z].byte] & RAILS[z].mask)
            bits |= 1u << z;
    return bits;
}
