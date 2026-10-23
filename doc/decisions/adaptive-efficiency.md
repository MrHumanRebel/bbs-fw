# ADR: bounded Adaptive Efficiency without runtime measurement storage

Status: implemented for review. This decision applies to the user's updated requirements of 22 October 2026. Those requirements supersede the measurement, logging, telemetry and diagnostic additions requested by the supplied research task document. The supplied files remain unchanged as research evidence.

## Evidence and uncertainty

There are exactly three selectable optimization profiles: BBS02B, BBSHD and TSDZ2. All 93 factory-reference rows, all four BBSHD landmarks, all nine external trainer observations, all 29 parameter rows, their JSON mirror, manifest and 14 source records are checked offline. `research/efficiency/tools/control_evidence.py` produces a reproducible evidence summary; none of these tables is linked into any controller's RAM or firmware image.

The BBS02B sweep is an accepted engineering proxy with an unidentified source revision. Output crank speed is not automatically rider cadence. Older Grin BBS02 parameters remain related-family references. BBSHD has its own four-point source and independently measured specimen parameters. TSDZ2 winding R/L conventions are unidentified, and the nine trainer observations concern TSDZ2B, not the supported TSDZ2 controller. No source supplies a verified, transferable two-dimensional efficiency surface or calibrated thermal RC model.

Consequently the firmware neither infers motor torque, phase current, rider watts nor physical efficiency. No published resistance, inductance, maximum-efficiency rpm, nominal motor wattage or 70 °C trainer endpoint becomes a hardware control constant. Actual human power requires per-bike torque calibration; this feature does not introduce such an estimate. A failed TSDZ2 torque sensor disables Adaptive Efficiency PAS, while independently configured throttle retains its original behavior.

## Control policy

Adaptive Efficiency is the sole adaptive PAS variant alongside legacy cadence/torque/variable PAS. The previously separate Adaptive Power controller has been removed; its historical settings migrate to Efficiency without adding configuration fields. Its base remains the bounded, voltage-aware watt budget. Profile-specific low-cadence thresholds are conservative research-informed policy choices, not fitted factory-map cutoffs:

| Profile | Enter below | Release at/above | Minimum request multiplier |
|---|---:|---:|---:|
| BBS02B | 35 rpm | 45 rpm | 90% |
| BBSHD | 40 rpm | 50 rpm | 95% |
| TSDZ2 | 45 rpm | 55 rpm | 90% |

After three seconds of valid demand, low-cadence shaping arms when the requested fraction of its own PAS watt budget reaches 40%, or when the unshaped current command reaches 40% of the independent level current ceiling. The alternative preserves operation when the current ceiling clips the full watt budget. It releases below 30% on both request references, or at the profile's cadence-release threshold. The 30% release point is above the 25% launch floor so a lightly requested quarter-budget command can release the hysteresis. It acts only above 75% of the configured speed ceiling. The multiplier recovers whenever that low-cadence/speed condition no longer applies. Speed approach no longer adds an independent 97% trim or 93% release threshold. Each eligible 100 ms interval changes it by one percentage point, with no catch-up steps. Positive current reductions round down, preserving the profile's minimum multiplier even with coarse current resolution. Missing configured wheel speed retains the unshaped bounded PAS budget; missing pulses while standing cannot trigger a low-speed derate. Hill starts and low-speed climbing therefore retain the existing budget.

The budget fraction is already defined by `clamp(cadence_x10 / 600, 1/4, 1)`.
Thus its 40% entry condition is `cadence_x10 >= 240`, and its below-30% release
condition is `cadence_x10 < 180`. The positive-command check remains mandatory.
Factoring out the common watt budget avoids new divisions, buffers or state.
The level-current comparisons reduce exactly to `current * 5 >= level * 2`
and `current * 10 < level * 3`; their largest valid product is 1000, within
16 bits. This is an unshaped **request fraction before whole-percent current
quantization**, not measured electrical/mechanical motor load. It does not imply
that two motors or two budgets have identical physical efficiency.

