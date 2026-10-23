# ADR: bounded Adaptive Efficiency without runtime measurement storage

Status: implemented for review. This decision applies to the user's updated requirements of 22 October 2026. Those requirements supersede the measurement, logging, telemetry and diagnostic additions requested by the supplied research task document. The supplied files remain unchanged as research evidence.

## Evidence and uncertainty

There are exactly three selectable optimization profiles: BBS02B, BBSHD and TSDZ2. All 93 factory-reference rows, all four BBSHD landmarks, all nine external trainer observations, all 29 parameter rows, their JSON mirror, manifest and 14 source records are checked offline. `research/efficiency/tools/control_evidence.py` produces a reproducible evidence summary; none of these tables is linked into any controller's RAM or firmware image.

The BBS02B sweep is an accepted engineering proxy with an unidentified source revision. Output crank speed is not automatically rider cadence. Older Grin BBS02 parameters remain related-family references. BBSHD has its own four-point source and independently measured specimen parameters. TSDZ2 winding R/L conventions are unidentified, and the nine trainer observations concern TSDZ2B, not the supported TSDZ2 controller. No source supplies a verified, transferable two-dimensional efficiency surface or calibrated thermal RC model.

Consequently the firmware neither infers motor torque, phase current, rider watts nor physical efficiency. No published resistance, inductance, maximum-efficiency rpm, nominal motor wattage or 70 °C trainer endpoint becomes a hardware control constant. Actual human power requires per-bike torque calibration; this feature does not introduce such an estimate. A failed TSDZ2 torque sensor disables experimental PAS, while independently configured throttle retains its original behavior.

## Control policy

Adaptive Efficiency is an explicit PAS variant, separate from unchanged Adaptive Power and legacy cadence/torque/variable PAS. Its base remains the bounded, voltage-aware watt budget. Profile-specific low-cadence thresholds are conservative experimental policy choices, not fitted factory-map cutoffs:

| Profile | Enter below | Release at/above | Minimum request multiplier |
|---|---:|---:|---:|
| BBS02B | 35 rpm | 45 rpm | 90% |
| BBSHD | 40 rpm | 50 rpm | 95% |
| TSDZ2 | 45 rpm | 55 rpm | 90% |

After three seconds of valid demand, low-cadence shaping arms when the requested fraction of its own PAS watt budget reaches 40%, or when the unshaped current command reaches 40% of the independent level current ceiling. The alternative preserves operation when the current ceiling clips the full watt budget. It releases below 30% on both request references, or at the profile's cadence-release threshold. The 30% release point is above the 25% launch floor so a lightly requested quarter-budget command can release the hysteresis. It acts only above 75% of the configured speed ceiling. Near 97% of that ceiling the same bounded trim reduces unnecessary demand regardless of cadence. Below 93%, the multiplier recovers unless the sustained low-cadence condition still applies. Each eligible 100 ms interval changes it by one percentage point, with no catch-up steps. Positive current reductions round down, preserving the profile's minimum multiplier even with coarse current resolution. Missing configured wheel speed falls back to unshaped Adaptive Power; missing pulses while standing cannot trigger a low-speed derate. Hill starts and low-speed climbing therefore retain the existing budget.

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
condition before any trim. Tests isolate it at 80% of the speed ceiling, below
the independent 97% speed-trim route. BBS02B then commands 10% current. At
48 V / 30 A, BBSHD also arms with a 250 W budget but its 5% trim of an 11%
command is smaller than one current step; the command stays 11%. At 500 W it
changes 22% to 21%. Keeping the existing trim bound takes priority over rounding
that small correction up. No physical efficiency gain follows from these
command-only examples.

This is an efficiency-aware load-management hypothesis, not proof of a physical optimum. It cannot identify gradient, aerodynamic drag, gearing or human contribution. The selected speed remains the existing safety ceiling, including its original soft approach; it is not an independently guaranteed cruise-speed setpoint. For 100 km/h global and 20% level speed, that ceiling is 20 km/h. Original display/global speed constraints still apply. The policy can lower speed or change rider effort in some conditions; no efficiency gain is established by that change alone.

## Memory and scheduling

The existing five-byte cadence cache is a union with Adaptive control state. The existing seven-byte rising-ramp state is a union with a six-byte fixed-point tracker. Members are mutually exclusive by PAS variant, and level changes reset the correct union member. Native compile-time assertions enforce these sizes. The existing `app_process()` handles all work; no new task, interrupt, telemetry opcode, event type, buffer, measurement array, runtime statistic, history or heap allocation is introduced. Existing logging and telemetry implementations are preserved.

The Adaptive tracker uses Q7 percentage position and velocity, advancing at most one 20 ms step per invocation. It retains position and velocity across upward target changes, avoiding the old stretched-origin plateau. All new tracker arithmetic fits 16 bits: the largest rate product is 255 × 256 = 65280. Distance feedback eases the approach; smoothness controls acceleration. Arrival, reduced targets and hard shutdown may truncate a step. Missed application deadlines cannot generate a catch-up surge. The old segmented ramp remains byte-equivalent for non-Adaptive PAS across all 101 smoothness values.

## Configuration and protections

Schema 8 keeps the exact 156-byte configuration and 161-byte maximum request. Existing byte 154 packs bit 0 voltage compensation, bits 1–2 profile (0 BBS02B, 1 BBSHD, 2 TSDZ2; 3 rejected), and bit 3 optional comfort decay. Reserved bits are rejected. No EEPROM field or page is added. The explicit Adaptive Efficiency variant combines existing Adaptive and Variable bits; v7 considered that combination invalid. Genuine v5/v6/v7 migration never selects it and never enables comfort. The firmware and write guard reject enabled efficiency settings for a different compile target. The BBS02 build mapping still requires independently verified compatible BBS02B STC/UART hardware; profile selection cannot establish PCB compatibility.

Optional comfort decay operates only while valid forward-pedaling demand remains positive. Its output cannot exceed the current full-budget electrical envelope or per-level current limit, including after voltage recovery. Pedal stop, invalid voltage, braking, hard LVC, thermal, speed, shift, motor fault and configuration-change shutdown retain priority. Partial protection envelopes disable comfort even when rider demand is already below the protective cap. Throttle remains independent of the PAS-only watt budget and has an explicit GUI disclosure.

## Validation limits

Host tests, configuration/XML and UI binding tests, native SDCC builds and MCU instruction-set simulations cover the implemented invariants. Linker stack availability is not measured peak interrupt-plus-call stack use. Off-target timing is not a hardware deadline guarantee. Actual controller revision checks, current/voltage calibration, stack watermarking with interrupts, transient capture, wheel-clear safety tests and matched road A/B runs remain physical validation work. No percentage range improvement or universal safe hardware power rating is claimed.
