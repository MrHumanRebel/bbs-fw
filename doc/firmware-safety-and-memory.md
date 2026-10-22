# Current/power safeguards and firmware size

## Configuration determines the electrical envelope

Adaptive Power uses the configured battery-current limit; it does not invent a
single motor watt rating. The existing platform ceilings are BBS02 **30 A**,
BBSHD **33 A**, and TSDZ2 **20 A**. These are software upper bounds, not a
recommendation to use that current on every controller revision, motor or battery.
The configured limit must respect the actual equipment's ratings.

A level's maximum configurable PAS budget is:

```
floor(min(2550 W, MaxBatteryVolts * MaxCurrentAmps * LevelCurrentPercent / 100) / 10) * 10
```

The tool also bounds current by the selected platform before calculating its
preview. Firmware rejects writes above its own platform's current ceiling.
For example, 25 A × 48 V permits 1200 W; 33 A × 52 V permits 1716 W, encoded
as at most 1710 W. A 50% level current limit halves that envelope. Zero current
headroom cannot become a positive budget; select Motor Disabled or configure
sufficient headroom before writing an Adaptive Power level.

**Max Battery Voltage means the fully charged voltage**, not the pack's nominal
48 V or 52 V label. Those examples illustrate the calculation; use the correct
full-charge voltage and LVC for the installed battery.

At runtime the power-to-current conversion uses the higher of measured voltage
and the slow voltage filter, rounds current downward to the command's 1%
resolution, and caps it at the level's current percentage. Thus a high watt
request cannot request more than the configured battery current. Optional
throttle retains its separate current limit and can exceed the PAS watt budget;
it still obeys the global current limit and final protections. This remains an
electrical input-power model, not mechanical shaft power or a phase-current limit.

The Windows tool updates all twenty level ceilings when global current/voltage
changes and reduces a budget when its level current limit falls. Input rounding
never increases a watt request. File/controller reads retain values for review;
write validation rejects an excessive budget, including direct model changes.
The reviewed snapshot is checked against the **actual connected controller**.

## Protection reductions have priority

Every normal demand source passes through the same final protections: cadence
PAS, variable PAS, torque PAS where supported, Adaptive Power, cruise, physical
or virtual throttle, and walk assist.

- The normal falling-current ramp can no longer raise a thermal or LVC reduction.
  Brake, speed, shift, power-block and virtual-control interlocks remain effective.
- The existing 80–85 °C thermal ramp remains: 100% at 80 °C, down to 20% at 85 °C.
  **Above 85 °C the command becomes zero**, rather than remaining at 20% indefinitely.
  Limiting uses the full signed temperature reading; cached display temperatures
  saturate instead of wrapping above 127 °C. Protection requires an available,
  enabled, working sensor; some supported motors/controllers lack these sensors.
- A raw voltage at/below configured LVC, zero, or above the protocol's valid
  100 V range immediately commands zero for every demand source. The existing
  minimum-following filtered LVC ramp remains; it is not changed to a recovery
  estimator. Its sampling interval now survives millisecond-counter wrap.
- Reported LVC, Hall and current-sense motor faults block every demand source.
  The undocumented BBS02 `MOTOR_ERROR_POWER_RESET` bit keeps its existing error
  reporting without acquiring a speculative new interlock. Unknown status bits
  and the motor-control firmware's fault handling retain their existing meanings.
- A collapsed Keep Current cadence interval no longer divides by zero. Torque
  PAS's minimum-current behavior cannot override a zero level current ceiling.
  A zero-current PAS level also suppresses pretension while retaining optional
  throttle under its independent limit.
- The initialization conversion uses unsigned multiplication so 33 A becomes
  33000 mA without overflowing a signed 16-bit intermediate.
- Voltage-padding multiplication is widened before multiplication, preventing
  16-bit overflow at the configuration format's voltage-range extremes.

