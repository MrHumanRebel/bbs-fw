# Profile 2/3 — Bafang BBSHD (48 V / 1000 W)

## Evidence
`data/bbshd_factory_4_landmarks.csv`: Bafang 2016 manufacturer performance curve, **four** printed labeled reference points only (`BBSHD_FACTORY`); not a continuous numeric 2D map.

| Factory point | Torque Nm | Output rpm | Battery A | Mechanical output W | Published η |
|---|---:|---:|---:|---:|---:|
| Initial load | 0.7 | 159.3 | 2.17 | unknown | unknown |
| Max η | 29.4 | 156.4 | 11.75 | 481.53 | **85.6%** |
| Max measured torque | 114.5 | 76.1 | 28.03 | 912.50 | not labeled |
| Max mechanical output | 87.0 | 118.3 | 29.85 | 1077.82 | not labeled |

`motor_parameters_3_profiles.csv`: independent Grin 2019 BBSHD sample no-load output Kv ≈ 3.27 crank rpm/V and black-box equivalent resistance ≈ 0.086 Ω. Stator length 48 mm, shell+stator mass ~1.80 kg, rotor magnets 8; relative thermal-dissipation reference ~1.15 vs earlier family comparator. **Never** treat black-box R as physical stator phase R or claim universal thermodynamics from a single specimen. See `GRIN_2019` in sources.

## Firmware candidates
- Independent BBSHD watt-limit/pedaling/load-responsive PAS candidate under verified sensor availability; do not copy BBS02B limits or apply a 117 rpm target.
- Monitor reference operating envelope; 156.4 rpm is single **point** of maximum published η in this particular factory test, not a global optimum.
- For thermal foresight, first measure motor casing/sensor placement and effective thermal RC under repeatable controlled input. Do not replace existing thermal protection or force cooling thresholds from external samples.
- Actual command %, pack V, pack A, cadence and wheel speed only: no direct output torque/phase-current access on STC+separate NEC architecture.

## Missing data
Full 2D RPM × torque η surface; per-unit thermal RC/winding probe; independent drivetrain/rider power measurement; measured Wh/km improvement.
