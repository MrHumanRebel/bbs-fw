/* Test production battery filtering and assist/limit ordering with mocked I/O. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __SDCC 1
#include "../src/firmware/battery.c"
#include "../src/firmware/app.c"

config_t g_config;
static uint32_t now;
static uint16_t voltage = 480, wheel_speed;
static uint8_t motor_current, motor_cadence, throttle;
static bool pedaling = true, backwards, brake, shift;
static int16_t temperature;
uint32_t system_ms(void) { return now; }
uint16_t motor_get_battery_voltage_x10(void) { return voltage; }
uint8_t motor_get_target_current(void) { return motor_current; }
void motor_set_target_current(uint8_t value) { motor_current = value; }
void motor_set_target_speed(uint8_t value) { motor_cadence = value; }
void motor_enable(void) {}
void motor_disable(void) {}
uint16_t motor_status(void) { return 0; }
void lights_set(bool value) { (void)value; }
void lights_enable(void) {}
void lights_disable(void) {}
bool throttle_ok(void) { return true; }
uint8_t throttle_read(void) { return throttle; }
uint8_t throttle_map_response(uint8_t value) { return value; }
bool pas_is_pedaling_forwards(void) { return pedaling; }
bool pas_is_pedaling_backwards(void) { return backwards; }
uint16_t pas_get_pulse_counter(void) { return 100; }
uint16_t pas_get_cadence_rpm_x10(void) { return MAX_CADENCE_RPM_X10; }
uint16_t speed_sensor_get_rpm_x10(void) { return wheel_speed; }
bool speed_sensor_is_moving(void) { return wheel_speed > 0; }
uint16_t torque_sensor_get_nm_x100(void) { return 1000; }
bool torque_sensor_ok(void) { return true; }
bool brake_is_activated(void) { return brake; }
bool shift_sensor_is_activated(void) { return shift; }
int16_t temperature_contr_x100(void) { return temperature; }
int16_t temperature_motor_x100(void) { return temperature; }
bool eventlog_is_enabled(void) { return false; }
void eventlog_write(uint8_t event) { (void)event; }
void eventlog_write_data(uint8_t event, int16_t value) { (void)event; (void)value; }

static void setup(void)
{
	g_config.max_current_amps = 25;
	g_config.current_ramp_amps_s = 100;
    g_config.use_voltage_compensation = 1;
	g_config.max_battery_x100v_u16h = 5880 >> 8;
	g_config.max_battery_x100v_u16l = 5880 & 255;
	g_config.low_cut_off_v = 42;
	g_config.wheel_size_inch_x10_u16h = 280 >> 8;
	g_config.wheel_size_inch_x10_u16l = 280 & 255;
	g_config.max_speed_kph = 25;
	g_config.assist_startup_level = 1;
	g_config.pas_keep_current_percent = 60;
	g_config.pas_keep_current_cadence_rpm = 40;
	g_config.assist_levels[0][1].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_THROTTLE;
	g_config.assist_levels[0][1].target_current_percent = 100;
	g_config.assist_levels[0][1].max_throttle_current_percent = 100;
	g_config.assist_levels[0][1].max_cadence_percent = 100;
	g_config.assist_levels[0][1].max_speed_percent = 100;
}

static void start(void)
{
	battery_init();
	app_init();
}

static void run(unsigned milliseconds)
{
	for (unsigned i = 0; i < milliseconds; i += 5)
	{
		now += 5;
		battery_process();
		app_process();
	}
}

/* Frozen pre-optimization formula; include every uint16_t/uint8_t input, including clamps. */
static uint16_t reference_current_ramp_curve(uint16_t progress, uint8_t smoothness)
{
    progress = progress < 1000u ? progress : 1000u;
    smoothness = smoothness < 100u ? smoothness : 100u;
    uint32_t smooth = ((uint32_t)progress * progress * (3000ul - 2ul * progress)) / 1000000ul;
    return (uint16_t)(((uint32_t)progress * (100u - smoothness) + smooth * smoothness) / 100u);
}

