# hello_sdcard

Reads and writes the main CPU's SD card through OneWili over the FwGUI display
link. The LCD shows connection, write progress, and the final result; RTT
carries full file and directory details.

The test creates /owlog, appends ten records to /owlog/run.txt, stats and
reads the file back, then lists the directory. It requires an inserted SD card
and stock firmware on the main CPU.

Calls use the recovery-aware OneWili and SDFS wrappers. Paths are absolute,
at most two files may be open, and callers must check ow_sd_close() because
write errors surface there. The large ow_device remains static to avoid the
small app stack.

Build with fw build hello_sdcard. This path has not yet been hardware-verified
by this BSP.
