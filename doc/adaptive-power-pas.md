# Adaptive Power PAS variant

## Configuration and behavior

For any Standard or Sport assist level, select **Type: PAS**, then
**Variant: Adaptive Power**. Set **PAS budget (W)** in **10 W steps**, from 10 W up to the smaller of 2550 W
and the configured voltage/current/level ceiling. Selecting this variant starts
with up to 250 W, bounded by that ceiling. This is a
per-level control algorithm, not a global ECO switch or a preset of the existing
cadence mode. No torque or additional speed sensor is required.

The algorithm requests electrical power according to cadence:

| Pedal cadence | Requested fraction of this level's PAS budget |
| --- | --- |
| Initial forward pedaling through 15 rpm | 25% |
| 30 rpm | 50% |
| 45 rpm | 75% |
| 60 rpm and above | 100% |

The launch floor applies only after forward pedaling and the existing start
pulse requirement have been detected. It is not an idle current. Once PAS stop
is detected, this variant requests zero PAS current, including when pretension
is enabled. These reference points are a transparent conservative envelope,
not a measured efficiency optimum for every motor, gear or rider.

Current is calculated from this power request and battery voltage, then bounded
by the level's Max Current (%) and the existing protections. The voltage used
for conversion is the higher of the current measurement and the existing slow
filtered measurement. Consequently a short voltage sag cannot immediately raise
current, and recovery uses a higher instantaneous voltage without waiting for
the filter. A raw reading of zero, above 100 V or at/below configured LVC produces
zero current for **every demand source** through the shared final protection.
Zero current configuration, zero LVC configuration and a
zero encoded power budget also produce zero PAS current.

The budget limits the power implied by the **requested** battery-current
percentage and measured voltage. The motor controller's tracking error,
calibration accuracy and transients mean that this is not a hard instantaneous
limit on measured physical watts. The 1% current-command resolution rounds down;
small budgets or low current ceilings can produce a request of zero. For example,
250 W at 48 V with a 25 A global limit requests 20% current, corresponding to
240 W under this model.

Global Keep Current and voltage-compensation settings retain their existing
meaning for the old PAS variants. Adaptive Power calculates its own
cadence-to-power envelope and voltage-to-current conversion, so those settings
do not add a second scaling/boost. Max Current, Max Cadence and Max Speed remain
independent level ceilings. Throttle remains optional, uses its own existing
current mapping and can override the PAS budget. Walk assist retains its normal
behavior and release interlock.

For Adaptive Power and Adaptive Efficiency, rising targets use a fixed-point
position/velocity tracker that retains velocity across target changes. It advances
at most one bounded 20 ms step per application call; delayed processing cannot
produce a catch-up surge. Smoothness controls acceleration and the final approach.
This removes the reproduced 20% to 100% target plateau at 405 ms. Falling targets
and safety reductions remain immediate by default. The optional System comfort
setting gradually reduces positive pedaling demand, always bounded by the updated
full-budget current ceiling; coasting and all protection envelopes bypass it.
Level/mode changes reset the correct shared ramp state. Other PAS variants retain
the previous segmented ramp byte-for-byte across all 101 smoothness settings.

## Adaptive Efficiency: explicit experimental variant

Select **System → Motor profile: BBS02B, BBSHD or TSDZ2**, then choose
**PAS → Adaptive Efficiency (experimental)** on each desired level. This does not
activate a global ECO mode or alter other levels. New/default and migrated
configurations never enable the variant or comfort automatically. An enabled
profile must match the actual connected controller; BBS02B additionally requires
verified compatible STC/UART hardware, which profile selection cannot detect.

The base watt budget, cadence envelope and all current/speed/protection ceilings
remain. After a three-second launch grace, a small bounded current trim responds
to low cadence and sustained requested demand, with hysteresis. Activation uses
40% of this level's own cadence-shaped watt budget, or 40% of the level current
ceiling when that ceiling is the attainable reference. Release uses 30% on both
references. These are requested-command proxies, not measured motor load. It acts only above
75% of the configured speed ceiling, recovering when that condition ends. Speed
approach uses one eased envelope instead of a separate 97% correction. BBS02B and
TSDZ2 retain at least 90% of the base request; BBSHD retains at least 95%. Coarse
current reductions round down rather than erasing a positive one-percent command.
Very small corrections can round to no current change: BBSHD at 48 V / 30 A,
250 W and 39 rpm arms its 5% trim, but an 11% current command remains 11%; at
500 W it changes 22% to 21%. The profile's trim bound is not rounded upward.
Low-speed hill starts retain the base budget. Missing configured wheel speed
falls back to unshaped Adaptive Power; invalid cadence, voltage or the existing
TSDZ2 torque-sensor fault indication produces zero experimental PAS demand.

A global 100 km/h limit and a 20% level limit give the existing 20 km/h safety
ceiling. Adaptive Efficiency eases PAS current over the existing approach region
(normally the last 3 km/h, at most half the limit), with zero current at/above
the same hard ceiling. As speed falls, assistance returns through the existing
rate/acceleration-limited tracker. The speed envelope has gentle entry/exit
slopes before command quantization; it is not a cruise-speed guarantee or a
measured motor-efficiency optimum. Slope, gearing, human power and motor output torque remain unobserved.
The supplied datasets support test hypotheses, not transferable real-time maps.
See the [engineering decision](decisions/adaptive-efficiency.md) for profile
thresholds, evidence quality, memory reuse and validation limits.

