# PAS integrity, loaded SOC freshness and enabled-sensor fail-safe

This follow-up is measured against `7c84199`. It applies to BBS02/BBS02B, BBSHD
and TSDZ2 within their existing hardware capabilities. Adaptive Efficiency is now
the only adaptive PAS controller; no new PAS mode or configuration field is added.

## PAS minimum interval

Both sensor ISRs now apply the existing `PAS_SENSOR_MIN_PULSE_MS_X10` threshold
before accepting a PAS1 rising edge. The configured threshold is 50 timer ticks,
nominally 5 ms at the unchanged 100 microsecond sampling interval. The existing
strict `>50` counter convention accepts the first subsequent rising edge at
52 elapsed sample ticks, storing a 51-tick period. Normal supported pedal
cadences are well below the resulting upper cadence bound. The old “500 rpm”
comment describes 24 pulses/revolution; TSDZ2 uses 20 pulses/revolution.

A rejected edge cannot alter accepted pulse count, direction or cadence. It
still advances the saturating period counter and stop timeout, and raw edge
history still updates. A rejected high level therefore cannot become a delayed
rising edge simply by remaining high. The next valid pulse, direction reversal
and restart after a stop retain their original behavior. No buffer, debounce
state, timer configuration or GPIO assignment is added or changed.

This is an interval gate, not a universal electrical glitch filter. A short
pulse arriving after the minimum interval, or a sustained noisy pulse train
that resembles valid pedaling, can still be accepted. Bench captures are needed
to establish actual sensor pulse widths and disturbance behavior; software does
not reconstruct genuine missing pulses.

## SOC freshness without fabricated battery parameters

The display estimate remains voltage-based, using the configured full/empty
padding and the unchanged independent voltage filter. No battery chemistry,
internal resistance, capacity, rider power or remaining-distance estimate is
invented. Existing LVC and power compensation continue to use their own voltage
inputs/envelopes, not the displayed SOC.

- A first valid reading above 0 V and at/below 100 V initializes the display estimate. Zero, above
  100 V or an invalid configured voltage range cannot initialize it or satisfy
  the quiet dwell. The historical startup placeholder remains 70%.
- A positive motor-current command **or measured current above 0.5 A** marks a
  loaded interval. A zero command by itself is no longer considered unloaded.
- During uninterrupted load, the existing filtered terminal voltage can reduce
  the displayed SOC by **at most one percentage point per ten seconds**. Voltage
  rebound cannot increase SOC under load. There are no catch-up decrements after
  a missed deadline, so transient sag does not become a sudden percentage drop.
- After zero demand and measured current at/below 0.5 A remain for two seconds,
  the estimate can re-anchor to the live voltage; quiet updates repeat every
  two seconds. This can correct a conservative loaded estimate upward. A mode
  transition or invalid voltage restarts the dwell. Elapsed-time arithmetic is
  wrap-safe.

The existing timestamp is reused, and the percentage byte's previously unused
high bit records loaded/quiet control state. Both getters mask that flag;
normal and SW102 display mappings remain within 0–100. There is no new static
allocation, recording buffer, counter of energy/distance, logging or EEPROM write.

This improves freshness, not calibrated SOC accuracy. Sustained sag can still
bias the estimate downward, and the rate limit can lag a rapid real discharge.
An actual OCV/SOC model or coulomb counter would require battery-specific data
and a different scope; the firmware does not claim either.

## Enabled thermistor failure handling

The existing temperature-selection mask declares which physically supported
sensors are installed. The user explicitly approved this meaning. Disabled
sensors return the existing unavailable value; compile-time-unsupported sensors
remain unavailable rather than becoming faults. BBS02 has controller sensing,
BBSHD has controller and optionally motor sensing, and base TSDZ2 has neither
of these temperature inputs.

For an enabled BBS sensor, raw 10-bit ADC values **0–4 or 1022 and above** return
an internal fault sentinel before filtering, resistance division or BBSHD
NTC/PTC classification. This catches near-short/open rails and ADC reset values.
The low-end guard also prevents extreme controller NTC calculations from
exceeding the signed centidegree return range: ADC 2–4 formerly calculated
approximately 424–341 °C and could wrap negative; the lowest accepted ADC 5
remains representable at approximately 318 °C and triggers overtemperature.
The final BBSHD PTC LUT endpoint is included in its upper bound, preventing a
read past the table when resistance equals the final landmark.

The application commands zero immediately for a fault and reports the already
reserved Bafang controller/motor sensor status codes `0x15`/`0x14`. It reuses the
existing cached temperature bytes for fault state; it does not publish the
sentinel as a fabricated physical temperature, add a telemetry field or extend
logging. All demand sources, including throttle and walk, pass through this
stop. A subsequent valid reading clears the fault; no new persistent latch or
recovery timer is introduced. Motor output remains off while startup ADC data
is unavailable.

New defaults select **Controller** for BBS02/BBSHD and **Disabled** for TSDZ2.
**Saved selections remain unchanged.** On BBSHD, **All requires an installed
motor thermistor**. If it is absent, select Controller before riding. An enabled
but missing sensor intentionally stops the motor; silently disabling it would
contradict its declared presence. The Windows tooltip explains these choices.

