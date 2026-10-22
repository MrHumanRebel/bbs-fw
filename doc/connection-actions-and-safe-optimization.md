# Connection actions and behavior-preserving ramp optimization

## Connection workspace

The Connection page exposes Open, Save, Save log, Read, Write and Reset with
native font icons and accessible names. File commands keep their existing XML
and log formats. The new buttons and the menu invoke the same view-model
commands. Controller actions are disabled while disconnected or while another
configuration/calibration request is in progress. Cards use the available width;
the outer scrollbar appears only when the measured content exceeds the viewport.

Every configuration write requires a new Yes/No confirmation, with **No as the
default**. The dialog identifies the controller and firmware, shows the maximum
current and battery limits, and explains that stored settings will be replaced.
Closing or declining the dialog sends no write request. Resetting configuration,
saving voltage calibration and resetting voltage calibration use the same guard
and also require explicit confirmation.

The shared guard rejects overlapping requests rather than queuing them. It
releases its busy state after cancellation, completion or exceptions, and never
retries a write automatically. It checks the connection generation before and
after confirmation: disconnecting or reconnecting while a modal dialog is open
invalidates the approval. Configuration writes use an independent snapshot of
the reviewed values. Bench controls are stopped before controller operations.
Read and reset responses from an obsolete connection cannot replace the local
configuration.

## Firmware scope and hardware review

The only firmware change is in `current_ramp_curve()` in `src/firmware/app.c`.
Its integer formula now returns early for mathematically exact cases:

- Linear shape (`smoothness == 0`) returns the clamped progress directly.
- Progress endpoints (`0` and `1000`) return directly for every shape.
- Full smoothness (`100`) returns the cubic result without the subsequent blend.

Both existing input clamps remain before the fast paths. Intermediate shapes
retain the original cubic and integer blend, including truncation. The default
linear shape therefore avoids the cubic's 32-bit arithmetic and divisions.
This is a computation-time optimization, not a RAM reduction. No cycle-time or
battery-life improvement is claimed without controller measurements.

The review used `drawings/bbshd.pdf`, the Eagle source
`drawings/pcb/bbshd.sch`, and `src/firmware/bbsx/pins.h`. The BBSHD schematic's
brake P2.4, throttle P1.3, speed P2.2, display UART P3.0/P3.1 and motor UART
P1.0/P1.1 agree with the existing mappings. The schematic contains untraced
signals, so it does not establish complete electrical compatibility for every
controller revision. BBS02 and TSDZ2 use their existing target definitions.

The patch does not change pin assignments, GPIO or ADC setup, interrupt handlers,
UART drivers, register accesses, watchdog configuration, EEPROM layout, wire
protocol, current/speed/thermal/low-voltage limits, or application scheduling.
The ramp's `RAM_REENTRANT` declarations remain in place for the 8051 targets.
Other potential changes around sensor reads, interrupt timing and protection
calculations were deliberately excluded because their equivalence requires
additional hardware evidence.

## Verification

The C harness compares the production optimized function against the frozen
original formula for all **65,536 progress values × 256 smoothness values =
16,777,216 combinations per target**, including inputs beyond both clamps.
`sh tests/run.sh` repeats this for BBS02, BBSHD and TSDZ2 and retains the ramp,
brake, throttle, shift, thermal, low-voltage, walk, speed-limit, protocol and
configuration-migration regressions.

Clean before/after builds with SDCC 4.5.0 produced these linker reports:

| Target | RAM before / after | Stack space reported before / after | Flash / CODE before / after |
| --- | --- | --- | --- |
| BBS02 | XRAM 904 / 904 bytes | 150 / 150 bytes available | Flash 24,826 / 24,870 bytes |
| BBSHD | XRAM 918 / 918 bytes | 138 / 138 bytes available | Flash 26,382 / 26,426 bytes |
| TSDZ2 | DATA 587 / 587; INITIALIZED 61 / 61 bytes | SSEG reservation 1 / 1 byte | CODE 15,026 / 15,069 bytes |

The flash cost is 44 bytes on each 8051 target and 43 bytes in the STM8 CODE
section. SSEG reservation is not a measurement of peak runtime stack usage.

Portable .NET tests exercise approval rejection, fresh approval for each write,
connection changes during confirmation, overlapping reads/writes, request
failures and confirmation failures. The Windows UI suite checks both themes at
1000×650, 1280×720 and 1920×1080, all six quick-action bindings, icon/label fit,
disconnected availability, and rejection through the real Write, Reset and
calibration handlers. These tests use an injected rejecting confirmation and
never open a controller serial port.

Host equivalence and successful target builds establish the checked software
properties. They do not establish physical electrical compatibility or replace
bench validation on an actual controller. Existing hardware support restrictions
and the supported flashing procedure still apply.
