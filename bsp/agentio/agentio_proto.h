/*
 * agentio_proto — wire protocol shared by the target and tools/fw.py.
 * Keep this file and the protocol table in docs/drivers/agentio.md in sync.
 *
 * Commands: ASCII, newline-terminated, on RTT DOWN channel AGENTIO_RTT_CHANNEL.
 * Responses: on RTT UP channel AGENTIO_RTT_CHANNEL.
 *   - non-capture: "OK\n" or "ERR <reason>\n"
 *   - capture:     an 18-byte big-endian header, then payload_len bytes
 *
 * Capture header layout (all multi-byte fields big-endian):
 *   0  4  magic "FW2C"
 *   4  1  surface (AGENTIO_SURFACE_*)
 *   5  1  format  (AGENTIO_FORMAT_PACKBITS16)
 *   6  2  x
 *   8  2  y
 *  10  2  w   (output width, after scaling)
 *  12  2  h   (output height, after scaling)
 *  14  4  payload_len
 *
 * Payload: PackBits-16 (see agentio_rle.h). Control byte read as signed int8:
 *   0..127   -> n+1 literal units follow
 *   -127..-1 -> the next unit repeats 1-n times
 *   -128     -> never emitted
 * Units are RGB565 colour values serialized big-endian. Rows are encoded
 * independently and concatenated.
 */
#ifndef AGENTIO_PROTO_H
#define AGENTIO_PROTO_H

#define AGENTIO_RTT_CHANNEL   1
#define AGENTIO_MAGIC         "FW2C"
#define AGENTIO_HEADER_LEN    18

#define AGENTIO_SURFACE_LCD   0
#define AGENTIO_SURFACE_DVI   1

#define AGENTIO_FORMAT_PACKBITS16 0

/* Command verbs */
#define AGENTIO_CMD_PING  "PING"
#define AGENTIO_CMD_BTN   "BTN"
#define AGENTIO_CMD_TAP   "TAP"
#define AGENTIO_CMD_TCH   "TCH"
#define AGENTIO_CMD_TYPE  "TYPE"
#define AGENTIO_CMD_CAP   "CAP"

/* TCH modes */
#define AGENTIO_TOUCH_UP   0
#define AGENTIO_TOUCH_DOWN 1
#define AGENTIO_TOUCH_TAP  2

/* Limits */
#define AGENTIO_CMD_MAX    128   /* longest command line, including the NUL */
#define AGENTIO_TYPE_MAX    64   /* characters per TYPE command            */

#endif /* AGENTIO_PROTO_H */
