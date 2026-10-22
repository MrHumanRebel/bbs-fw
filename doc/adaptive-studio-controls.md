# Adaptive Studio, global compensation and current-rise curves

## Layout and icon

The selected-tab content now stretches to the complete workspace. The selected
navigation item's accent is scoped to its header instead of inheriting into
form text. Local checkbox styles inherit the shared checkbox template.

System cards are measured at their natural heights and distributed into the
shortest available column. Column count follows the available logical width,
with a minimum card width of 310 pixels and 16-pixel gaps. Small windows scroll
vertically when necessary; wide windows use additional columns. Assist editors
use the same panel alongside a constrained, virtualized level list. Connection
heading and Refresh have separate layout slots. The log scrollbar is automatic;
scrollbars follow the active palette.

The generated motor/lightning emblem is embedded in the executable, window icon
and header. Its ICO contains 16, 24, 32, 48, 64, 128 and 256-pixel frames. The app
still uses native WPF, system fonts and no charting or web UI packages.

## One compensation switch

System → Compensate battery voltage drop controls cadence/variable PAS across
all ten levels in both Standard and Sport. Level current ceilings, braking,
thermal/LVC limits and speed limits retain priority. Torque PAS already derives
current from power and voltage, so it is not compensated twice; throttle output
is unchanged by this switch.

The curve and switch are global configuration fields, rather than mirrored
per-level flags. Firmware and the Windows app use **configuration schema 6**,
with a 154-byte v5 prefix followed by two bytes:

| Offset | Field | Range/default |
| --- | --- | --- |
| 154 | `use_voltage_compensation` | 0/1, default 0 |
| 155 | `current_ramp_smoothness_percent` | 0–100, default 0 |

Total payload is 156 bytes; the largest write command is 161 bytes. Both MCU
families retain bytewise serialization, without endian-sensitive added fields.

Firmware upgrades read valid v5 EEPROM data, preserve its settings, clear the
legacy per-level compensation flags, enable the global switch if any legacy
level enabled it, and persist v6. This intentionally applies compensation to all
cadence/variable PAS levels once enabled. With no legacy flags it remains off.
The new curve defaults to linear. A failed migration write keeps the migrated
RAM settings; a later boot retries the original v5 data. Corrupt EEPROM retains
the existing default-recovery behavior.

The tool can read v1–v5 controller configurations and import old XML. Old v5/XML
flags receive the same global migration. Writing requires v6 firmware, enforced
by the existing version check: update firmware and tool together. Export a
configuration backup before flashing; flash tools may erase EEPROM. Restart the
controller after writing settings so cached controller parameters agree.

## Editing the ramp

System → Current response provides:

- An editable curve: drag Duration horizontally or Shape vertically.
- Average current rise rate in A/s (the existing 1–255 encoded value).
- Equivalent full rise time in seconds, quantized to that A/s value.
- S-curve blend from 0% to 100%, with a slider and numeric entry.
- Keyboard adjustments when the chart has focus: Left/Right changes the rate;
  Up/Down changes softness.

The full-rise reference time is `MaxCurrentAmps / CurrentRampAmpsSecond`. A
nonlinear rising segment starts at the current ramp output and ends at the
requested current. Its duration scales with that current difference. Retargeting
starts a fresh segment from the current output. Targets that fall, disabled
ramping and safety stops immediately reset the rising envelope; the existing
ramp-down and safety logic remain in their original order.

For progress `p` from 0 to 1000, the integer smooth curve is
`floor(p*p*(3000-2*p)/1000000)`. The stored softness linearly blends this with
`p`. At 0% the ramp is linear and retains the existing A/s setting. Elapsed-time
segments remove the old one-percent-per-application-tick ceiling at high rates;
actual updates remain quantized to the 5 ms application loop. At 100%, start
and finish slopes approach zero and peak slope is approximately 1.5 times the
average A/s. Thus A/s is an average for nonlinear ramps, not an instantaneous
slew ceiling. Target current and motor protections still cap the output.

The native chart uses the same integer formula. It previews a stable request
from zero to maximum configured current, not real motor telemetry. Other limits
and changing requests can alter actual output. On the 8051, arithmetic scratch
space uses reentrant stack storage to fit the direct-RAM budget.

Throttle input calibration and Keep Current have additional reference plots.
The former shows the normalized voltage input before response mapping and
current limits. The latter labels its 150 rpm reference endpoint explicitly;
actual cadence endpoints depend on controller and per-level cadence settings.
These plots visualize the existing settings and do not add throttle/torque maps.

## Validation

- Production C host tests cover EEPROM migration, configuration frames, curve
  values, monotonicity, duration, changed targets, wraparound and safety stops.
- Portable .NET tests cover binary/XML migration and the same curve values.
- Clean SDCC builds and HEX validation cover BBS02, BBSHD and TSDZ2.
- A separate Windows workflow exercises both themes and three viewport sizes,
  checks layout and bindings, and uploads screenshot previews.

Local Linux builds cannot execute the Windows visual suite or establish motor
behavior. Inspect Windows screenshots and verify real DPI, input release, ramp
feel, current ceilings and controller protections on hardware before riding.