The audited 48 V / 25 A / 250 W / 34 rpm request is 11% current: the old
40%-of-level gate never armed at a 100% level ceiling. The new budget-relative
route arms after the existing launch grace, still requiring the original speed
condition before any trim. Tests isolate it at 80% of the speed ceiling, outside
the soft speed-approach region. BBS02B then commands 10% current. At
48 V / 30 A, BBSHD also arms with a 250 W budget but its 5% trim of an 11%
command is smaller than one current step; the command stays 11%. At 500 W it
changes 22% to 21%. Keeping the existing trim bound takes priority over rounding
that small correction up. No physical efficiency gain follows from these
command-only examples.

This is an efficiency-aware load-management hypothesis, not proof of a physical optimum. It cannot identify gradient, aerodynamic drag, gearing or human contribution. The selected speed remains the existing safety ceiling with the coordinated soft approach described below; it is not an independently guaranteed cruise-speed setpoint. For 100 km/h global and 20% level speed, that ceiling is 20 km/h. Original display/global speed constraints still apply. The policy can lower speed or change rider effort in some conditions; no efficiency gain is established by that change alone.

## Coordinated speed approach, sag recovery and target jitter

Adaptive Efficiency uses one speed envelope after PAS/throttle arbitration. The
old independent 97% correction has been removed. The existing level speed limit,
display/global constraints and exact hard cutoff remain; no new Cruise mode,
setpoint, configuration field or motor-profile threshold is introduced.

The soft region retains its configured 3 km/h width, capped at half the speed
limit. Below this region the speed envelope passes the bounded PAS request
unchanged. Inside it, let `r` be the remaining fraction of the region, truncated
to an integer percentage from 100 at entry to 0 at the speed limit. Its gain is
`floor(r*r/50)` for `r <= 50`, otherwise
`100 - floor((100-r)*(100-r)/50)`. Two quadratics meet at 50% with equal slope,
and have zero slope at both outer endpoints before quantization. The result is
multiplied into the already bounded PAS current and rounded down. At or above
the configured speed limit the original immediate zero-current branch takes
precedence. A configured zero-width region therefore still gives a hard cutoff.

Nominal 20 km/h limit / 3 km/h soft-region design points, before wheel-RPM and
whole-percent current quantization:

| Speed | PAS speed-envelope gain |
| --- | --- |
| At or below 17 km/h | 100% |
| 18 km/h | 77% |
| 18.5 km/h | 50% |
| 19 km/h | 21% |
| 19.5 km/h | 5% |
| At or above 20 km/h | 0% |

This envelope reduces unnecessary near-limit demand; it does not estimate
human watts, drag, gradient or motor torque, and cannot guarantee holding the
speed ceiling under every load. Three synthetic road-response cases verify
settling after a load increase and removal, without a large limit cycle in that
model. They do not establish road behavior or a physical efficiency improvement.
Throttle winners bypass this PAS curve entirely and keep their original linear
speed envelope, override options, immediate demand arbitration and protections.

Soft LVC can recover only while the active level uses Adaptive Efficiency and
independent throttle is not the winning demand. The existing four-byte filter
allocation now contains a 16-bit centivolt value and an eight-sample dwell,
initialized without byte-order assumptions. Downward filtering is algebraically
equivalent to the original signed `/8` update. Upward filtering uses `/32`, only
after eight consecutive eligible samples at the existing >125 ms interval, with
raw voltage at least 0.5 V above both the filtered value and soft-ramp endpoint.
The 0.5 V deadband, invalid voltage, downward movement, an ineligible mode/throttle
or a sampling gap over 250 ms clears the dwell. Recovery is bounded by the live
reading and is four times slower than sag tracking. It may stop within the
0.5 V deadband; it never invents a full battery or resets a motor/BMS fault.
Other modes and throttle do not drive upward recovery. They share the existing
filter history, including a recovery completed earlier in Adaptive Efficiency.
Hard LVC, invalid-voltage shutdown and all final protective envelopes remain
immediate; the external BMS and motor-fault handling are unchanged.

