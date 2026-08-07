# FW2App contract hardware verification (2026-08-06)

## Setup

- FreeWili 2 DISPLAY app launched from an SD-card UF2 through the stock App
  Explorer.
- App UF2s were generated from branch `docs/fw2app-contract` and validated as
  SRAM-only before installation; no QSPI-flash payload was present.
- Hardware was operated by the owner for the physical selection/HOME steps and
  by `fw install-app` over MAIN's legacy `COM44` interface for SD handoff.

## Observed results

- `hello_agentio` launched successfully from `/apps/hello_agentio.uf2`.
- Holding the physical HOME button for five seconds left the RAM app and
  returned to the stock firmware. This verifies the physical HOME recovery
  path for this app on hardware.
- The first build left pixels from the preceding DISPLAY firmware visible in
  regions it did not redraw. This demonstrated that LCD RAM survives the app
  handoff and that partial first-frame drawing is insufficient.
- The contract and every affected LCD example were updated to establish the
  complete 480x320 surface before enabling the backlight. `hello_agentio` was
  bumped to version 002 and reinstalled. Visual confirmation of the corrected
  first frame is still pending.
- All 15 registered example UF2s were copied to `/apps/` in one SD mount. Each
  destination matched its build artifact by SHA-256 before the card was safely
  ejected and returned to MAIN.

## Installer issue found

Windows refused one safe eject and later left the USB mass-storage reader in an
error state until the board was reset. The installer correctly kept a mounted
card with the PC when eject failed. A separate pre-mount enumeration timeout
revealed that the mux was also left with the PC even though no filesystem had
mounted; `fw install-app` now returns ownership to MAIN in that safe case while
preserving the fail-closed behavior after a volume has appeared.

## Remaining hardware work

- Relaunch `hello_agentio` version 002 and confirm no inherited pixels are
  visible before its test pattern.
- Exercise AgentIO screenshot/input commands while v002 is running. The stock
  DISPLAY firmware used during selection does not itself expose AgentIO.
- The other 14 installed apps were copied and integrity-checked but were not
  individually launched during this session.