Only Adaptive Efficiency also permits slow soft-LVC recovery after sustained
voltage recovery: eight eligible samples (about one second), a 0.5 V margin and
an upward filter four times slower than the existing downward filter. Transient
rebounds, invalid readings, sampling gaps and throttle/mode changes cancel the
recovery dwell. Hard cutoff and faults remain immediate. Other modes/throttle
keep downward-only filtering, sharing the existing filter history. Small
Efficiency target changes retain the Q7 timer/velocity without delaying stops
or holding output above a reduced request.

No logging, telemetry, diagnostics, measurement buffers, history, runtime
statistics, extra scheduled tasks or EEPROM fields are added. The existing
logging infrastructure remains unchanged. All research analysis and simulation
instrumentation run offline.

## Storage and compatibility

Configuration schema **8** retains the **156-byte** payload, **6-byte** assist
level and **161-byte** maximum request. Native compile-time checks enforce the
configuration, five-byte PAS cache and seven-byte maximum shared ramp state.

- Flag `0x80` keeps its schema-7 Adaptive Power meaning. In schema 8,
  `0x80 | 0x08` selects Adaptive Efficiency; the combination was invalid in v7.
- The existing `torque_amplification_factor_x10` byte still stores Adaptive
  budgets in 10 W steps and retains its original Torque PAS meaning.
- Existing byte 154 packs voltage compensation in bit 0, the three profile
  choices in bits 1–2, and comfort in bit 3. Reserved profile/option bits are
  rejected. No EEPROM layout or frame grows.
- v7 migration preserves genuine Adaptive Power and rejects its previously
  invalid combined variant. v6 clears the retired per-level compensation bit;
  v5 also migrates compensation to the global setting. None activates efficiency
  or comfort. The compiled profile is only a selection default.
- Old XML `VoltageCompensation` names still migrate to the global control. New
  XML retains the explicitly selected variant, profile and comfort option.
- Writes require matching schema 8 and reject mixed adaptive/torque or
  adaptive/cruise flags, zero/excessive budgets and incompatible efficiency
  profiles before changing live RAM. A valid write/reset stops the motor until
  restart. Every controller write still requires the default-No confirmation.

Use matching firmware and tool releases. Back up settings before upgrading.
Downgrading to older firmware does not preserve schema-8 EEPROM settings; restore
a suitable backup rather than reinterpreting the packed options or variant bits.

## Firmware memory and safety verification

See [current/power safeguards and firmware size](firmware-safety-and-memory.md)
for the final three-controller size comparison, configuration-derived watt limits,
thermal/LVC changes, restart interlock, compiler/simulator checks and the limits
of hardware-free validation. Schema sizes remain unchanged.

## Evidence and limits

The design is informed by Arango, Lopez and Ceren,
[Improving the Autonomy of a Mid-Drive Motor Electric Bicycle Based on System
Efficiency Maps and Its Performance](https://www.mdpi.com/2032-6653/12/2/59),
*World Electric Vehicle Journal* 12(2), 59 (2021), DOI
[10.3390/wevj12020059](https://doi.org/10.3390/wevj12020059).

That paper optimizes assist level and chain ratio using **experimentally
identified** efficiency maps and a vehicle/rider model. Those maps, actual gear
selection and reliable human-power measurements are not available on all
supported controllers. Its reported savings cannot be transferred to this
firmware. In particular, Table 5 reports no energy reduction for its combined
motor-and-rider comparison. This implementation is a small cadence-aware power
controller, not a reproduction of the paper's map optimizer, a phase-current
loss estimator, or a verified Wh/km optimizer.

The patch does not alter GPIO, ADC scaling, UART timing/drivers, interrupts,
watchdog configuration, NEC control firmware, STM8 commutation/FOC constants,
or the existing thermal ramp thresholds. Hard stops above maximum temperature
and at raw LVC/invalid voltage remain. Soft-LVC sampling is wrap-safe; only the
Adaptive Efficiency PAS path gains the bounded recovery described above.

Automated tests exercise budget bounds, voltage sag/recovery and invalid readings,
all 20 assist levels, optional throttle, stop/coast, current-curve retargeting,
clock wrap and brake/speed/thermal/LVC/shift/walk interlocks. Existing non-adaptive
ramps are compared with the frozen previous function over changing requests,
all 101 shapes and multiple current limits/rates. Protocol tests reject invalid
or old-version writes; EEPROM and XML/binary migration are tested separately.
Windows UI checks cover the new variant, watt input and preview in both themes.

No physical controller has been flashed or connected for this validation.
Bench-test the supported controller with the driven wheel clear before riding.
Any range claim requires measured battery Wh/km, distance, elapsed time, rider
contribution, gearing and comparable route/weather conditions; a smaller
configured power budget alone does not establish improved system efficiency.
