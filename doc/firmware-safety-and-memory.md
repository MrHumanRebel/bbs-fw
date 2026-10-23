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
PAS, variable PAS, torque PAS where supported, Adaptive Power, Adaptive Efficiency, cruise, physical
or virtual throttle, and walk assist.

- An active thermal, LVC, speed or shift envelope prevents the normal falling-current
  ramp from restoring a previous higher output, including when input is released
  or is already below the cap. The ordinary falling ramp remains active outside
  protection regions. Brake, power-block and virtual-control interlocks remain effective.
  Shift duration also remains correct across millisecond-counter wrap.
- The existing 80–85 °C thermal ramp remains: 100% at 80 °C, down to 20% at 85 °C.
  **Above 85 °C the command becomes zero**, rather than remaining at 20% indefinitely.
  Limiting uses the full signed temperature reading; cached display temperatures
  saturate instead of wrapping above 127 °C. Protection requires an available,
  enabled, working sensor; some supported motors/controllers lack these sensors.
- A raw voltage at/below configured LVC, zero, or above the protocol's valid
  100 V range immediately commands zero for every demand source. The existing
  downward filtered LVC ramp remains algebraically equivalent. Adaptive Efficiency
  alone permits slow soft recovery after sustained voltage margin and a dwell;
  independent throttle and other modes do not drive that recovery. The filter
  remains shared across mode changes. Sampling survives millisecond-counter wrap.
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
save persists those corrected values. Schema-5/6/7 migration remains supported; none activates experimental efficiency or comfort.

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
| BBS02 | Flash 25,805 / 25,772 bytes | XRAM 904 / 861 bytes | 150 / 156 bytes available |
| BBSHD | Flash 27,361 / 27,328 bytes | XRAM 918 / 875 bytes | 138 / 144 bytes available |
| TSDZ2 | Image 16,396 / 15,464; CODE 15,655 / 14,733 bytes | DATA 587 / 581; INITIALIZED 61 / 61 bytes | SSEG reservation 1 / 1 byte |

Measurements use clean SDCC **4.5.0** builds, comparing the preceding Adaptive
Power implementation (`f116eab`) with this hardening/optimization patch. The table
reports firmware flash/CODE, not Intel HEX text-file size. Linker stack capacity
and STM8 SSEG reservation are not measurements of peak runtime stack use.

`ci/check_firmware_size.py` now enforces the pre-efficiency static-RAM and 8051
stack-capacity baseline exactly, with a separately reviewed allowance for the
new control code in flash. The STM8 check includes
the complete HEX address span so vectors, startup code and constant tables cannot
escape a check of the CODE section alone. These regression budgets
are deliberately distinct from MCU ratings. PR CI builds all three controllers,
runs host regressions, and executes the extracted shipping conversion using
SDCC/uCsim; release builds also enforce the size budgets. No global compiler
optimization or unsafe-memory-read flags are enabled.

## Adaptive Efficiency: zero additional static RAM

The comparison below records pre-feature baseline `4a50c09` against `31f73d5`,
using clean SDCC 4.5.0 builds and the same target settings. The coordinated
speed/sag refinement is measured separately below. The research datasets and all
measurement probes stay offline. The existing five-byte PAS cache and seven-byte
ramp state are reused as unions; compile-time size checks and CI memory budgets
prevent static-RAM growth.

| Build target / selectable profile | Flash before / after / delta | Static RAM before / after | Linker stack before / after |
| --- | --- | --- | --- |
| BBS02 / BBS02B | 25,772 / 27,914 / +2,142 bytes | XRAM 861 / 861 bytes | 156 / 156 bytes available |
| BBSHD / BBSHD | 27,328 / 29,486 / +2,158 bytes | XRAM 875 / 875 bytes | 144 / 144 bytes available |
| TSDZ2 / TSDZ2 | Full image 15,464 / 16,673 / +1,209 bytes; CODE 14,733 / 15,942 | DATA 581 / 581; INITIALIZED 61 / 61 bytes | SSEG reservation 1 / 1 byte |

Flash increases to implement the new control policy, fixed-point target tracker,
comfort ceiling and schema validation/migration. Configuration remains 156 bytes;
no EEPROM field, request byte, telemetry field, event, buffer, runtime statistic
or background task is added. The Adaptive tracker uses only bounded 16-bit
arithmetic and runs at most one tracking step per 20 ms from the existing
application call. Profile decisions change at most once per 100 ms there.

`tests/run_target_control.py` compiles the production application with mock I/O
and checks every smoothness setting, changing targets, clock wrap, profile
shaping and immediate safety stops on each MCU instruction set. Its optional
`--baseline-ref 4a50c09` repeats the same probe against the old implementation.
Stack watermarks and simulator-cycle counts describe that finite probe only,
with no interrupts or real peripheral I/O. They do not certify the hardware's
peak stack requirement or the 5 ms application deadline. STM8 simulation sets
SP to the S105's actual 0x07ff RAM top and keeps overflow checking enabled with
a conservative 0x03ff lower boundary.

### Low-budget activation correction relative to `3a164c3`

The audit regression reproduces the old inactive gate at 250 W on all three
profiles. It now compares the known fraction of the configured PAS watt request,
with a current-ceiling alternative when that ceiling clips the attainable budget.
No division, new state or protocol/EEPROM change is needed. The existing flash
budgets remain unchanged and all native builds satisfy them.

| Target | Flash at audit / corrected / delta | Static RAM delta | Linker stack-capacity delta |
| --- | --- | --- | --- |
| BBS02 | 27,913 / 27,914 / +1 byte | 0 | 0 |
| BBSHD | 29,485 / 29,486 / +1 byte | 0 | 0 |
| TSDZ2 | Image 16,669 / 16,673 / +4 bytes; CODE 15,938 / 15,942 | 0 | 0 |