A valid configuration write or reset immediately requests zero and disables the
motor, then latches a stop **until controller restart**. This prevents a new
software current limit from being combined with the motor MCU's old initialized
limit. A failed EEPROM write also leaves this stop latched. Existing Windows
confirmations remain mandatory and default to No.

Both boot loading and UART writes validate controller current, voltage ordering,
wheel diameter, speed-sensor divisor, throttle calibration ordering/ranges,
percentage limits and Adaptive Power combinations/budgets. Invalid UART settings
are rejected before changing live configuration. Invalid EEPROM settings follow
the existing factory-default recovery path. An otherwise valid old schema-7
Adaptive Power budget above its ceiling is reduced in RAM without discarding
other settings; zero available headroom clears its adaptive flag. A subsequent
save persists those corrected values. Schema-5/6 migration remains supported.

Configuration stays **156 bytes**, each assist level **6 bytes**, and the maximum
UART request **161 bytes**. No new configuration fields or per-level arrays are
introduced. The restart latch is a single bit on 8051 and a bool on STM8.

## Size reductions and regression budgets

Private helpers share one foreground-only current scratch byte. Known XRAM
pointers use 8051-specific address qualifiers instead of generic-pointer runtime
access. Startup-only voltage intermediates use temporary storage. EEPROM/level
iteration advances within the configuration's byte representation instead of
repeatedly dividing an index into mode/level coordinates. No helper is called
from an ISR; no wire structure, hardware address or interrupt contract changes.

On STM8, integer wheel-speed conversion removes the runtime floating-point
library from the firmware. The rational coefficient is checked against the old
single-precision formula over every supported 10.0–40.0-inch wheel and every
byte-sized speed input. One explicit rounding-boundary correction preserves
legacy output. Display data has a separately checked reachable range of 0–204
(the maximum 127 °C scaled by 161/100). BBS thermistor floating-point calculations
remain unchanged. The bounded thermal ramp uses an exact unsigned 16-bit mapping
whose maximum intermediate product is 40000.

| Target | Flash/CODE before / after | Static RAM before / after | Linker stack information before / after |
| --- | --- | --- | --- |
| BBS02 | Flash 25,805 / 25,762 bytes | XRAM 904 / 861 bytes | 150 / 157 bytes available |
| BBSHD | Flash 27,361 / 27,318 bytes | XRAM 918 / 875 bytes | 138 / 145 bytes available |
| TSDZ2 | CODE 15,655 / 14,733 bytes | DATA 587 / 581; INITIALIZED 61 / 61 bytes | SSEG reservation 1 / 1 byte |

Measurements use clean SDCC **4.5.0** builds, comparing the preceding Adaptive
Power implementation (`f116eab`) with this hardening/optimization patch. The table
reports firmware flash/CODE, not Intel HEX text-file size. Linker stack capacity
and STM8 SSEG reservation are not measurements of peak runtime stack use.

`ci/check_firmware_size.py` rejects flash/static-RAM growth or reduced 8051 stack
capacity beyond the preceding implementation's budgets. These regression budgets
are deliberately distinct from MCU ratings. PR CI builds all three controllers,
runs host regressions, and executes the extracted shipping conversion using
SDCC/uCsim; release builds also enforce the size budgets. No global compiler
optimization or unsafe-memory-read flags are enabled.

## Verification limits

GPIO/pin assignments, ADC scaling, UART drivers/baud rates, ISR bodies, watchdog
settings, Bafang motor-MCU initialization and STM8 commutation/FOC constants are
unchanged. Host tests mock hardware I/O; the processor simulator validates
arithmetic, not the complete physical controller.

No physical controller was connected or flashed. Peak stack watermarking,
current-regulation error, sensor calibration/failure behavior, thermal response,
voltage sag, phase current and real motor/battery ratings require hardware
validation. An ampere-times-voltage ceiling alone cannot guarantee that a motor
or battery will not be damaged. Bench-test with the driven wheel clear and verify
braking, cutoff, thermal response and current limits before riding.
