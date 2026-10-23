# CODEX TASK — Adaptive Efficiency for BBS02B + BBSHD + TSDZ2 ONLY

Role: embedded motor-controls, firmware performance, data analysis, WPF tooling, test engineer for `MrHumanRebel/bbs-fw`.

**Exactly three motor optimization profiles must ship: `BBS02B`, `BBSHD`, `TSDZ2`.** Do not introduce a fourth BBS02 or TSDZ2B profile. The repository may still compile BBS02 because the user-selected BBS02B hardware, if positively verified compatible, maps to that firmware build target. Treat a 2019 Grin BBS02 tested sample as **related-family context only**; do not mislabel its Kv/R as directly measured on BBS02B. TSDZ2B test points are likewise external reference observations for TSDZ2, NOT proof of interchangeable motors.

## Goal
Implement practical improvements for battery **Wh/km** under equivalent speed, route and rider contribution, while preserving existing safety behavior and RAM/flash constraints. Do not promise percentage range improvement without replicated A/B riding tests. Build on PR #9 Adaptive Power PAS if present; DO NOT overwrite newer work.

## Required first actions
1. Read `research/efficiency/README.md`, `data/DATA_DICTIONARY.md`, `data/dataset_manifest.json`, all three `docs/PROFILE_*.md`, `docs/RESEARCH_REPORT_HU.md`, `docs/IMPLEMENTATION_SPEC.md`, `docs/TEST_MATRIX.md` and `data/sources.csv`.
2. Run `python research/efficiency/tools/validate_dataset.py`. Examine `data/research_dataset.json` and **all three** profile CSVs plus `motor_parameters_3_profiles.csv`.
3. Inspect `git status`, checked-out branch, PR #9 and current firmware/tool implementation. Establish baseline automated tests, flash, XRAM/DATA and runtime stack margin for actual three **build targets BBS02, BBSHD, TSDZ2**.
4. Create short engineering ADR distinguishing measurements vs proxy/references vs missing/unidentifiable quantities.

## Mandatory use of research
- **BBS02B:** Use `data/bbs02b_factory_reference_93points.csv` (93 rows) for source-tagged **offline** load-sweep trends, smoke tests and opt-in experiment proposals. User specifically approves this dataset as a BBS02B reference. DO NOT fabricate 2D `eta(rpm,torque)`, infer instantaneous torque absent a torque sensor, or automatically deploy map-derived current cutoffs.
- **BBSHD:** Use `data/bbshd_factory_4_landmarks.csv` plus `motor_parameters_3_profiles.csv` (direct BBSHD Grin data). Four points are insufficient for a comprehensive efficiency map. BBSHD must have its own, opt-in behavior and missing-calibration fallback.
- **TSDZ2:** Use 36/48 V R/L, pole pairs and output torque sensor guidance from `motor_parameters_3_profiles.csv` *only where conventions/calibration permit*; use `data/tsdz2_external_bench_9points.csv` for test design and sensitivity analysis. It is a **TSDZ2B** physical comparison, not a TSDZ2 board calibration or the repo's firmware efficiency result.

## Deliver working changes in this order
1. **Measurement layer and trip logger:** use measured Vdc and Idc and wheel revolutions/accurate circumference to integrate `Wh` and `Wh/km`, robust against missing data, 0 distance, invalid intervals, clock wrap and calibration. Use Windows side when feasible to protect scarce 8051 MCU RAM; export CSV with hardware/software metadata and baseline vs test runs. Missing measured Idc => Wh unavailable, never invented.
2. **PR9 safety/control audit:** per-level 10–2550 W PAS budget, current percentage conversion, user-facing throttle-bypass disclosure, invalid voltage/LVC behavior, hardware protections and v5/6/7 migration. Retain default behavior for old PAS variants. Repair moving-target S-curve plateau: at 25 A, 10 A/s, S100, 20% target until 400 ms then 100% from 405 ms, a previously observed ~250 ms output freeze must be eliminated without upward spikes or broken hard shutdown.
3. **Three opt-in experimental profiles:** create platform-specific candidate current shaping **based on valid sensors only**. Begin with limited low-cadence/high-request behavior and hysteresis; do not globally reduce hill-start safety/torque without testing. For BBS02B/BBSHD, battery command and sensor signals are available, motor phase current/torque/FOC not directly observed by this application. For TSDZ2, calibrate human torque and compute human input power independently of motor output power. All inferred quantities labeled with uncertainty.
4. **Thermal/efficiency diagnostics:** show source-qualified offline plots, ranges and confidence. Add a monitor-only temperature predictor if enough calibrated thermal parameters available. Keep original LVC, temp, current, braking, shift, speed and watchdog protections higher priority; no dangerous defaults, no NEC modifications, no BBSHD/BBS02B FOC edits.
5. **Validation & delivery:** implement firmware host tests, C# serializer/UI tests, memory and timing checks, hardware bench plan and paired road A/B protocol as `docs/TEST_MATRIX.md`. Report exact changed files, passed/failed tests, unknowns and genuine unvalidated hardware areas. All three target profile settings must serialize without unsupported protocol changes; 156-byte config payload/161-byte request stays unless a reviewed migration is explicitly needed.

## Constraints
- Production-ready, fail-safe on sensor loss; feature flag defaults OFF; legacy behavior unchanged when OFF.
- Integer/fixed-point MCU arithmetic with audited widths; avoid 32-bit division per 5 ms cycle where possible, extra heap allocations, RAM pressure, and invented sensor data.
- No estimated or published factory η passed off as *real-time physical measured efficiency*. Only `Wh/km` from observed DC energy and distance may be called a real vehicle consumption metric.
- **Only profile names in final new feature/UI/data schema:** `BBS02B`, `BBSHD`, `TSDZ2`. Compile-target name `BBS02` may exist in existing build scripts, but must NOT be surfaced as a fourth efficiency calibration profile.
- A/B optimization must preserve comparable vehicle performance and human pedal contribution. Raise no speed/current/phase/temperature safety ceilings.

## Deliverable checklist
- [ ] Dataset validator passes
- [ ] Current PR9 implementation audited, moving-target ramp fixed/tested if applicable
- [ ] Wh, Wh/km CSV logging with source-quality flags and robust unit tests
- [ ] Exactly 3 opt-in efficiency profile identifiers (BBS02B, BBSHD, TSDZ2)
- [ ] All existing firmware build targets needed to run these 3 profiles pass and memory/stack/timing reported
- [ ] Schema migrations and WPF controls tested; no unsafe silent activation
- [ ] Safety invariants tested and documented; physical test needs clearly listed
- [ ] No claimed percentage range increase without paired real rides
