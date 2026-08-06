#pragma once
#include "usb_core.h"

// Single-port hub passthrough (implemented in Task 10).
hcd_result_t usb_hub_attach(const usb_device_t *hub);
// Non-blocking: does ONE pass over the hub's ports looking for a connected FS
// device, and if found, resets + enumerates it (that part is a bounded ~0.6 s
// blocking sequence, but only runs when a device is actually present). Returns
// true + fills *drive on success. Returns false immediately (no sleep_ms) when
// nothing is connected yet -- call again on the next usb_msc_task() tick; the
// caller may wait indefinitely for a connection. Transient reset/enumeration
// failures receive a bounded set of backed-off retries; a non-MSC device, or a
// device that exhausts those retries, is ignored until it reconnects. Replaces
// the old usb_hub_wait_drive(), which blocked the whole caller for up to
// timeout_ms whenever a hub was attached with no drive behind it.
bool         usb_hub_poll_drive(usb_device_t *drive);
bool         usb_hub_drive_present(void);
void         usb_hub_detach(void);
