# Firmware + Windows implementation specification (exactly three profiles)

Targets: `BBS02B`, `BBSHD`, `TSDZ2`. Existing firmware build names for these targets: verified-compatible BBS02B uses `BBS02`, native BBSHD uses `BBSHD`, native Tongsheng uses `TSDZ2`. **These are compile targets, not evidence that any particular controller revision is flash-safe.**

## Sensing and actuation
| Capability | BBS02B | BBSHD | TSDZ2 |
|---|---|---|---|
| Battery V and I telemetry | if validated on target revision | supported interface subject to calibration | supported interface subject to calibration |
| Pedal cadence/PAS | yes | yes | yes |
| Rider torque | no | no | yes, calibration mandatory |
| Directly tune motor PWM/FOC from application | no, separate NEC driver | no, separate NEC driver | yes, in STM8 motor code; **separate experiment** |
| Full verified η(rpm,T) 2D map | none | none | none |

## Architecture and sequencing
1. **Observability first**: log measured pack V, pack A, calibrated wheel distance, duration and events. Prefer WPF/Windows user tool, which avoids allocating more scarce 8051 MCU static RAM; if motor telemetry missing, show Wh/km **unavailable**. Export per-trip CSV with raw+derived values and device configuration/quality flags. `energy_Wh += V*I*dt_ms/3600000`, and `Wh/km = energy_Wh/distance_km` only when distance > 0 and data valid. Track missing packets; never mistake requested motor current percent for actual pack current.
2. **Adaptive Power PAS:** leverage existing PR #9 10–2550 W per-level budget; current limit, speed, cadence, throttle configuration and braking, LVC, thermal, shift protection stay intact. Do not apply global voltage compensation twice. Reject raw ADC/battery voltage at or below configured LVC; long-filter sag must not create upward current spikes. Explicit tool warning: independent throttle can exceed PAS-only watt budget.
3. **Ramp stability:** fixed-point positive slew and optionally jerk-bounded target tracking. In PR9, preserving the old S-curve origin when requesting 20% then 100% can cause ~250 ms plateau in one tested example (25 A / 10 A/s / smoothness100). Ensure no target freeze, upward overshoot, stale hidden target after mode reload, sudden comeback after throttle release or clock-wrap. Compare legacy ramp byte-for-byte with frozen baseline, include all smoothness 0..100.
4. **Experimental per-profile shaping** (opt-in, OFF by default): at high sustained *requested* electrical load and low cadence, consider conservative reducing the PAS current cap with hysteresis while preserving immediate safe hill starts and thermal/LVC margins. The 93-point BBS02B proxy is suitable for **offline** reference/sanity, not as a live 2D torque×rpm lookup. BBSHD 4 anchors are not enough for production calibrated optimum. Tongsheng rider `torque × cadence` estimates *human* input power only, not motor output. All controls fall back safely for absent or invalid sensors.
5. **Thermal advisory only until calibrated**: measured drive loss requires measured actual output torque and RPM, not just pack power. First-order thermal prediction requires measured Rth/Cth and specific sensor location. Never replace original overtemperature, hard LVC, brake, speed, shift, current or watchdog protections.

## Source-to-feature map
- `BBS02B`: `data/bbs02b_factory_reference_93points.csv` + target-filtered `motor_parameters_3_profiles.csv`. Origin family parameters identified as proxy. No arbitrary 2D interpolations; no power curve at unsupported voltage/torque combinations.
- `BBSHD`: `data/bbshd_factory_4_landmarks.csv` + own Grin measured parameter subset. Never transplant BBS02B 750 W sweep to this 1000 W motor.
- `TSDZ2`: `data/motor_parameters_3_profiles.csv` plus `tsdz2_external_bench_9points.csv` source-motor TSDZ2B; transfer only *hypotheses* to test, not calibration/limits.

## Repo code paths to inspect
`src/firmware/app.c` (`adaptive_pas_current`, `apply_current_ramp_up`, `apply_current_ramp_down`, safety clamps, `reload_assist_params`), `src/firmware/cfgstore.h/c`, `src/firmware/extcom.c`, `src/firmware/tsdz2/motor.c`, `src/tool/Model/Configuration.cs`, `src/tool/ViewModel/AssistLevelViewModel.cs`, `src/tool/View/AssistLevelPasView.xaml`, `src/tool/View/Controls/ResponseChart.cs`.

## Memory, protocol and hardening
The PR #9 reported **BBS02** XRAM904 bytes, BBSHD XRAM918 bytes, TSDZ2 DATA587 + INITIALIZED61 bytes. These are PR-specific reports and MUST be re-measured on the current code; linker-stated free stack is NOT peak runtime ISR+call stack margin. Do not grow 156-byte config / 161-byte UART request inadvertently, and preserve v5/v6→v7 EEPROM migration and 0x80 flag reuse safeguards. SDCC 8051/STM8 application: bound intermediate widths, no unchecked division/shift, no float in per-tick control, no heap allocations or unbounded search loops.

## Acceptance gates
Every on-device current reduction remains subordinate to original electrical protections; legacy modes unaffected unless separately approved. Host/portable .NET tests, WPF UI tests, clean target builds and stack/timing reports. Wheel-clear bench verification including throttle, coast, brake, LVC, speed, shift, thermal and watchdog. Road A/B matched for speed, route, mass and rider contribution; report quantitative range only after real Wh/km evidence.
