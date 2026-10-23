# Validation matrix — 3 profiles BBS02B / BBSHD / TSDZ2

## Datasets (run `tools/validate_dataset.py`)
| ID | Assertion |
|---|---|
| D01 | Exactly 3 target profiles (BBS02B 93 manufacturer-proxy points; BBSHD 4 manufacturer anchors; TSDZ2 9 **TSDZ2B-origin** shop references) |
| D02 | All CSV source IDs valid; no fourth motor target profile and no separate BBS02 load-sweep or Grin simulation table |
| D03 | For all 93 factory proxy rows, positive Pin/Pout, torque×rpm mechanical consistency, reported η and Pin≈V×A within source rounding |
| D04 | BBS02B 93 rows have `calibration_status` and revision-unverified provenance; 2D-η-map availability false |
| D05 | BBSHD four anchors have correct motor family and missing η/Pout remain empty, not zero |
| D06 | TSDZ2 benchmark observations retain `motor_family=TSDZ2B`, `target_profile=TSDZ2`; shop estimated η not hardware control parameter |
| D07 | Selected Grin BBS02 measured sample parameter rows labeled `related_family_reference_only` and attached to BBS02B target only as context |
| D08 | `research_dataset.json` agrees exactly with all CSV content and manifest counts |

## Firmware, UI and configuration regression
| ID | Scenario | Required outcome |
|---|---|---|
| F01 | Build for BBS02 target (only verified BBS02B compatible), BBSHD, TSDZ2 | successful, memory and real stack/headroom reported |
| F02 | PR9 250W, 48V,25A,60rpm | 20% request; coarse 1% current step implies ~240 W requested |
| F03 | 250W 30 rpm | 10% request, without double voltage compensation |
| F04 | Pedal stops, pretension on | PAS current zero; independent throttle behavior documented |
| F05 | voltage 0/>100V/<=LVC, current max 0, LVC config 0, budget 0 | zero PAS request, no divide by zero |
| F06 | Sag and slow filtered recovery | safe behavior, no upward surge from sag, proper recovery |
| F07 | 2550 W or max I percent | finite fixed-point arithmetic, per-level ceiling |
| F08 | Cadence 0, 15, 30, 45, 60, >60 rpm | published PR9 envelope, forward pulse gating |
| F09 | Braking while PAS/throttle | zero motor demand on next app processing cycle |
| F10 | Hard undervoltage, thermal, speed or shift clamp | no later comfort ramp may re-raise current |
| F11 | Ramp waveform 20% until 400ms then 100% at 405ms | no accidental ~250ms plateau or surge; bounded slew |
| F12 | Alternating 20/21%, 80→30→80%, mode change, clock wrap | bounded targets, no stale curve envelope |
| F13 | Throttle override/release | no hidden current jump, independent throttle cap obeyed |
| F14 | Existing cadence PAS through S0..S100 and rates | unchanged vs frozen old behavior |
| F15 | EEPROM v5,6,7 migrations; invalid CRC, flags, zero budget | no unintended Adaptive activation or malformed state |
| F16 | Max UART config frame 161 bytes, fragmented packets | parser robust, write confirmation intact |
| F17 | Walk/cruise interlock, pedaling backwards, sensor fault | no unintended motor restart |
| F18 | TSDZ2 rider torque sensor missing/drift/negative/disconnected | reject invalid rider W, fail safe |
| F19 | Wh/km telemetry missing current, zero distance, bad time dt, dropout | quality marked invalid, no fabricated efficiency |
| F20 | Selecting profile BBS02B/BBSHD/TSDZ2 | exactly three UI profile choices; incompatible hardware safely disabled |

## Hardware A/B
Bench with driven wheel clear, isolated safe test conditions, external DC shunt/current meter and safe controller setup. Capture actual supply V/A, PWM/performance where available, brake/thermal/low-voltage safeguards, current transients at ≥10 Hz and real stack-watermark. Verify **BBS02B PCB/MCU and UART compatibility** explicitly before flashing. Log BBSHD separately. Tongsheng TSDZ2 tests require its own board/revision and calibrated torque sensor; TSDZ2B bench data not enough to justify flashing a TSDZ2B.

Road A/B: paired randomized ABBA if possible, at least 3 valid pairs, preferably >5, same route/gearing/tire/pressure/rider power where available, bike mass, wind/elevation, battery SOC/temperature and speed. Record total observed DC Wh, valid km, Wh/km, moving time, temp/rider torque and uncertainties. Claim improvement only if matched conditions and uncertainty support it; lower speed alone is not valid evidence.