The updated full 101-shape MCU probe uses 250 W and 500 W PAS budgets and
platform-valid 25 A / 30 A / 20 A current settings for its app-level cases.
The same source harness was run against `3a164c3` and the corrected application;
the baseline skips only the assertions for the newly corrected activation.
The main/fixture frame and interrupt stack are outside these stack-extension
measurements. Linker capacity is unchanged, and no physical peak-stack claim is
made.

| Target | Audit probe stack / corrected probe stack / delta | Audit / corrected simulator clocks |
| --- | --- | --- |
| BBS02 | 33 / 31 / −2 bytes | 2,041,489,896 / 2,042,271,852 |
| BBSHD | 33 / 31 / −2 bytes | 2,042,622,636 / 2,043,313,008 |
| TSDZ2 | 74 / 74 / 0 bytes | 31,104,896 / 31,102,814 |

Clock counts compare finite software workloads, not physical motor efficiency or
real controller deadline margins. Host regressions additionally cover 150, 250,
500 and 750 W on every profile, exact 24 rpm / 18 rpm hysteresis boundaries,
current-clipped budgets, launch grace and low-speed recovery. In particular,
BBSHD's 5% soft trim can arm at 250 W without changing an 11% command, because
that correction is smaller than the existing one-percent command resolution.
At 500 W the same profile demonstrably changes 22% to 21%. These are requested
commands in simulation, not measured battery or shaft power.

### Coordinated speed, recoverable soft LVC and target jitter versus `31f73d5`

This follow-up removes the independent 97% trim, adds a single eased PAS speed
envelope, permits slow soft-LVC recovery only for Adaptive Efficiency without a
winning throttle, and retains Q7 timing/velocity through small Efficiency target
changes. The three motor profiles and configuration/wire layouts do not change.
No telemetry, event, log, buffer, runtime statistic or scheduled task is added.

Clean SDCC 4.5.0 builds, same flags and controller selection:

| Target | Flash before / after / delta | Static RAM before / after | Linker stack before / after |
| --- | --- | --- | --- |
| BBS02 / BBS02B | 27,914 / 28,227 / +313 B | XRAM 861 / 860 B (−1) | 156 / 164 B available (+8) |
| BBSHD | 29,486 / 29,799 / +313 B | XRAM 875 / 874 B (−1) | 144 / 152 B available (+8) |
| TSDZ2 | Image 16,673 / 16,780 / +107 B; CODE 15,942 / 16,047 (+105) | DATA 581 / 581 B; INITIALIZED 61 / 61 B | SSEG reservation 1 / 1 B |

All other 8051 static/bit areas remain unchanged. The four-byte filter allocation
is reused for its voltage and recovery dwell, with a native compile-time size
assertion. LVC arithmetic now uses bounded unsigned 16-bit differences instead
of signed 32-bit division. Reentrant LVC removes static compiler spills; its
elapsed-time sample is reused rather than reading the clock repeatedly. No
additional static RAM is required on any target.

The old flash regression allowances covered the preceding feature set. They are
explicitly updated for this measured code cost: BBS02 28,256 B, BBSHD 29,824 B;
TSDZ2 CODE 16,064 B and complete image 16,800 B. These leave only 29/25/17/20 B
above the new builds, respectively, and are not physical MCU capacities. XRAM
ceilings tighten to 860/874 B and 8051 stack-availability floors to 164/152 B.
STM8 DATA/INITIALIZED ceilings remain 581/61 B. Memory-regression tests check
rejection above the revised limits and below the stack floors.

The identical updated MCU harness was run against `31f73d5` and this follow-up:

| Target | Baseline / updated probe stack extension | Baseline / updated simulator clocks |
| --- | --- | --- |
| BBS02 | 31 / 31 B (0) | 2,066,656,152 / 2,046,407,628 |
| BBSHD | 31 / 31 B (0) | 2,067,592,272 / 2,047,342,404 |
| TSDZ2 | 74 / 74 B (0) | 31,643,634 / 31,271,394 |

The probe now covers all 101 ramp shapes, small-target timing, the eased PAS
speed envelope, unchanged throttle speed shaping and immediate full-throttle
arbitration, LVC dwell/recovery and hard stops on each MCU ISA. The baseline
skips new-policy assertions and retains its original nonrecovering filter; paths
therefore differ deliberately despite identical input sequences. Clock counts
are finite-workload simulator observations, not a physical CPU deadline or
motor-efficiency measurement. Stack extensions exclude the fixture/main frame
and interrupts; unchanged probe peaks do not certify real peak stack use.

The host runner passes 544 groups, including exhaustive monotone speed-envelope
sweeps for every 1–100% input command, 20 km/h and low-limit endpoints, 100%
throttle, slow recovery, interrupted recovery dwell, mode/throttle scope, frozen
legacy LVC comparisons across wrap, all 101 jitter shapes and existing safety/
protocol/migration regressions. Representative curve, LVC-recovery and timing
assertions fail against `31f73d5` on every target and pass after the change.

An explicitly synthetic three-phase road-response test (load increase/removal)
settles within a 0.3 km/h peak-to-peak band in its last ten seconds per phase,
without self-generated crossing of the 20 km/h hard ceiling. It uses 500 W and
25/30/20 A for BBS02B/BBSHD/TSDZ2. Its simplified drag/inertia response is not a
measured motor, rider or road model. It cannot establish physical efficiency,
range gains or universal absence of hunting on real bikes.

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
