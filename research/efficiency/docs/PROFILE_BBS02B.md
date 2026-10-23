# Profile 1/3 — Bafang BBS02B (48 V / 750 W)

## Evidence
**Dataset:** `data/bbs02b_factory_reference_93points.csv` — all **93 actual published manufacturer 48 V/750 W BBS load-sweep rows** applied as a user-approved BBS02B engineering reference. `BBS750_FACTORY` is the primary source ID and the external PDF URL is in `sources.csv`. The tested manufacturer's exact revision is not disclosed: user acceptance establishes suitability as a **provisional reference**, not a physical 750W BBS02B proof. One scan/protocol test must positively establish this particular device uses supported STC/UART hardware before flashing.

Key original reference observations:

| Factory condition | Output shaft torque | Output rpm | Pin | Pout | Published eta |
|---|---:|---:|---:|---:|---:|
| Highest reported η, row 50 | 35.1 Nm | 117.0 rpm | 501.29 W | 430.07 W | **85.8%** |
| High-load endpoint, row 93 | 90.4 Nm | 92.5 rpm | 1157.63 W | 875.69 W | **75.6%** |

The motor output rpm is *not automatically PAS cadence*. Torque and speed covary across the rows: only points along **one measured coupled load curve** are known. Interpolate gently only along published sweep, not as a free-running 2D efficiency map. `P_loss = Pin − Pout` reflects the complete drive's losses (motor, electronics, transmission).

Further available but weaker evidence: `motor_parameters_3_profiles.csv` contains Grin 2019 measurements on an older **BBS02 sample**, reclassified explicitly as **related-family evidence for BBS02B**, NOT native BBS02B measurement: no-load output Kv 2.71 crank rpm/V, black-box equivalent resistance 0.131 Ω, 29 mm stator, 1.13 kg stator-plus-shell, 16 magnets. These values must not be used as phase resistance or hard-wired safety thresholds.

## Firmware feature candidate
- Retain PR9 `Adaptive Power` PAS with user-set watt cap, existing current, speed, cadence and safety caps.
- Optionally show source-derived 1D factory trend chart at configuration time: `load-index → η, Pin, Pout, P_loss`, properly labeled **factory reference, revision unverified**.
- Investigate low-cadence/high-current *soft derating* behind explicit opt-in, evidence-conditioned and bench-tuned, with short hill-start exception, hysteresis and stable S-curve tracking. Do not derive fixed 117 rpm target from the single maximum-η factory point.
- Do not calculate motor torque from PAS sensor or assume NEC PWM/FOC motor control is exposed on STC firmware.
- Actual optimization claim requires Wh/km A/B with rider W controlled or at least bounded, and measured DC current/voltage externally checked.

## What is not established
No direct revision-verified BBS02B 2D η map, winding resistance definition, motor winding temperature, phase current, motor torque or proven range improvement.
