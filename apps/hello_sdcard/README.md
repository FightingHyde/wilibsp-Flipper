# hello_sdcard

Reads and writes the **SD card** from the display CPU using the OneWili C API
(`libs/onewili`) over the FwGUI display link (UART0, 8 Mbaud, hardware flow
control on GPIO 0-3).

The card belongs to the main CPU. `ow_open_fwgui` arms an SDFS client that
reaches fw2main's SDFS server over the same link, so an app here gets the same
file access the stock display firmware has — no direct SDIO/SPI path exists
from the display CPU.

What it does: `mkdir /owlog` → append `tick N` to `/owlog/run.txt` once a
second for ten seconds → `stat` the file → read it back whole → list the
directory. Everything is reported over RTT (`fw rtt`).

Requires the main CPU to run the stock FreeWili 2 firmware (it carries both
the OneWili bridge and the SDFS server) and an SD card to be inserted.

Worth knowing before you copy this:

- Paths are absolute and `/`-rooted, at most 128 characters. There is no
  internal-flash route.
- At most **two** files may be open at once (fw2main's `SDFS_HOST_MAX_HANDLES`);
  a third open fails with `OW_ERR_FAILED` / `SDFS_ERR_BUSY`.
- Writes are fire-and-forget — **always check `ow_sd_close`**, that is where a
  dropped chunk surfaces. `ow_sd_last_error()` gives the underlying sdfslib
  status.
- Calls block with a 2 s idle timeout (`ow_sd_set_timeout_ms` to change it).
- `ow_device` is ~37 KB, so it must be `static` — it will not fit the 2 KB
  stack. Every app executes from SRAM, so keep read buffers modest or put them
  in PSRAM.

Build/flash: `fw build hello_sdcard` / `fw flash hello_sdcard`.

**Not yet verified on hardware.** See `libs/onewili/README.md` for the API
and `docs/hardware/facts.md` for what is confirmed.