The 5.1 kΩ divider and BBSHD temperature connections were checked against the
repository's `drawings/pcb/bbshd.sch` and `drawings/bbshd.pdf`; pin mapping, ADC
channel sequencing/scaling, thermistor constants and the existing NTC/PTC
classification policy are unchanged. Rail detection cannot establish every
sensor failure: plausible stuck values, incorrect sensor type, calibration,
missing optional hardware versus a disconnected enabled sensor, and the existing
single-reading NTC/PTC identification ambiguity need hardware-specific review.

## Retired Adaptive Power compatibility

The UI exposes only **Adaptive Efficiency**, without an experimental suffix;
its preview and budget bindings use the same name. The independent Adaptive
Power controller is removed. Existing cadence/torque/variable PAS remain.
Removing a UI qualifier does not establish experimentally verified efficiency.

Genuine historical v7/v8 power-only `0x80` settings migrate to current `0x88`
Efficiency settings while retaining budget/current/speed/throttle fields and
selecting the known connected/compiled motor profile. Unbound imported files
still require review against actual hardware. Genuine invalid combined v7
flags remain rejected. v5/v6 legacy voltage-compensation flags cannot activate
Efficiency. Current UART writes accept only the canonical Efficiency encoding;
use the matching updated Windows tool. No EEPROM/wire field is added.

The reported PAS watt-entry issue was withdrawn as a false positive. Its
rounding/ceiling logic is unchanged; property/preview names were updated for the
sole remaining controller. Schema 8 stays 156 bytes, with 6-byte levels and a
161-byte maximum request.

## Native builds, memory and simulations

Clean SDCC 4.5.0 builds use the same controller/compiler options:

| Target | Flash before / after / delta | Static RAM before / after | Linker stack before / after |
| --- | --- | --- | --- |
| BBS02 / BBS02B | 28,227 / 28,732 / +505 B | XRAM 860 / 860 B | 164 / 172 B available (+8) |
| BBSHD | 29,799 / 30,338 / +539 B | XRAM 874 / 874 B | 152 / 155 B available (+3) |
| TSDZ2 | Full image 16,780 / 16,988 / +208 B; CODE 16,047 / 16,255 | DATA 581 / 581 B; INITIALIZED 61 / 61 B | SSEG reservation 1 / 1 B |

No static RAM area grows. Foreground SOC/controller-temperature scratch operands
use reentrant stack storage instead of permanent compiler spills. Regression
limits explicitly allow the measured flash cost and enforce the improved RAM
layout; the limits are not physical MCU capacities.

The same production-control probe runs all 101 smoothness values, watt budgets,
retargets and hard stops. New checks also verify propagation of enabled sensor
faults through the existing status path. Baseline and updated control probes
measure 31/31/74 B stack extensions on BBS02/BBSHD/TSDZ2. The separate probe using
real sensor/SOC implementations measures 10/10/48 B before and 24/24/51 B after.
These are finite workloads excluding their main/fixture frame and interrupts,
not complete physical peak-stack measurements.

Full-control probe simulator clocks (baseline / updated) are
2,047,382,400 / 2,059,145,604 for BBS02,
2,048,726,496 / 2,060,473,296 for BBSHD, and
31,292,725 / 31,736,234 for TSDZ2. These finite workloads execute the extra
loaded-SOC/fault paths; they are not motor-efficiency or hardware deadline
measurements.

Six isolated sensor-ISR brackets, in simulator clocks:

| Input case | BBS02/BBSHD baseline → updated, C52 model | TSDZ2 baseline → updated |
| --- | --- | --- |
| Idle | 2256 → 2256 | 70 → 70 |
| Valid forward pulse / wheel edge | 2664 → 2808 | 84 → 89 |
| Too-fast PAS edge | 2508 → 2580 | 81 → 77 |
| Valid direction reversal | 2652 → 2796 | 84 → 89 |
| Stop timeout | 2364 → 2364 | 74 → 74 |
| Saturated counter | 2364 → 2364 | 74 → 74 |

The brackets include mock pin accesses and fixture call overhead, exclude real
interrupt entry/exit and the BBS shared timer wrapper, and use a generic C52
model rather than STC15's 1T instruction timing. They are reproducible regression
measurements, **not physical 100 µs deadline certification**. The real compiled
production ISR is additionally checked for helper calls and multiply/divide
instructions; none is introduced. Timer configuration and sampling period stay
unchanged. Actual GPIO/interrupt timing and priority/preemption require bench
measurement with the real controller and motor commutation active.

Validation includes 651 host PASS groups, 96 portable .NET checks, 26 Python
CI/release/research tests, all three native builds/memory guards, and both MCU
probe suites. Representative PAS/SOC assertions fail against `7c84199` on all
three targets; thermistor-fault assertions fail on both BBS targets. TSDZ2
unavailable sensors intentionally remain unchanged. Windows layout/runtime tests
exercise the renamed sole variant and preview bindings in both themes.

No physical controller was connected or flashed. Bench-test installed sensors,
fault recovery, braking, current, LVC and thermal shutdown with the driven wheel
clear before riding. No measured motor-efficiency, range or calibrated SOC gain
is claimed.
