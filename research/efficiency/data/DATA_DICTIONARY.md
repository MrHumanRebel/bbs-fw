# Data dictionary and provenance rules — FINAL 3 profiles

All CSV are UTF-8 with header; empty is *not reported*, never zero. `source_id` references `sources.csv`, URLs are external and do not imply a local original PDF. Exactly 3 top-level profiles exist: BBS02B, BBSHD, TSDZ2.

## BBS02B (93 rows): `bbs02b_factory_reference_93points.csv`
- `row`, `source_row`: 1..93 manufacturer printed sequence.
- `motor_family=BBS02B`: **application target**; original unversioned BBS 48 V/750 W factory test is source evidence. `calibration_status=accepted_BBS02B_reference_not_direct_revision_measurement`. `origin_csv` points to external published source table (raw original not duplicated inside package), `original_source_motor_family=BBS_48V_750W_unversioned`.
- `torque_Nm`, `crank_output_rpm`, `mechanical_output_W`: factory mechanical output side; motor output rpm not necessarily rider PAS cadence.
- `battery_voltage_V`, `battery_current_A`, `battery_input_W`: published DC measurements.
- `efficiency_pct_reported`: manufacturer η in percentage; `efficiency_pct_recomputed=100*Pout/Pin`; `loss_total_W_derived=Pin-Pout` total drive losses.
- This 93-row **correlated torque-speed load sweep** is NOT a measured 2D RPM×torque map. Do not extrapolate arbitrary operating conditions or phase winding temperature/current.

## BBSHD (4 rows): `bbshd_factory_4_landmarks.csv`
- `landmark`: four manually labeled factory curve observations, NOT a full set of measured combinations. No synthetic interpolation between labeled points as verified measurements.
- `mechanical_output_W`, `efficiency_pct_reported`, `battery_input_W_derived`, `efficiency_pct_derived`: derived fields only where enough values. Empty η remains unknown.

## TSDZ2 (9 external reference rows): `tsdz2_external_bench_9points.csv`
- `motor_family=TSDZ2B`: original shop's tested motor. `target_profile=TSDZ2` is the destination research profile, NOT a claim of hardware equivalence.
- `nominal_voltage_V`, `battery_current_setpoint_A`: nominal settings; not synchronized, precision pack V/A time series.
- `crank_rpm`: shop-selected input cadence at 60/90 rpm; `firmware`, `cooling`: different experimental settings.
- `trainer_output_W_approx`, `efficiency_pct_reported_approx`: shop-estimated mechanical trainer and efficiency, not universal laboratory 2D η.
- `time_to_70C_seconds`, `thermal_peak_time_seconds`, `thermal_peak_C`, `cover_temp_C`, `measured_start_C`: source's specific observed temperature points. A blank time-to-70 means **not reached**, never zero. 70 °C is NOT a preset thermal safety cutoff.

## `motor_parameters_3_profiles.csv`
- `motor_family`: **one of the three target profile identifiers**, not always original tested machine.
- `variant` and `evidence_type` preserve independent target vs family-proxy distinction. In BBS02B rows with `related_family_reference_only`, Grin actually tested older BBS02 samples, NOT a verified BBS02B revision.
- Grin equivalent resistance is a black-box lumped value, NOT Rphase, and no-load Kv is output-side crank rpm/V, NOT rotor rpm/V.
- Tongsheng R/L and no-load shaft speed from another controller documentation have an unknown phase/line convention; measure before using in safety-critical FOC.

## Physical equations and units
```
Pin_W = measured_Vdc * measured_Idc
Pshaft_W = crank_output_Nm * crank_output_rpm * (2*pi/60)
eta_pct = 100 * Pshaft_W / Pin_W        # only if actual Pshaft measured
loss_total_W = Pin_W - Pshaft_W
trip_Wh = sum(measured_Vdc * measured_Idc * delta_ms / 3600000)
Wh_per_km = trip_Wh / measured_trip_km  # nonzero, valid, calibrated distance only
```
Do not substitute requested PAS-current % for actual DC pack I, do not call shaft power rider power, do not infer measured η from factory references and never report a percentage Wh/km improvement without paired physical test evidence.
