# Issue #207: virtual display and bench controls

## Use

Install the updated firmware and use the accompanying Windows configuration
tool (.NET 6 Desktop Runtime x64). Connect with the usual USB programming
adapter, then open **Virtual Display**. It reads live wheel speed, battery
voltage/current, calculated electrical power, PAS and controller status without
enabling bench control. Speed uses the controller's stored wheel size and the
tool's metric/imperial preference. Power is measured `V * A`, without a 999 W
display cap; this does not alter a physical 800S display's behavior.

To control the bike, support it with the driven wheel clear and enable **bench
controls**. Enabling starts at PAS 0 and zero virtual throttle. Select a PAS
level; its existing configuration and limits apply. The throttle slider sets a
virtual input percentage, not motor-current percentage. Hold the throttle button
with the mouse or Space to apply it. The selected level must allow throttle.
Configured response curves, current limits, speed/thermal/LVC protection and
the physical brake continue to apply.

Release sends a zero-throttle command immediately, independently of telemetry
polling. **STOP**, window deactivation, leaving the tab and starting a flash
configuration operation disarm control. A stop returns PAS to zero. Control
state is volatile; no flash or EEPROM is written by this panel.

## Firmware interlocks

- No positive virtual throttle is accepted until a zero-throttle command arms
  the session.
- Commands must be checksum-valid, enabled/disabled correctly, PAS 0..9 and
  throttle 0..100. Invalid or incomplete packets cannot renew a command.
- Valid enabled commands renew a 1500 ms lease. Expiry clears throttle, selects
  PAS 0 and requests a stop on the next application cycle. Elapsed-time
  subtraction handles clock wrap. A late positive command cannot resurrect an
  expired session, even if it arrives before that application cycle.
- Zeroing a positive virtual throttle also blocks power briefly and requires a
  pedal stop before PAS re-arms. Motor-driven pedal pulses must not sustain drive
  after the throttle button is released.
- While bench control is enabled, virtual throttle replaces the physical
  throttle input. Physical brake protection remains effective. On disarm,
  ordinary controls return after the stop block; this is not a permanent motor
  lock or a substitute for a physical power disconnect.

The UI refreshes at nominally 500 ms without overlapping polls. Telemetry timeouts
or rejected controls disarm the UI. If the program freezes, the cable is removed
or a stop frame is lost, the firmware lease remains the independent fallback.
It is checked by the normal application loop, not a separate hardware watchdog.

## Protocol additions

All fields are bytes, multi-byte fields are big-endian, and the checksum is the
sum of all preceding bytes modulo 256. The existing configuration layout is
unchanged.

- Telemetry request: `01 04 05`.
- Reply, 15 bytes: `01 04 01 Vhi Vlo Ihi Ilo RPMhi RPMlo Wheelhi Wheello PAS Status VirtualThrottle Checksum`.
  Voltage/current/RPM/wheel diameter are in tenths (wheel diameter in inches).
  Byte 2 is telemetry schema version 1.
- Control request, 7 bytes:
  `02 F4 Sequence Enable PAS Throttle Checksum`.
- Control reply, 5 bytes: `02 F4 Sequence Accepted Checksum`.
  `Enable=0` stops; `Enable=1` arms/renews. `Accepted` is 0 or 1.
  Sequence zero is reserved by the tool for unawaited zero-throttle or STOP
  frames; awaited requests use nonzero sequences to correlate acknowledgements.

The tool verifies telemetry length, schema and checksum before enabling its
controls. Older firmware does not implement this telemetry request and cannot
use the new panel. Use the matched firmware/tool build. Disconnect any competing
physical display or other serial master when using the programming adapter.

## Validation

`sh tests/run.sh` exercises the real firmware control and telemetry handlers,
including first-command arming, range checks, partial/corrupt packets, explicit
stop, stale PAS inhibition on throttle release, lease timeout, late commands,
clock wrap, physical braking, speed limiting and telemetry field/checksum
encoding, for all three controller targets. Earlier regression suites also run.

`dotnet run --project tests/ConfigurationTests` tests the production telemetry
decoder against the firmware layout, speed conversion and power above 999 W, and
rejects partial/bad-checksum frames. Existing configuration round trips remain
tested. The full WPF application and all three firmware builds are validated.

No physical USB/controller/display or interactive Windows UI test has been
performed in the cloud environment. End-to-end serial timing, mouse/keyboard
capture and motor stopping still need verification on a supported bench before
normal use. In particular, test release, STOP, unplugging the adapter and
closing/deactivating the window; never rely only on the on-screen indication.
