/* A protection remains active even when rider demand falls below its current cap. */
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main

int main(int argc, char **argv)
{
    assert(argc == 3);
    setup();
    g_config.use_voltage_compensation = 0;
    throttle = 100;
    start(); run(2000); assert(motor_current == 100);
    bool clear = !strcmp(argv[1], "unlimited") || !strcmp(argv[1], "thermal_cool") || !strcmp(argv[1], "speed_clear");
    if (clear)
    {
        if (!strcmp(argv[1], "thermal_cool")) temperature = 7900;
        if (!strcmp(argv[1], "speed_clear"))
        {
            g_config.use_speed_sensor = true;
            wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 - speed_limit_ramp_interval_rpm_x10 - 1u;
        }
    }
    else if (!strcmp(argv[1], "thermal")) temperature = 8500;
    else if (!strcmp(argv[1], "speed"))
    {
        g_config.use_speed_sensor = true;
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 - speed_limit_ramp_interval_rpm_x10 / 2u;
    }
    else if (!strcmp(argv[1], "lvc"))
    {
        voltage = g_config.low_cut_off_v * 10u + 1u;
        /* Stage the filter update of the frame that also receives a released input.
         * Leave the previous motor/ramp output intact until app_process applies it. */
        for (unsigned sample = 0; sample < 100; ++sample)
        {
            now += 130;
            requested_current = 100;
            apply_low_voltage_limit();
            if (requested_current < 90) break;
        }
        assert(requested_current < 90);
    }
#if HAS_SHIFT_SENSOR_SUPPORT
    else if (!strcmp(argv[1], "shift") || !strcmp(argv[1], "shift_wrap"))
    {
        g_config.use_shift_sensor = 1;
        g_config.shift_interrupt_duration_ms_u16h = 600 >> 8;
        g_config.shift_interrupt_duration_ms_u16l = (uint8_t)600;
        g_config.shift_interrupt_current_threshold_percent = 10;
        if (!strcmp(argv[1], "shift_wrap"))
        {
            now = UINT32_MAX - 100;
            shift = true; run(5); shift = false; run(590); assert(motor_current <= 10);
            run(15); assert(motor_current == 100);
            puts("Shift protection retains its duration across clock wrap: PASS");
            return 0;
        }
        shift = true;
    }
#endif
    else assert(0);
    pedaling = false;
    throttle = !strcmp(argv[2], "small") ? 5 : 0;
    run(5);
    if (clear)
    {
        assert(motor_current > throttle && motor_current <= 100);
        printf("Normal falling ramp remains active outside protection (%s): PASS\n", argv[1]);
        return 0;
    }
    assert(motor_current <= throttle);
    printf("Active %s protection cannot restore released/smaller demand (%s): PASS\n", argv[1], argv[2]);
    return 0;
}
