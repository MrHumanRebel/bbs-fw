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

For Adaptive Power, moving targets preserve the rising curve's time origin
so repeated cadence samples cannot continuously restart its flat beginning.
Falling targets and safety reductions are bounded immediately: the ordinary
ramp-down cannot raise the new lower PAS request. Level/mode reloads involving Adaptive Power reset any hidden envelope to the
last commanded current. Other variants retain the old ramp behavior. This is not a predictive or fully jerk-limited trajectory planner.

## Storage and compatibility

Configuration schema **7** retains the **156-byte** payload, **6-byte** assist
level and **161-byte** maximum protocol frame. Compile-time checks reject
accidental structure growth.

- Flag `0x80`, retired from per-level voltage compensation in schema 6, identifies
  Adaptive Power in schema 7.
- The existing `torque_amplification_factor_x10` byte stores the Adaptive Power
  budget in 10 W steps. For Torque PAS it keeps its original meaning.
- No per-level arrays, lookup table, heap allocation or new periodic task are
  added. Arithmetic uses bounded integers. A restart stop latch protects
  configuration changes.
- EEPROM schema 6 migrates to 7 without changing existing global controls or
  selecting Adaptive Power. The old retired bit is cleared.
- Schema 5 migration still converts per-level voltage compensation to the global
  toggle. It cannot select Adaptive Power.
- Old XML `VoltageCompensation` flag names migrate to the global toggle. New XML
  uses the distinct `PasAdaptive` name. Invalid adaptive combinations and budgets
  outside the encoded 10 W steps are rejected.
- Configuration writes require schema 7 and reject mixed adaptive/torque,
  adaptive/variable or adaptive/cruise flags and zero budgets before changing
  configuration RAM. They also validate the controller-specific current ceiling
  and power envelope. A valid write/reset stops the motor until restart. The
  Windows tool still reads older supported schemas, but
  writes only to matching firmware. Existing write confirmations remain mandatory.

Use matching firmware and tool releases. Back up settings before upgrading.
Downgrading to schema-6 firmware does not preserve schema-7 EEPROM settings;
restore a suitable backup rather than treating the bit as an old option.

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
the existing thermal ramp thresholds or the minimum-following LVC filter. It adds
a stop above maximum temperature and at raw LVC/invalid voltage, and makes LVC
sampling wrap-safe. The filter is **not** changed into a voltage-recovery mechanism.

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
