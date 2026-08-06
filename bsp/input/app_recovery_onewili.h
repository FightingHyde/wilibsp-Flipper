/* Recovery-aware adapters for OneWili's synchronous FwGUI transports.
 * Include this only in apps that link libs/onewili. */
#ifndef FW2_APP_RECOVERY_ONEWILI_H
#define FW2_APP_RECOVERY_ONEWILI_H

#include "input/app_recovery.h"
#include "onewili.h"
#include "onewili_fwgui.h"
#include "onewili_sd.h"

typedef struct {
    ow_transport inner;
} fw2_recovery_ow_ctx_t;

static fw2_recovery_ow_ctx_t fw2_recovery_ow_ctx;

static int fw2_recovery_ow_read(void *ctx, uint8_t *buf, size_t cap,
                                uint32_t timeout_ms) {
    fw2_recovery_ow_ctx_t *wrapped = (fw2_recovery_ow_ctx_t *)ctx;
    if (timeout_ms == 0) {
        int result = wrapped->inner.read(wrapped->inner.ctx, buf, cap, 0);
        fw2_app_recovery_task();
        return result;
    }
    while (timeout_ms != 0) {
        uint32_t slice_ms = timeout_ms < 10u ? timeout_ms : 10u;
        int result = wrapped->inner.read(wrapped->inner.ctx, buf, cap, slice_ms);
        fw2_app_recovery_task();
        if (result != 0) return result;
        timeout_ms -= slice_ms;
    }
    return 0;
}

static inline void fw2_app_recovery_wrap_onewili(ow_device *dev) {
    fw2_recovery_ow_ctx.inner = dev->t;
    dev->t.ctx = &fw2_recovery_ow_ctx;
    dev->t.read = fw2_recovery_ow_read;
}

typedef struct {
    sdfs_transport_t inner;
} fw2_recovery_sd_ctx_t;

static fw2_recovery_sd_ctx_t fw2_recovery_sd_ctx;

static int fw2_recovery_sd_send(void *ctx, const uint8_t *buf, size_t len) {
    fw2_recovery_sd_ctx_t *wrapped = (fw2_recovery_sd_ctx_t *)ctx;
    int result = wrapped->inner.send(wrapped->inner.ctx, buf, len);
    fw2_app_recovery_task();
    return result;
}

static int fw2_recovery_sd_recv(void *ctx, uint8_t *buf, size_t cap,
                                size_t *len) {
    fw2_recovery_sd_ctx_t *wrapped = (fw2_recovery_sd_ctx_t *)ctx;
    int result = wrapped->inner.recv(wrapped->inner.ctx, buf, cap, len);
    fw2_app_recovery_task();
    return result;
}

static inline ow_status fw2_app_recovery_wrap_sd(void) {
    sdfs_transport_t wrapped;
    fw2_recovery_sd_ctx.inner = ow_fwgui_sdfs_transport();
    wrapped.send = fw2_recovery_sd_send;
    wrapped.recv = fw2_recovery_sd_recv;
    wrapped.ctx = &fw2_recovery_sd_ctx;
    return ow_sd_arm(&wrapped);
}

#endif
