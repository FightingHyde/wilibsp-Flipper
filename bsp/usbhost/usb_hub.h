#pragma once
#include "usb_core.h"

// Single-port hub passthrough (implemented in Task 10).
hcd_result_t usb_hub_attach(const usb_device_t *hub);
// Non-blocking: does ONE pass over the hub's ports looking for a connected FS
// device, and if found, resets + enumerates it (that part is a bounded ~0.6 s
// blocking sequence, but only runs when a device is actually present). Returns
// true + fills *drive on success. Returns false immediately (no sleep_ms) when
// nothing is connected yet -- call again on the next usb_msc_task() tick; the
// caller owns the overall wait timeout. Replaces the old usb_hub_wait_drive(),
// which blocked the whole caller for up to timeout_ms (see AGENTS.md/repo
// memory: this stalled the wilicankit LVGL+touch loop for ~5s at a time
// whenever a hub was attached with no drive behind it).
bool         usb_hub_poll_drive(usb_device_t *drive);
bool         usb_hub_drive_present(void);
void         usb_hub_detach(void);
