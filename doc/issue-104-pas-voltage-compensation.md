> Configuration v6 supersedes the original per-level UI described below.
> Compensation is now one System switch for both profiles. Existing v5 flags
> migrate to the global setting; see [adaptive controls](adaptive-studio-controls.md).

# Optional battery voltage compensation for PAS (#104)

The pasted feature request is issue #104. Issue #105 describes a separate
PAS/throttle interaction and is not the feature implemented here.

## Use

In the updated Windows configuration tool, select a cadence or variable PAS
level and enable **Compensate battery voltage drop**. This is independently
configurable for each Standard and Sport level, and defaults to off for existing
and reset configurations. Write the configuration to the controller as usual.
Set **Max Battery Voltage** to the actual full-charge voltage of the pack.

The firmware treats the unmodified PAS demand as a full-charge reference and
calculates:

```
compensated current = original current * full-charge voltage / filtered voltage
requested current = min(compensated current, level Max Current, 100%)
```

This is voltage compensation, not an increase proportional to the displayed SOC.
It keeps approximate electrical input power consistent; it does not measure
mechanical output power or compensate changing motor efficiency.

For a level with Max Current = 100% and Keep Current = 60%, using a 58.8 V
full-charge reference:

| Filtered battery voltage | Settled Keep Current request |
| --- | --- |
| 58.8 V | 60% |
| 54.0 V | 65% |
| 48.0 V | 73% |
| 45.0 V | 78% |

Current requests have 1 percentage point resolution and are rounded down. The
ideal value at 48 V is 73.5%. At a 40% level ceiling the same 60% Keep Current
means 24% original controller current, rising to 29% at 48 V, still below 40%.

## Scope and limits

- Compensation runs only inside an active cadence/variable PAS calculation,
  after the Keep Current calculation and before throttle selection and limits.
- A dominant throttle request keeps its original mapping. An unchanged smaller
  throttle request can naturally be overtaken by the compensated PAS request.
- Torque PAS already divides requested pedal power by measured voltage, so it
  is not compensated a second time. Its checkbox is disabled.
- Walk assist, pretension without active PAS, cruise and throttle-only levels
  retain their existing requests. Cruise already requests the level's current
  ceiling and has no compensation headroom under the preserved limit.
- Full-current acceleration and any other PAS request already at the level's
  ceiling cannot gain more power. Maintaining the ceiling takes priority over
  constant power.
- Speed, thermal, low-voltage, shift and brake restrictions run afterward.
  Existing current ramps remain in place. This feature cannot maintain full
  power in the low-voltage protection region.
- Unavailable/out-of-range voltage, invalid full/cutoff configuration and a
  filtered voltage at or below cutoff disable the boost. Voltage above the
  reference does not reduce the existing request.

The separate voltage filter initializes from the first valid reading and updates
every 125 ms with `filtered += (sample - filtered) / 16`, in both directions.
Its nominal time constant is approximately 2 seconds. It uses millivolt internal
precision, handles the system clock wrapping, and is independent of the SOC
estimate that updates only when the motor is idle. Filtering avoids following
each short load sag; it does not remove the battery's physical current limits.

## Configuration compatibility

The feature uses the previously unused `0x80` bit of the existing per-level
assist-flags byte. The EEPROM/wire layout remains version 5 and 154 bytes. No
settings reset or EEPROM migration is needed. Default flags leave it off.
The updated tool preserves the bit in binary and XML configuration round trips.
Use the updated tool for XML files containing `VoltageCompensation`; older tools
do not recognize that enum name. Older firmware ignores the new bit and does not
provide this feature.

## Validation

Run `sh tests/run.sh` from the repository root. The new suite includes the actual
`app.c` and `battery.c`, with mocked time and hardware. It exercises the complete
`app_process()` path, not a copy of the formula. The 51 PAS cases across the three
controllers cover enabled/disabled behavior, current ceilings, throttle,
variable PAS, inactive PAS, filtering in both directions, clock wrap, invalid
readings/configuration, walk mode and downstream safety restrictions. The
controller-specific cases test BBS shift protection and TSDZ2 torque behavior.

Run `dotnet run --project tests/ConfigurationTests` with .NET 8 for binary/XML
round trips, flag isolation, default-off behavior and view-model notifications.
The full WPF application can be cross-built on Linux using the existing project
with `-p:EnableWindowsTargeting=true`; interactive UI checks require Windows.

All three firmware targets must be rebuilt separately with `make clean` before
switching `TARGET_CONTROLLER`. The BBS02 image also includes the earlier #114
sensor-filtering patch in this workspace. Hardware ride testing remains needed:
verify the configured battery voltages, begin at a low assist level, compare
steady PAS input power at different charge states, and check braking and limits
before normal use.
