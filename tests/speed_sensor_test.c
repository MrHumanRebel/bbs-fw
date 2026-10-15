/* Host regression tests for the real BBS02/BBSHD sensor ISR.
 * Only the STC GPIO and interrupt registers are replaced.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define __SDCC 1
#define _STC_15_H_
static bool pins[6][8];
static bool ET0;
#define GET_PIN_STATE_(port, pin) pins[port][pin]
#define GET_PIN_STATE(...) GET_PIN_STATE_(__VA_ARGS__)
#define SET_PIN_INPUT(...) ((void)0)
#define SET_PIN_QUASI(...) ((void)0)

#include "../src/firmware/bbsx/sensors.c"

config_t g_config;
void timer0_init_sensors(void) {}
uint16_t adc_get_temperature_contr(void) { return 0; }
uint16_t adc_get_temperature_motor(void) { return 0; }
void eventlog_write_data(uint8_t evt, int16_t data) { (void)evt; (void)data; }

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); \
		return false; \
	} \
} while (0)

static void sample(bool level, unsigned ticks)
{
	GET_PIN_STATE(PIN_SPEED_SENSOR) = level;
	while (ticks--) sensors_timer0_isr();
}

static void reset(void)
{
	memset(pins, 0, sizeof pins);
	sensors_init();
}

/* 220 ms/revolution is about 35 km/h with a 2.14 m wheel. */
static void establish_speed(void)
{
	reset();
	for (unsigned i = 0; i < 4; i++)
	{
		sample(false, 2100);
		sample(true, 100);
	}
}

static bool normal_and_acceleration(void)
{
	establish_speed();
	CHECK(speed_sensor_get_rpm_x10() >= 2727 && speed_sensor_get_rpm_x10() <= 2730);
	sample(false, 1500);
	sample(true, 100);
	CHECK(speed_sensor_get_rpm_x10() >= 3750 && speed_sensor_get_rpm_x10() <= 3753);
	sample(false, 1100);
	sample(true, 100);
	CHECK(speed_sensor_get_rpm_x10() >= 5000 && speed_sensor_get_rpm_x10() <= 5005);
	return true;
}

static bool short_high_glitch(void)
{
	/* Test every rejected width, at different phases between real edges. */
	for (unsigned width = 1; width < 10; width++)
	{
		for (unsigned phase = 600; phase <= 1800; phase += 600)
		{
			establish_speed();
			uint16_t expected = speed_sensor_get_rpm_x10();
			sample(false, phase);
			sample(true, width);
			CHECK(speed_sensor_get_rpm_x10() == expected);
			sample(false, 2100 - phase - width);
			sample(true, 100);
			CHECK(speed_sensor_get_rpm_x10() == expected);
		}
	}
	return true;
}

static bool short_low_glitch(void)
{
	for (unsigned width = 1; width < 10; width++)
	{
		reset();
		for (unsigned i = 0; i < 4; i++)
		{
			sample(false, 200);
			sample(true, 2000);
		}
		uint16_t expected = speed_sensor_get_rpm_x10();
		sample(false, 200);
		sample(true, 1100);
		CHECK(speed_sensor_get_rpm_x10() == expected);
		sample(false, width);
		sample(true, 900 - width);
		CHECK(speed_sensor_get_rpm_x10() == expected);
		sample(false, 200);
		sample(true, 100);
		CHECK(speed_sensor_get_rpm_x10() == expected);
	}
	return true;
}

static bool alternating_noise(void)
{
	establish_speed();
	uint16_t expected = speed_sensor_get_rpm_x10();
	sample(false, 600);
	for (unsigned i = 0; i < 30; i++)
	{
		sample(true, 1);
		sample(false, 1);
	}
	CHECK(speed_sensor_get_rpm_x10() == expected);
	return true;
}

static bool minimum_width_pulses(void)
{
	reset();
	for (unsigned i = 0; i < 5; i++)
	{
		sample(false, 2100);
		sample(true, 10);
	}
	CHECK(speed_sensor_get_rpm_x10() >= 2843 && speed_sensor_get_rpm_x10() <= 2846);
	return true;
}

static bool timeout_and_restart(void)
{
	for (unsigned level = 0; level < 2; level++)
	{
		establish_speed();
		sample(level, 70000); /* Includes the 16-bit counter saturation. */
		CHECK(!speed_sensor_is_moving());
		CHECK(speed_sensor_get_rpm_x10() == 0);
		for (unsigned i = 0; i < 3; i++)
		{
			sample(false, 2100);
			sample(true, 100);
		}
		CHECK(speed_sensor_get_rpm_x10() >= 2727 && speed_sensor_get_rpm_x10() <= 2730);
	}
	return true;
}

static bool minimum_low_width(void)
{
	reset();
	for (unsigned i = 0; i < 5; i++)
	{
		sample(false, 10);
		sample(true, 2190);
	}
	CHECK(speed_sensor_get_rpm_x10() >= 2727 && speed_sensor_get_rpm_x10() <= 2730);
	return true;
}

static bool noise_does_not_keep_moving(void)
{
	establish_speed();
	sample(false, 100);
	for (unsigned i = 0; i < 2600; i++)
	{
		sample(true, 1);
		sample(false, 9);
	}
	CHECK(!speed_sensor_is_moving());
	return true;
}

static bool too_fast_edges(void)
{
	reset();
	sample(false, 2100);
	sample(true, 100);
	uint16_t expected = speed_sensor_get_rpm_x10();
	/* Even a debounced edge must respect the existing 50 ms limit. */
	sample(false, 100);
	sample(true, 100);
	CHECK(speed_sensor_get_rpm_x10() == expected);
	return true;
}

static bool multiple_magnets(void)
{
	establish_speed();
	uint16_t expected = speed_sensor_get_rpm_x10();
	speed_sensor_set_signals_per_rpm(2);
	CHECK(speed_sensor_get_rpm_x10() == expected / 2);
	return true;
}

int main(void)
{
	bool ok = true;
#define RUN(test) do { bool passed = test(); printf("%s: %s\n", #test, passed ? "PASS" : "FAIL"); ok &= passed; } while (0)
	RUN(normal_and_acceleration);
	RUN(short_high_glitch);
	RUN(short_low_glitch);
	RUN(alternating_noise);
	RUN(minimum_width_pulses);
	RUN(timeout_and_restart);
	RUN(minimum_low_width);
	RUN(noise_does_not_keep_moving);
	RUN(too_fast_edges);
	RUN(multiple_magnets);
	return ok ? 0 : 1;
}
