# Speed sensor regression tests

Run all C regression suites with `sh tests/run.sh`. For the optional voltage
compensation feature, see [#104](../doc/issue-104-pas-voltage-compensation.md).
Its configuration serialization tests run with
`dotnet run --project tests/ConfigurationTests` (.NET 10).
The same runner covers the [walk-release fix for #120](../doc/issue-120-walk-release.md).
It also covers the [speed-limit endpoint fix for #197](../doc/issue-197-speed-limit.md).
The [virtual display/control implementation for #207](../doc/issue-207-virtual-display.md)
adds protocol, lease-expiry and motor-stop tests plus telemetry-decoder checks.
The [additional RAM optimization](../doc/issue-86-additional-ram-optimization.md)
adds real-driver UART ring tests and maximum-size/pipelined configuration tests.

From the repository root, run:

```sh
for target in BBS02 BBSHD; do
    cc -std=c99 -Wall -Wextra -Wno-unknown-pragmas -D"$target" \
        -I src/firmware tests/speed_sensor_test.c -lm -o /tmp/bbs-speed-test || exit 1
    /tmp/bbs-speed-test || exit 1
done
```

The host harness includes the production `bbsx/sensors.c`, mocks GPIO and
interrupt registers, and calls the real timer ISR once per simulated 100 us.
Temperature ADC and logging dependencies are stubbed; no speed algorithm is
duplicated in the tests. No physical controller is required.

The tests cover constant speed, acceleration, short high and low glitches,
alternating noise, minimum accepted high and low durations, the existing minimum
period, timeout, counter saturation, restart, and multiple magnets. The original
code fails the short-glitch and alternating-noise cases. Rejected noise must not
reset the wheel-period counter or keep the moving flag set.

## Issue #114

The original 50 ms minimum **edge interval** is not a pulse-width filter. A
single noisy sample after that interval can reset the revolution timer and
publish a false speed to both the display and the speed limiter. A short low
glitch during a high signal can also create an extra rising edge. Debouncing
only rising edges would leave this second failure mode in place.

The fix requires ten consecutive 100 us samples of the new level before accepting
either transition (nominally 1 ms). It adds no averaging of wheel periods, and
keeps the existing minimum period and stopped-wheel timeout. Genuine changes in
speed still reach the speed limiter on the next qualified rising edge.

These tests establish a software filtering defect and its correction. They do
not establish the electrical cause of every report in #114. In particular,
missing pulses or disturbances lasting at least the debounce interval are not
reconstructed or rejected by this change. The 1 ms qualification threshold must
also be checked against actual sensor high and low widths on affected hardware.

For hardware verification, capture P2.6 on an affected BBS02B while reproducing
the load-dependent problem. Compare the raw pulse widths and periods with the
reported speed, then repeat with the patch and the speed sensor/limiter enabled.
Use a controlled bench setup first, and verify real acceleration, deceleration,
stop detection and configured speed limiting before road use. A clean build and
host simulation alone are not on-bike validation.

## Display current response tests (#123)

```sh
for target in BBS02 BBSHD TSDZ2; do
    cc -std=c99 -D"$target" -ffunction-sections -fdata-sections \
        -I src/firmware tests/display_current_test.c -Wl,--gc-sections \
        -lm -o /tmp/bbs-current-test || exit 1
    /tmp/bbs-current-test || exit 1
done
```

These tests call the production current-response handler and capture its UART
output. They pass on the existing code: there is no reproduced 999 W clamp in
this handler. They cover incomplete requests, all 331 inputs from 0 to 33 A in
0.1 A steps, values corresponding to power above 999 W, and the protocol maximum.
The power examples use `P = U * I`; they do not emulate the 800S firmware or
establish which voltage it uses. Unused handler sections are discarded at link
time so only the tested current getter and UART writer need mocks.

See [the #123 investigation](../doc/issue-123-display-power.md) for evidence and
the hardware information still required. This test is diagnostic evidence, not
a fix for the reported display behavior.


## Adaptive Studio and configuration v6

`sh tests/run.sh` also exercises the integer current-rise curve, segment duration,
monotonicity, target reductions, wraparound and interlocks with S-curve shaping
enabled. The EEPROM harness runs the production v5-to-v6 migration and verifies
persistence, preservation of old settings and corrupt-data recovery. Protocol
tests use the new 156-byte configuration and 161-byte maximum command frame.

`dotnet run --project tests/ConfigurationTests --configuration Release` checks
v6 binary/XML serialization, legacy compensation migration, shape validation and
UI/firmware curve reference values.

On Windows, run:

```pwsh
dotnet run --project tests/WindowsUiTests --configuration Release -- ui-preview
```

This renders the disconnected application offscreen at exact viewport sizes, checks all
six pages at 1000×650, 1280×720 and 1920×1080 in both themes, and writes PNG
screenshots. It checks workspace stretch, nonoverlapping cards, the wide System
layout without scrollbars, text colors, the connection toolbar and two-way curve
bindings. It neither connects to a controller nor arms bench controls. The
`Windows UI validation` workflow runs this suite on pull requests and uploads
`windows-ui-preview`. The runner desktop can be smaller than the tested viewport; offscreen layout
avoids the top-level HWND size cap. Screenshot dimensions are WPF logical pixels; physical
multi-monitor DPI behavior still needs a manual Windows check.

## Connection actions and firmware optimization

The same C runner compares the optimized ramp function with its frozen original
formula over every `uint16_t` progress and `uint8_t` smoothness input for all
three controller targets. The portable .NET suite tests the shared controller
operation guard and independent configuration snapshots. Windows UI tests also
exercise rejection through the real configuration/reset/calibration commands,
using an injected rejecting confirmation without opening a serial port.

See [scope, hardware review and build measurements](../doc/connection-actions-and-safe-optimization.md).

## Adaptive Power PAS and schema 7

`sh tests/run.sh` also runs `adaptive_power_test.c` for every controller target.
It tests the cadence-based watt envelope, current ceilings, invalid/sagging/
recovering battery voltage, global-compensation independence, all Standard/Sport
levels, throttle override, zero-demand coast and the existing safety interlocks.
It compares non-adaptive ramp outputs with the frozen previous implementation
and tests moving adaptive targets across clock wrap. Configuration and command
frame sizes remain 156 and 161 bytes.

Portable .NET checks exercise schema-7 binary/XML round trips, exact encoding of
all 255 budgets, schema-5/6 migration and rejection of ambiguous flag combinations
or unrepresentable budgets. Windows tests render Adaptive Power and verify the
watt input and preview bindings in both themes at all three viewport sizes.
See [the algorithm, memory measurements and research limitations](../doc/adaptive-power-pas.md).
