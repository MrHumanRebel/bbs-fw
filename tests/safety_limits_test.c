/* Exercise immediate protection reductions through the real app pipeline. */
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main

int main(int argc, char **argv)
{
    assert(argc == 3);
    setup();
    g_config.use_voltage_compensation = 0;
    if (!strcmp(argv[2], "adaptive") || !strcmp(argv[2], "adaptive_pas") || !strncmp(argv[2], "efficiency", 10))
    {
        g_config.assist_levels[0][1].flags |= ASSIST_FLAG_PAS_EFFICIENCY;
        g_config.assist_levels[0][1].torque_amplification_factor_x10 = 25;
        g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE;
        if (!strncmp(argv[2], "efficiency", 10))
        {
            g_config.assist_levels[0][1].flags |= ASSIST_FLAG_PAS_VARIABLE;
            g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE;
            if (!strcmp(argv[2], "efficiency_comfort")) g_config.use_voltage_compensation |= SYSTEM_EFFICIENCY_COMFORT;
        }
        throttle = !strcmp(argv[2], "adaptive") ? 100 : 0; /* Test PAS and its throttle override separately. */
    }
    else if (!strcmp(argv[2], "throttle"))
    {
        g_config.assist_levels[0][1].flags = ASSIST_FLAG_THROTTLE;
        throttle = 100;
    }
    else if (!strcmp(argv[2], "variable"))
    {
        g_config.assist_levels[0][1].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_VARIABLE;
        throttle = 100;
    }
    else if (!strcmp(argv[2], "cruise"))
    {
        g_config.assist_levels[0][1].flags = ASSIST_FLAG_CRUISE;
        throttle = 80;
    }
#if HAS_TORQUE_SENSOR
    else if (!strcmp(argv[2], "torque"))
    {
        g_config.assist_levels[0][1].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_TORQUE;
        g_config.assist_levels[0][1].torque_amplification_factor_x10 = 10;
    }
#endif
    if (!strcmp(argv[1], "zero_pas"))
    {
        g_config.use_speed_sensor = true; g_config.use_pretension = true;
        g_config.assist_levels[0][1].target_current_percent = 0;
        wheel_speed = 1; pedaling = false; /* Coasting must not create idle PAS current. */
    }
    start();
    if (!strcmp(argv[2], "walk")) { g_config.use_push_walk = 1; app_set_assist_level(ASSIST_PUSH); }
    run(2000);
    if (!strcmp(argv[1], "zero_pas"))
    {
        assert(motor_current == 0);
        throttle = 50; run(1000); assert(motor_current == 50);
        puts("Zero-current PAS suppresses pretension and preserves optional throttle: PASS");
        return 0;
    }
    assert(motor_current >= 10);
    if (!strcmp(argv[1], "thermal"))
    {
        temperature = 8500;
        run(5);
        assert(motor_current <= MAX_TEMPERATURE_LOW_CURRENT_PERCENT);
        temperature = 8501; run(5); assert(motor_current == 0);
        temperature = 20000; /* A signed 8-bit display temperature must not wrap the limiter. */
        run(5);
        assert(motor_current == 0);
        assert(app_get_status_code() == STATUS_ERROR_MOTOR_OVER_TEMP);
    }
    else if (!strcmp(argv[1], "sensor_controller") || !strcmp(argv[1], "sensor_motor"))
    {
        controller_temperature_failed = !strcmp(argv[1], "sensor_controller");
        motor_temperature_failed = !strcmp(argv[1], "sensor_motor");
        run(5);
        assert(motor_current == 0 && !motor_enabled);
        assert(app_get_status_code() == (controller_temperature_failed ?
            STATUS_ERROR_CONTROLLER_TEMP_SENSOR : STATUS_ERROR_MOTOR_TEMP_SENSOR));
        assert(app_get_temperature() == 0); /* Never publish the fault as a hot/cold measurement. */
        controller_temperature_failed = false; motor_temperature_failed = false;
        run(3000); assert(motor_current >= 10);
    }
    else if (!strcmp(argv[1], "cutoff"))
    {
        voltage = g_config.low_cut_off_v * 10u;
        run(5);
        assert(motor_current == 0);
    }
    else if (!strcmp(argv[1], "invalid_voltage"))
    {
        voltage = 0; run(5); assert(motor_current == 0);
        voltage = 1001; run(5); assert(motor_current == 0);
    }
    else if (!strcmp(argv[1], "lvc_wrap"))
    {
        now = UINT32_MAX - 130u;
        run(130); /* The old absolute deadline can become UINT32_MAX here. */
        voltage = g_config.low_cut_off_v * 10u + 1u;
        run(5000);
        assert(motor_current == 0); /* Above hard cutoff: filtered ramp must continue after wrap. */
    }
    else if (!strcmp(argv[1], "thermal_curve"))
    {
        for (int32_t sample = INT16_MIN; sample <= INT16_MAX; ++sample)
        {
            temperature = sample;
            requested_current = 100;
            apply_thermal_limit();
            assert(app_get_temperature() <= 127); /* Keeps the display conversion input bound valid. */
            uint8_t expected = sample == TEMPERATURE_SENSOR_FAULT_X100 || sample > MAX_TEMPERATURE * 100 ? 0 :
                sample < (MAX_TEMPERATURE - MAX_TEMPERATURE_RAMP_DOWN_INTERVAL) * 100 ? 100 :
                (uint8_t)MAP32(sample,
                    (MAX_TEMPERATURE - MAX_TEMPERATURE_RAMP_DOWN_INTERVAL) * 100,
                    MAX_TEMPERATURE * 100, 100, MAX_TEMPERATURE_LOW_CURRENT_PERCENT);
            assert(requested_current == expected);
        }
    }
    else if (!strcmp(argv[1], "fault"))
    {
        const uint16_t faults[] = {MOTOR_ERROR_LVC, MOTOR_ERROR_HALL_SENSOR, MOTOR_ERROR_CURRENT_SENSE};
        for (unsigned i = 0; i < sizeof faults / sizeof faults[0]; ++i)
        {
            motor_faults = faults[i]; run(5);
            assert(motor_current == 0 && !motor_enabled);
            motor_faults = 0; run(2000); assert(motor_current >= 10);
        }
    }
    else if (!strcmp(argv[1], "unknown_status"))
    {
        uint8_t previous = motor_current;
        motor_faults = MOTOR_ERROR_POWER_RESET;
        run(5);
        assert(motor_current == previous && app_get_status_code() == STATUS_ERROR_PHASE_LINE);
    }
    else if (!strcmp(argv[1], "keep_range"))
    {
        g_config.pas_keep_current_cadence_rpm = MAX_CADENCE_RPM_X10 / 10;
        g_config.assist_levels[0][1].max_cadence_percent = 100;
        pedal_cadence = MAX_CADENCE_RPM_X10 + 10;
        reload_assist_params(); run(500);
        assert(motor_current <= 100);
    }
    else assert(0);
    printf("Immediate %s protection (%s): PASS\n", argv[1], argv[2]);
    return 0;
}
