/* Exercise stock BBS thermal configurations through real sensors and app code. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sensor_fixture.h"
#include "../src/firmware/battery.c"
#include "../src/firmware/app.c"

static uint32_t now;
static uint8_t motor_current;
static bool motor_enabled;
static bool throttle_pressed = true;
uint32_t system_ms(void) { return now; }
uint16_t motor_get_battery_voltage_x10(void) { return 480; }
uint16_t motor_get_battery_current_x10(void) { return 0; }
uint8_t motor_get_target_current(void) { return motor_current; }
void motor_set_target_current(uint8_t value) { motor_current = value; }
void motor_set_target_speed(uint8_t value) { (void)value; }
void motor_enable(void) { motor_enabled = true; }
void motor_disable(void) { motor_enabled = false; }
uint16_t motor_status(void) { return 0; }
void lights_set(bool value) { (void)value; }
void lights_enable(void) {}
void lights_disable(void) {}
bool throttle_ok(void) { return true; }
uint8_t throttle_read(void) { return throttle_pressed ? 100 : 0; }
uint8_t throttle_map_response(uint8_t value) { return value; }
bool eventlog_is_enabled(void) { return false; }
void eventlog_write(uint8_t event) { (void)event; }

static void run(unsigned milliseconds)
{
    for (unsigned i = 0; i < milliseconds; i += 5)
    {
        now += 5;
        battery_process();
        app_process();
    }
}

static void expect_fault(uint8_t status)
{
    run(5);
    assert(motor_current == 0 && !motor_enabled);
    assert(app_get_status_code() == status);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    sensors_init();
    PIN_VALUE(PIN_BRAKE) = true;
    PIN_VALUE(PIN_SHIFT_SENSOR) = true;
    g_config.max_current_amps = 25;
    g_config.current_ramp_amps_s = 100;
    g_config.max_battery_x100v_u16h = 5880 >> 8;
    g_config.max_battery_x100v_u16l = (uint8_t)5880;
    g_config.low_cut_off_v = 42;
    g_config.wheel_size_inch_x10_u16h = 280 >> 8;
    g_config.wheel_size_inch_x10_u16l = (uint8_t)280;
    g_config.speed_sensor_signals = 1;
    g_config.max_speed_kph = 25;
    g_config.assist_startup_level = 1;
    g_config.assist_levels[0][1].flags = ASSIST_FLAG_THROTTLE;
    g_config.assist_levels[0][1].max_throttle_current_percent = 100;
    g_config.assist_levels[0][1].max_cadence_percent = 100;
    g_config.assist_levels[0][1].max_speed_percent = 100;
    g_config.use_temperature_sensor = TEMPERATURE_SENSOR_CONTR;
    bool controller_only = !strcmp(argv[1], "controller_only") || !strcmp(argv[1], "controller_startup");
    bool ptc = !strcmp(argv[1], "ptc") || !strcmp(argv[1], "startup_ptc");
    bool startup = !strcmp(argv[1], "startup") || !strcmp(argv[1], "startup_ptc");
    if (controller_only)
    {
        motor_adc = 1023;
        if (!strcmp(argv[1], "controller_startup")) controller_adc = 0;
    }
    else
    {
        g_config.use_temperature_sensor |= TEMPERATURE_SENSOR_MOTOR;
        if (startup) controller_adc = motor_adc = 0;
        else if (ptc) motor_adc = 168;
        else if (!strcmp(argv[1], "absent")) motor_adc = 1023;
        else assert(!strcmp(argv[1], "ntc"));
    }
    battery_init(); app_init();
    if (controller_only && controller_adc == 0)
    {
        expect_fault(STATUS_ERROR_CONTROLLER_TEMP_SENSOR);
        controller_adc = 677;
    }
    if (startup)
    {
        expect_fault(STATUS_ERROR_MOTOR_TEMP_SENSOR);
        /* Once the controller ADC is ready, the enabled motor input still blocks. */
        controller_adc = 677;
        expect_fault(STATUS_ERROR_MOTOR_TEMP_SENSOR);
        motor_adc = ptc ? 168 : 677;
    }
    if (!strcmp(argv[1], "absent"))
    {
        expect_fault(STATUS_ERROR_MOTOR_TEMP_SENSOR);
        assert(g_config.use_temperature_sensor == (TEMPERATURE_SENSOR_CONTR | TEMPERATURE_SENSOR_MOTOR));
        motor_adc = 0;
        expect_fault(STATUS_ERROR_MOTOR_TEMP_SENSOR);
        motor_adc = 677;
    }
    run(3000);
    assert(motor_current == 100 && motor_enabled && app_get_status_code() == STATUS_NORMAL);
    if (controller_only)
    {
        motor_adc = 0; run(5);
        assert(motor_current == 100 && app_get_status_code() == STATUS_NORMAL);
        /* Both missing-sensor rails are ignored only while the optional input is disabled. */
        assert(temperature_motor_x100() == 0);
    }
    else
    {
#if HAS_MOTOR_TEMP_SENSOR
        assert(bbshd_ptc_thermistor == ptc);
        int16_t measured = temperature_motor_x100();
        assert(ptc ? measured >= 0 && measured < 100 : measured > 2400 && measured < 2600);
#endif
        const uint16_t rails[] = {0, 1023};
        for (unsigned i = 0; i < sizeof rails / sizeof rails[0]; ++i)
        {
            motor_adc = rails[i];
            expect_fault(STATUS_ERROR_MOTOR_TEMP_SENSOR);
            motor_adc = ptc ? 168 : 677;
            run(3000);
            assert(motor_current == 100 && app_get_status_code() == STATUS_NORMAL);
        }
    }
    const uint16_t rails[] = {0, 1023};
    for (unsigned i = 0; i < sizeof rails / sizeof rails[0]; ++i)
    {
        controller_adc = rails[i];
        expect_fault(STATUS_ERROR_CONTROLLER_TEMP_SENSOR);
        controller_adc = 677; run(3000);
        assert(motor_current == 100 && app_get_status_code() == STATUS_NORMAL);
    }
    /* Controller overtemperature and braking still override full throttle. */
    controller_adc = 30; run(10000);
    assert(motor_current == 0 && app_get_status_code() == STATUS_ERROR_CONTROLLER_OVER_TEMP);
    controller_adc = 677; run(10000);
    assert(motor_current == 100);
    PIN_VALUE(PIN_BRAKE) = false; run(5);
    assert(motor_current == 0 && !motor_enabled);
    puts("Factory thermal configuration, rail faults, readiness and full-throttle protection: PASS");
    return 0;
}
