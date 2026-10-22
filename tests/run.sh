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
