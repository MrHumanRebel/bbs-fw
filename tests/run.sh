#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

for target in BBS02 BBSHD TSDZ2; do
	printf '\nTesting %s\n' "$target"
	cc -std=c99 -D"$target" -ffunction-sections -fdata-sections -I src/firmware \
		tests/display_current_test.c -Wl,--gc-sections -lm -o "$test_dir/current"
	"$test_dir/current"
	cc -std=c99 -D"$target" -ffunction-sections -fdata-sections -I src/firmware \
		tests/virtual_display_test.c -Wl,--gc-sections -lm -o "$test_dir/virtual"
	"$test_dir/virtual"
    cc -std=c99 -ffp-contract=off -D"$target" -I src/firmware tests/wheel_conversion_test.c -o "$test_dir/wheel"
    "$test_dir/wheel"
	cc -std=c99 -D"$target" -I src/firmware tests/cfgstore_migration_test.c -o "$test_dir/config"
	"$test_dir/config"
	cc -std=c99 -D"$target" -Wall -Wextra -Wno-type-limits -Wno-sign-compare \
		-I src/firmware tests/pas_voltage_test.c -o "$test_dir/voltage"
	for test in curve curve_equivalence ramp_profile global_compensation disabled compensated full cap throttle inactive variable invalid bounds filter wrap brake speed thermal lvc walk walk_release_immediate walk_release_wait_stop walk_release_throttle walk_release_wrap walk_display_only walk_release_cruise speed_limit_cutoff speed_limit_ramp speed_limit_app speed_limit_global speed_limit_override speed_limit_disabled speed_limit_ranges; do
		"$test_dir/voltage" "$test"
	done
    for test in brake speed thermal lvc walk walk_release_immediate walk_release_wait_stop walk_release_throttle walk_release_wrap walk_release_cruise; do
        "$test_dir/voltage" "$test" curved
    done
    cc -std=c99 -D"$target" -Wall -Wextra -Wno-type-limits -Wno-sign-compare \
        -I src/firmware tests/safety_limits_test.c -o "$test_dir/safety"
    "$test_dir/safety" thermal_curve legacy
    "$test_dir/safety" zero_pas legacy
    for mode in legacy adaptive adaptive_pas efficiency efficiency_comfort variable cruise throttle walk; do
        for scenario in thermal cutoff invalid_voltage fault keep_range lvc_wrap unknown_status; do "$test_dir/safety" "$scenario" "$mode"; done
    done
    if [ "$target" = TSDZ2 ]; then
        for scenario in thermal cutoff invalid_voltage fault keep_range lvc_wrap unknown_status; do "$test_dir/safety" "$scenario" torque; done
    fi
    cc -std=c99 -D"$target" -Wall -Wextra -Wno-type-limits -Wno-sign-compare \
        -I src/firmware tests/protection_release_test.c -o "$test_dir/release"
    for clear in unlimited thermal_cool speed_clear; do "$test_dir/release" "$clear" released; done
    for protection in thermal speed lvc; do
        for demand in released small; do "$test_dir/release" "$protection" "$demand"; done
    done
    if [ "$target" != TSDZ2 ]; then
        "$test_dir/release" shift released
        "$test_dir/release" shift small
        "$test_dir/release" shift_wrap held
    fi
    cc -std=c99 -D"$target" -Wall -Wextra -Wno-type-limits -Wno-sign-compare \
        -I src/firmware tests/adaptive_power_test.c -o "$test_dir/adaptive"
    for scenario in budget voltage coast compensation throttle profiles legacy_ramp retarget_plateau moving_ramp walk brake speed thermal lvc; do
        "$test_dir/adaptive" "$scenario"
    done
    if [ "$target" != TSDZ2 ]; then "$test_dir/adaptive" shift; fi
    cc -std=c99 -D"$target" -Wall -Wextra -Wno-type-limits -Wno-sign-compare \
        -I src/firmware tests/efficiency_test.c -o "$test_dir/efficiency"
    for scenario in budget_fraction clipped_budget real_budget_150 real_budget_250 real_budget_500 real_budget_750 load_hysteresis speed_target sensors comfort clock_mode slew; do
        "$test_dir/efficiency" "$scenario"
    done
    cc -std=c99 -D"$target" -Wall -Wextra -Wno-type-limits -Wno-sign-compare \
        -I src/firmware tests/efficiency_refinement_test.c -o "$test_dir/refinement"
    for scenario in speed_curve speed_recovery synthetic_ride throttle lvc_recovery lvc_hysteresis lvc_legacy lvc_scope_efficiency lvc_scope_legacy lvc_scope_adaptive lvc_scope_throttle jitter; do
        "$test_dir/refinement" "$scenario"
    done
	if [ "$target" = TSDZ2 ]; then
		"$test_dir/voltage" torque
	else
		cc -std=c99 -D"$target" -Wno-unknown-pragmas -I src/firmware \
			tests/uart_test.c -o "$test_dir/uart"
		"$test_dir/uart"
		"$test_dir/voltage" shift
		cc -std=c99 -D"$target" -Wall -Wextra -Wno-unknown-pragmas \
			-I src/firmware tests/speed_sensor_test.c -lm -o "$test_dir/speed"
		"$test_dir/speed"
	fi
done