The Q7 tracker retains its timer and velocity across equal or at-most-one-percent
falling Efficiency targets, preventing small cadence changes from continually
postponing the next scheduled rise. It still clamps position/output to every
lower request immediately. Zero demand, bypass and larger falls reset the
tracker; no deadband holds excess current above a current/power/safety envelope.
Larger upward changes remain rate/acceleration bounded and missed deadlines
never generate catch-up steps. Existing optional comfort decay retains its own
safety bypass. Legacy cadence/torque/variable PAS keep their original tracker/ramp behavior;
retired power-only settings now use the sole Efficiency controller.

## Memory and scheduling

The existing five-byte cadence cache is a union with Adaptive control state. The existing seven-byte rising-ramp state is a union with a six-byte fixed-point tracker. Members are mutually exclusive by PAS variant, and level changes reset the correct union member. Native compile-time assertions enforce these sizes. The existing `app_process()` handles all work; no new task, interrupt, telemetry opcode, event type, buffer, measurement array, runtime statistic, history or heap allocation is introduced. Existing logging and telemetry implementations are preserved.

The Adaptive tracker uses Q7 percentage position and velocity, advancing at most one 20 ms step per invocation. It retains position and velocity across upward target changes, avoiding the old stretched-origin plateau. All new tracker arithmetic fits 16 bits: the largest rate product is 255 × 256 = 65280. Distance feedback eases the approach; smoothness controls acceleration. Arrival, reduced targets and hard shutdown may truncate a step. Missed application deadlines cannot generate a catch-up surge. The old segmented ramp remains byte-equivalent for non-Adaptive PAS across all 101 smoothness values.

## Configuration and protections

Schema 8 keeps the exact 156-byte configuration and 161-byte maximum request. Existing byte 154 packs bit 0 voltage compensation, bits 1–2 profile (0 BBS02B, 1 BBSHD, 2 TSDZ2; 3 rejected), and bit 3 optional comfort decay. Reserved bits are rejected. No EEPROM field or page is added. The explicit Adaptive Efficiency variant combines existing Adaptive and Variable bits; v7 considered that combination invalid. Genuine v5/v6 migration never selects it; retired v7/v8 power-only budgets now migrate to Efficiency. Migration never enables comfort. This follows the later request to remove the separate Adaptive Power controller. The firmware and write guard reject enabled efficiency settings for a different compile target. The BBS02 build mapping still requires independently verified compatible BBS02B STC/UART hardware; profile selection cannot establish PCB compatibility.

Optional comfort decay operates only while valid forward-pedaling demand remains positive. Its output cannot exceed the current full-budget electrical envelope or per-level current limit, including after voltage recovery. Pedal stop, invalid voltage, braking, hard LVC, thermal, speed, shift, motor fault and configuration-change shutdown retain priority. Partial protection envelopes disable comfort even when rider demand is already below the protective cap. Throttle remains independent of the PAS-only watt budget and has an explicit GUI disclosure.

## Validation limits

Host tests, configuration/XML and UI binding tests, native SDCC builds and MCU instruction-set simulations cover the implemented invariants. Linker stack availability is not measured peak interrupt-plus-call stack use. Off-target timing is not a hardware deadline guarantee. Actual controller revision checks, current/voltage calibration, stack watermarking with interrupts, transient capture, wheel-clear safety tests and matched road A/B runs remain physical validation work. No percentage range improvement or universal safe hardware power rating is claimed.

## Subsequent sensor/SOC requirements

The user approved using the existing temperature enable mask as the physical
sensor-presence declaration. New defaults therefore enable only a supported
controller sensor; persisted choices are preserved. See the [sensor/SOC report](../sensor-soc-hardening.md)
for raw ADC fault handling, PAS spacing and conservative loaded-SOC freshness.
The public mode label is Adaptive Efficiency, without an experimental qualifier;
that naming change does not establish a measured physical efficiency gain.