int main(int argc, char **argv)
{
	assert(argc == 2 || argc == 3);
	const char *test = argv[1];
	setup();
    if (argc == 3) g_config.current_ramp_smoothness_percent = 100;
	if (!strcmp(test, "curve"))
    {
        for (uint16_t shape = 0; shape <= 100; ++shape)
        {
            uint16_t last = 0;
            assert(current_ramp_curve(0, shape) == 0);
            assert(current_ramp_curve(1000, shape) == 1000);
            for (uint16_t t = 0; t <= 1000; ++t)
            {
                uint16_t value = current_ramp_curve(t, shape);
                assert(value >= last && value <= 1000);
                if (!shape) assert(value == t);
                last = value;
            }
        }
        assert(current_ramp_curve(250, 100) == 156);
        assert(current_ramp_curve(500, 100) == 500);
        assert(current_ramp_curve(750, 100) == 843);
    }
    else if (!strcmp(test, "curve_equivalence"))
    {
        for (uint32_t progress = 0; progress <= UINT16_MAX; ++progress)
            for (uint16_t shape = 0; shape <= UINT8_MAX; ++shape)
                assert(current_ramp_curve((uint16_t)progress, (uint8_t)shape) ==
                    reference_current_ramp_curve((uint16_t)progress, (uint8_t)shape));
    }
    else if (!strcmp(test, "ramp_profile"))
    {
        g_config.current_ramp_amps_s = 10;
        g_config.current_ramp_smoothness_percent = 100;
        start();
        uint8_t value = 0, previous = 0;
        apply_current_ramp_up(&value, true);
        for (unsigned t = 0; t <= 2500; t += 5)
        {
            now = t; value = 100; apply_current_ramp_up(&value, true);
            assert(value >= previous && value <= 100);
            if (t == 625) assert(value == 15);
            if (t == 1250) assert(value == 50);
            previous = value;
        }
        assert(value == 100);
        value = 0; apply_current_ramp_up(&value, false); assert(value == 0);
        now = UINT32_MAX - 100; value = 60; apply_current_ramp_up(&value, true);
        now += 1500; value = 60; apply_current_ramp_up(&value, true); assert(value == 60);
        value = 10; apply_current_ramp_up(&value, true); assert(value == 10);
        value = 90; apply_current_ramp_up(&value, false); assert(value == 90);
        g_config.current_ramp_smoothness_percent = 0;
        g_config.current_ramp_amps_s = 255;
        now = 0; value = 0; apply_current_ramp_up(&value, false);
        value = 100; apply_current_ramp_up(&value, true);
        now = 50; value = 100; apply_current_ramp_up(&value, true); assert(value >= 50 && value <= 52);
        now = 100; value = 100; apply_current_ramp_up(&value, true); assert(value == 100);
    }
    else if (!strcmp(test, "global_compensation"))
    {
        start(); battery_process();
        for (uint8_t mode = 0; mode < 2; ++mode)
            for (uint8_t level = 0; level < 10; ++level)
            {
                g_config.assist_levels[mode][level].flags = ASSIST_FLAG_PAS;
                g_config.assist_levels[mode][level].target_current_percent = 100;
                app_set_operation_mode(mode);
                app_set_assist_level(level);
                assert(compensate_pas_current(60) == 73);
                g_config.use_voltage_compensation = 0;
                assert(compensate_pas_current(60) == 60);
                g_config.use_voltage_compensation = 1;
            }
    }
    else if (!strcmp(test, "disabled"))
	{
		g_config.use_voltage_compensation = 0;
		start(); run(2000); assert(motor_current == 60);
	}
	else if (!strcmp(test, "compensated"))
	{
		start(); run(2000); assert(motor_current == 73);
		/* Full-charge and discharged electrical power differ by <1%. */
		assert((uint32_t)motor_current * voltage * 100 / (60u * 588u) >= 99);
	}
	else if (!strcmp(test, "full"))
	{
		voltage = 588; start(); run(2000); assert(motor_current == 60);
	}
	else if (!strcmp(test, "cap"))
	{
		g_config.assist_levels[0][1].target_current_percent = 40;
		g_config.pas_keep_current_percent = 90;
		start(); run(2000); assert(motor_current == 40);
	}
	else if (!strcmp(test, "throttle"))
	{
		pedaling = false; throttle = 80;
		start(); run(2000); assert(motor_current == 80);
		pedaling = true; run(1000); assert(motor_current == 80);
	}
	else if (!strcmp(test, "inactive"))
	{
		pedaling = false; start(); run(2000); assert(motor_current == 0);
	}
	else if (!strcmp(test, "variable"))
	{
		g_config.assist_levels[0][1].flags |= ASSIST_FLAG_PAS_VARIABLE;
		g_config.assist_levels[0][1].flags &= ~ASSIST_FLAG_THROTTLE;
		throttle = 50; start(); run(2000); assert(motor_current == 61);
	}
	else if (!strcmp(test, "invalid"))
	{
		start(); voltage = 0; battery_process();
		uint8_t current = 60; current = compensate_pas_current(current); assert(current == 60);
		voltage = 65535; battery_process();
		current = compensate_pas_current(current); assert(current == 60);
		voltage = 480; battery_process();
		g_config.max_battery_x100v_u16h = 0; g_config.max_battery_x100v_u16l = 0;
		current = compensate_pas_current(current); assert(current == 60);
	}
	else if (!strcmp(test, "bounds"))
	{
		start();
		for (unsigned limit = 0; limit <= 100; limit++)
		{
			assist_level_data.level.target_current_percent = limit;
			for (unsigned input = 0; input <= limit; input++)
			{
				for (unsigned v = 4300; v <= 6200; v += 100)
				{
					filtered_voltage_x1000 = v * 10;
					uint8_t result = compensate_pas_current(input);
					assert(result >= input && result <= limit);
					if (input == 0) assert(result == 0);
					if (v >= 5880) assert(result == input);
				}
			}
		}
	}
	else if (!strcmp(test, "filter"))
	{
		start(); battery_process(); assert(battery_get_filtered_voltage_x100() == 4800);
		voltage = 588; now = 124; battery_process(); assert(battery_get_filtered_voltage_x100() == 4800);
		now = 125; battery_process();
		assert(battery_get_filtered_voltage_x100() == 4867);
		for (unsigned i = 0; i < 160; i++) { now += 125; battery_process(); }
		assert(battery_get_filtered_voltage_x100() >= 5878);
		voltage = 480; now += 125; battery_process();
		assert(battery_get_filtered_voltage_x100() > 5800);
		for (unsigned i = 0; i < 160; i++) { now += 125; battery_process(); }
		assert(battery_get_filtered_voltage_x100() <= 4802);
	}
	else if (!strcmp(test, "wrap"))
	{
		start(); now = UINT32_MAX - 60; battery_process();
		voltage = 588; now = 64; battery_process();
		assert(battery_get_filtered_voltage_x100() == 4867);
	}
	else if (!strcmp(test, "brake"))
	{
		start(); run(2000); assert(motor_current == 73);
		brake = true; run(5); assert(motor_current == 0);
	}
	else if (!strcmp(test, "speed"))
	{
		g_config.use_speed_sensor = true;
		wheel_speed = 10000; start(); run(2000); assert(motor_current <= 1);
	}
	else if (!strcmp(test, "thermal"))
	{
		temperature = 10000; start(); run(2000); assert(motor_current <= 20);
	}
	else if (!strcmp(test, "lvc"))
	{
		voltage = 420; start(); run(20000); assert(motor_current <= 20);
	}
	else if (!strcmp(test, "walk"))
	{
		g_config.use_push_walk = true;
		g_config.assist_startup_level = ASSIST_PUSH;
		start(); run(2000); assert(motor_current == 10);
	}
	else if (!strcmp(test, "walk_release_immediate"))
	{
		g_config.use_push_walk = true;
		start(); app_set_assist_level(ASSIST_PUSH); run(2000); assert(motor_current == 10 && motor_cadence == 15);
		app_set_assist_level(ASSIST_1); run(5);
		assert(motor_current == 0); /* No old walk current at the new PAS cadence. */
	}
	else if (!strcmp(test, "walk_release_wait_stop"))
	{
		g_config.use_push_walk = true;
		start(); app_set_assist_level(ASSIST_PUSH); run(2000);
		app_set_assist_level(ASSIST_1);
		/* Simulate pedals still being driven by the motor beyond the timer. */
		run(2500); assert(motor_current == 0);
		pedaling = false; backwards = true;
		run(500); assert(motor_current == 0); /* Both directions must stop. */
		backwards = false; run(5);
		pedaling = true; run(1000); assert(motor_current == 73);
	}
	else if (!strcmp(test, "walk_release_throttle"))
	{
		g_config.use_push_walk = true;
		start(); app_set_assist_level(ASSIST_PUSH); run(2000); app_set_assist_level(ASSIST_1);
		run(1100); assert(motor_current == 0);
		throttle = 80; run(500); assert(motor_current == 80);
	}
	else if (!strcmp(test, "walk_release_wrap"))
	{
		g_config.use_push_walk = true;
		start(); app_set_assist_level(ASSIST_PUSH); run(2000);
		now = UINT32_MAX - 500;
		app_set_assist_level(ASSIST_1);
		assert(check_power_block());
		now += 999; assert(check_power_block());
		now += 1; assert(!check_power_block());
	}
	else if (!strcmp(test, "walk_release_cruise"))
	{
		g_config.use_push_walk = true;
		g_config.assist_levels[0][1].flags = ASSIST_FLAG_CRUISE;
		start(); app_set_assist_level(ASSIST_PUSH); run(2000); app_set_assist_level(ASSIST_1);
		run(1100); throttle = 80; run(500);
		assert(motor_current == 0); /* Old walk pulses cannot engage cruise. */
		pedaling = false; throttle = 0; run(5);
		pedaling = true; throttle = 80; run(1000);
		assert(motor_current == 100); /* Fresh rider input can engage it. */
	}
	else if (!strcmp(test, "walk_display_only"))
	{
		g_config.walk_mode_data_display = WALK_MODE_DATA_TEMPERATURE;
		pedaling = false; start(); run(200);
		app_set_assist_level(ASSIST_PUSH); run(200); assert(motor_current == 0);
		app_set_assist_level(ASSIST_1); pedaling = true;
		run(500); assert(motor_current == 73); /* No exit block if motor walk was off. */
	}
	else if (!strcmp(test, "speed_limit_cutoff"))
	{
		g_config.use_speed_sensor = true; g_config.max_speed_kph = 6;
		start(); wheel_speed = assist_level_data.max_wheel_speed_rpm_x10;
		uint8_t current = 60;
		assert(apply_speed_limit(&current, 100, false, true)); assert(current == 0);
		wheel_speed += 100; current = 0;
		assert(apply_speed_limit(&current, 0, false, false)); assert(current == 0);
		wheel_speed = 65535; current = 60;
		assert(apply_speed_limit(&current, 100, false, true)); assert(current == 0);
	}
	else if (!strcmp(test, "speed_limit_ramp"))
	{
		g_config.use_speed_sensor = true; g_config.max_speed_kph = 6;
		start(); uint16_t limit = assist_level_data.max_wheel_speed_rpm_x10;
		uint8_t previous = 60;
		for (wheel_speed = 0; wheel_speed <= limit; wheel_speed++)
		{
			uint8_t current = 60; apply_speed_limit(&current, 100, false, true);
			assert(current <= previous); previous = current;
		}
		assert(previous == 0);
		wheel_speed = 0; uint8_t current = 60;
		assert(!apply_speed_limit(&current, 100, false, true)); assert(current == 60);
	}
	else if (!strcmp(test, "speed_limit_app"))
	{
		g_config.use_speed_sensor = true; g_config.max_speed_kph = 6;
		pedaling = false; throttle = 100;
		start(); run(2000); assert(motor_current == 100);
		wheel_speed = assist_level_data.max_wheel_speed_rpm_x10;
		run(5); assert(motor_current == 0);
		throttle = 0; run(5); assert(motor_current == 0);
		wheel_speed = 0; throttle = 100; run(2000); assert(motor_current == 100);
	}
	else if (!strcmp(test, "speed_limit_global"))
	{
		g_config.use_speed_sensor = true; g_config.max_speed_kph = 100;
		g_config.throttle_global_spd_lim_opt = THROTTLE_GLOBAL_SPEED_LIMIT_ENABLED;
		g_config.throttle_global_spd_lim_percent = 6;
		start(); wheel_speed = global_throttle_speed_limit_rpm_x10;
		uint8_t current = 60;
		assert(apply_speed_limit(&current, 100, false, true)); assert(current == 0);
		current = 60;
		assert(!apply_speed_limit(&current, 100, true, true)); assert(current == 60);
	}
	else if (!strcmp(test, "speed_limit_override"))
	{
		g_config.use_speed_sensor = true;
		g_config.assist_levels[0][1].max_speed_percent = 20;
		g_config.assist_levels[0][1].flags |= ASSIST_FLAG_OVERRIDE_SPEED;
		start(); wheel_speed = assist_level_data.max_wheel_speed_rpm_x10;
		uint8_t current = 60;
		assert(!apply_speed_limit(&current, 100, false, true)); assert(current == 60);
		wheel_speed = global_speed_limit_rpm * 10; current = 60;
		assert(apply_speed_limit(&current, 100, false, true)); assert(current == 0);
	}
	else if (!strcmp(test, "speed_limit_disabled"))
	{
		start(); wheel_speed = 65535; uint8_t current = 60;
		assert(!apply_speed_limit(&current, 100, false, true)); assert(current == 60);
		g_config.use_speed_sensor = true; assist_level_data.max_wheel_speed_rpm_x10 = 0;
		assert(!apply_speed_limit(&current, 100, false, true)); assert(current == 60);
	}
	else if (!strcmp(test, "speed_limit_ranges"))
	{
		g_config.use_speed_sensor = true; start();
		const uint8_t speeds[] = {1, 4, 6, 25, 50};
		for (unsigned i = 0; i < sizeof speeds; i++)
		{
			app_set_wheel_max_speed_rpm(convert_wheel_speed_kph_to_rpm(speeds[i]));
			uint16_t limit = assist_level_data.max_wheel_speed_rpm_x10;
			uint8_t previous = 100;
			for (wheel_speed = 0; wheel_speed <= limit; wheel_speed++)
			{
				uint8_t current = 100;
				apply_speed_limit(&current, 100, false, true);
				assert(current <= previous); previous = current;
			}
			assert(previous == 0);
		}
		/* An interval rounded down to zero must never divide by zero. */
		assist_level_data.max_wheel_speed_rpm_x10 = 1;
		wheel_speed = 0; uint8_t current = 100;
		apply_speed_limit(&current, 100, false, true); assert(current == 100);
		wheel_speed = 1; apply_speed_limit(&current, 100, false, true); assert(current == 0);
	}
	else if (!strcmp(test, "shift"))
	{
#if HAS_SHIFT_SENSOR_SUPPORT
		g_config.use_shift_sensor = true;
		g_config.shift_interrupt_duration_ms_u16l = 200;
		g_config.shift_interrupt_current_threshold_percent = 10;
		start(); run(2000); shift = true; run(5); assert(motor_current <= 10);
#endif
	}
	else if (!strcmp(test, "torque"))
	{
#if HAS_TORQUE_SENSOR
		g_config.assist_levels[0][1].flags |= ASSIST_FLAG_PAS_TORQUE;
		g_config.assist_levels[0][1].torque_amplification_factor_x10 = 20;
		start(); battery_process(); uint8_t current = 0; apply_pas_torque(&current);
		uint8_t expected = current;
		g_config.use_voltage_compensation = 0;
		current = 0; apply_pas_torque(&current); assert(current == expected && current > 0);
#endif
	}
	else { fprintf(stderr, "Unknown test: %s\n", test); return 2; }
	printf("%s: PASS\n", test);
	return 0;
}
