# hello_usbdrive

On-hardware smoke test for the USB host mass-storage stack and FatFs. It powers
the USB-A ports, waits for a FAT32 drive, and counts *.ir files in its root.
The LCD shows waiting, mounted, file-count, and removal states; RTT carries the
complete root listing.

    fw build hello_usbdrive
    fw flash hello_usbdrive
    fw rtt

A seated drive should mount within a few seconds. Pulling and reinserting it
should produce a removal followed by a clean remount. With no drive, the app
remains safely on the waiting screen.
