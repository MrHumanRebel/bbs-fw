# Profile 3/3 — Tongsheng TSDZ2 (STM8 + rider torque sensor)

## Evidence levels
- Direct family reference: `motor_parameters_3_profiles.csv` cites `TSDZ2_V2_MOTOR` (a *separate* open-source controller project's 36 V and 48 V characterization): reported winding R 0.094 Ω / 0.125 Ω and inductance 76 / 135 µH respectively, 8 pole pairs, rotor no-load ≈ 4000 rpm. The phase/line convention and temperature calibration are **not defined** for this repo's exact hardware. Rotor rpm != pedal cadence. Also cite `TSDZ2_MANUAL` for nominal mass (~3.3 kg) and nominal maximum quoted torque (up to 80 Nm).
- **External variant only:** `data/tsdz2_external_bench_9points.csv` has nine **TSDZ2B** shop trainer tests (`TSDZ2B_BENCH`), used to form *comparative test hypotheses*, NOT to calibrate the older `TSDZ2` firmware automatically. This file's `motor_family=TSDZ2B` denotes **tested source**, while `target_profile=TSDZ2` denotes **intended research target**.

Approximate no-cooling, 48 V / 10 A nominal comparison, same workshop setup:

| Cadence | Stock trainer W | OSF trainer W | Approx. stock η | Approx. OSF η | Temperature test endpoint |
|---|---:|---:|---:|---:|---|
| 60 rpm | 331 | 353 | 65% | 70% | 70 °C reached after 5:40 vs 7:45 |
| 90 rpm | 328 | 370 | 64% | 73% | 70 °C reached after 4:58 vs 11:07 |

The battery voltage and mechanical/drivetrain conditions were not exactly normalized across these measurements. Cooling configurations in the other rows are separate interventions. These are **source-specific approximations**, NOT verified TSDZ2 firmware efficiencies, percentage range improvements or safe temperature setpoints.

## Firmware candidates
- Calibrate actual rider torque zero, scale, nonlinear response and cadence before any rider-W computation. `P_rider = torque_pedal × cadence × (2π/60)` estimates **human** mechanical input, not motor output efficiency.
- Monitor rider-relative assistance, sustained load and measured DC Wh/km; keep *opt-in* conservative shaping and all emergency protection priority unchanged.
- Unlike STC/NEC Bafang controllers, STM8 TSDZ2 may own current control, commutation and FOC code. Do **NOT** blindly paste 36/48 V R/L, estimated offset or any TSDZ2B results into existing FOC. Low-level control tuning is a separately gated, instrumented engineering experiment.
- Bench-test 60 and 90 rpm as controlled comparison points; vary motor and rider load, and log true pack V/A, actual torque calibrations and thermal history.

## Missing data
Direct tested TSDZ2 original 2D efficiency matrix, phase-resistance convention, winding thermal sensor/RC characterization and comparable-route Wh/km gain.
