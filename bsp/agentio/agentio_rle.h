/*
 * agentio_rle — PackBits-16 encoder for agentio screen captures.
 * Pure: no hardware includes, host-tested by tests/test_agentio_rle.c.
 * Format is specified in agentio_proto.h — keep the two in sync.
 */
#ifndef AGENTIO_RLE_H
#define AGENTIO_RLE_H

#include <stddef.h>
#include <stdint.h>

/* Worst-case encoded size for `units` 16-bit units: every unit literal, plus
 * one control byte per 128-unit block. */
size_t agentio_rle_bound(size_t units);

/* Encode `units` 16-bit units from `src` into `dst` (capacity `cap` bytes).
 * Units are serialized big-endian. Returns the number of bytes written, or 0
 * if the result would not fit in `cap` (and 0 for units == 0). */
size_t agentio_rle_encode(const uint16_t *src, size_t units,
                          uint8_t *dst, size_t cap);

#endif /* AGENTIO_RLE_H */
