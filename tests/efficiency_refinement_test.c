/* Offline control regressions. All inputs and plant responses are synthetic. */
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main

static void configure(void)
{
    setup();
    g_config.use_speed_sensor = true;
    g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE;
#if defined(BBSHD)
    g_config.max_current_amps = 30;
#elif defined(TSDZ2)
    g_config.max_current_amps = 20;
#endif
    g_config.max_speed_kph = 100;
    g_config.assist_levels[0][1].max_speed_percent = 20;
    g_config.assist_levels[0][1].flags |= ASSIST_FLAG_PAS_EFFICIENCY;
    g_config.assist_levels[0][1].torque_amplification_factor_x10 = 50;
    pedal_cadence = 600;
    start();
}

static uint8_t sample_lvc(bool recover, unsigned elapsed)
{
    now += elapsed;
    requested_current = 100;
    apply_low_voltage_limit(recover);
    return requested_current;
}

static uint8_t establish_sag(void)
{
    voltage = 440;
    uint8_t cap = 100;
    for (unsigned i = 0; i < 150; ++i) cap = sample_lvc(true, 130);
    assert(cap > 20 && cap < 90);
    return cap;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    configure();
    if (!strcmp(argv[1], "speed_curve"))
    {
        uint16_t limit = assist_level_data.max_wheel_speed_rpm_x10;
        uint16_t width = speed_limit_ramp_interval_rpm_x10;
        assert(limit == global_speed_limit_rpm * 2);
        for (unsigned demand = 1; demand <= 100; ++demand)
        {
            uint8_t previous = demand;
            for (wheel_speed = 0; wheel_speed <= limit + 1; ++wheel_speed)
            {
                uint8_t value = demand;
                test_speed_limit(&value, 0, true, false);
                assert(value <= previous && value <= demand);
                if (wheel_speed <= limit - width) assert(value == demand);
                if (wheel_speed >= limit) assert(value == 0);
                previous = value;
            }
        }
        wheel_speed = limit - width / 3;
        uint8_t shaped = 100;
        test_speed_limit(&shaped, 0, true, false);
        assert(shaped >= 20 && shaped <= 23); /* Eased final third, not a second 97% trim. */
        wheel_speed = limit - width * 2 / 3;
        shaped = 100; test_speed_limit(&shaped, 0, true, false);
        assert(shaped >= 76 && shaped <= 79);
        /* The same normalized policy also bounds unusually low speed settings. */
        assist_level_data.max_wheel_speed_rpm_x10 = 10;
        for (wheel_speed = 0; wheel_speed <= 10; ++wheel_speed)
        {
            shaped = 100; test_speed_limit(&shaped, 0, true, false);
            assert(shaped <= 100);
            if (wheel_speed <= 5) assert(shaped == 100);
            if (wheel_speed == 10) assert(shaped == 0);
        }
    }
    else if (!strcmp(argv[1], "speed_recovery"))
    {
        g_config.current_ramp_amps_s = 10;
        g_config.current_ramp_smoothness_percent = 100;
        uint16_t limit = assist_level_data.max_wheel_speed_rpm_x10;
        wheel_speed = limit - speed_limit_ramp_interval_rpm_x10 - 1;
        run(6000); uint8_t full = motor_current;
        assert(full == adaptive_pas_current());
        wheel_speed = limit * 98u / 100u;
        run(5000);
        assert(motor_current < full && assist_level_data.pas.efficiency.scale == 100);
        /* Ordinary speed recovery rises through the existing Q7 tracker. */
        wheel_speed = 0;
        uint8_t previous = motor_current;
        run(5); assert(motor_current <= previous + 1);
        run(6000); assert(motor_current == full);
        wheel_speed = limit; run(5); assert(motor_current == 0);
        wheel_speed = limit + 1; run(5); assert(motor_current == 0);
        wheel_speed = 0; run(6000); assert(motor_current == full);
        pedaling = false; run(5); assert(motor_current == 0);
    }
    else if (!strcmp(argv[1], "synthetic_ride"))
    {
        /* A deliberately simple road-response model, not calibrated motor data
         * or a claim about rider watts, physical efficiency or real hill grade. */
        g_config.current_ramp_amps_s = 10;
        g_config.current_ramp_smoothness_percent = 100;
        double speed = 0;
        for (unsigned phase = 0; phase < 3; ++phase)
        {
            double minimum = 100, maximum = 0;
            for (unsigned tick = 0; tick < 8000; ++tick)
            {
                wheel_speed = speed * assist_level_data.max_wheel_speed_rpm_x10 / 20.0;
                run(5);
                double drag = 0.6 + 0.01 * speed * speed + (phase == 1 ? 2.0 : 0.0);
                speed += (0.2 * motor_current - drag) * 0.005;
                if (speed < 0) speed = 0;
                assert(speed < 20.05); /* No self-generated crossing of the hard limit. */
                if (tick >= 6000)
                {
                    if (speed < minimum) minimum = speed;
                    if (speed > maximum) maximum = speed;
                }
            }
            assert(minimum > 16.0 && maximum - minimum < 0.3);
            printf("Synthetic phase %u: settled speed %.3f..%.3f km/h\n", phase, minimum, maximum);
        }
    }
    else if (!strcmp(argv[1], "throttle"))
    {
        g_config.current_ramp_amps_s = 10;
        g_config.current_ramp_smoothness_percent = 100;
        run(1000); assert(motor_current < 100);
        throttle = 100; run(5); assert(motor_current == 100);
        /* Independent throttle retains its original linear speed envelope. */
        uint16_t limit = assist_level_data.max_wheel_speed_rpm_x10;
        uint16_t width = speed_limit_ramp_interval_rpm_x10;
        for (wheel_speed = limit - width; wheel_speed <= limit; ++wheel_speed)
        {
            uint8_t value = 100;
            test_speed_limit(&value, 100, true, true);
            uint8_t expected = wheel_speed == limit ? 0 :
                (uint8_t)MAP32(wheel_speed, limit - width, limit, 100, 0);
            assert(value == expected);
        }
        wheel_speed = 0; run(5); assert(motor_current == 100);
        brake = true; run(5); assert(motor_current == 0);
        brake = false; temperature = 8501; run(5); assert(motor_current == 0);
        temperature = 0; voltage = g_config.low_cut_off_v * 10u;
        run(5); assert(motor_current == 0);
    }
    else if (!strcmp(argv[1], "lvc_recovery"))
    {
        uint8_t cap = establish_sag();
        voltage = 480;
        for (unsigned i = 0; i < 7; ++i) assert(sample_lvc(true, 130) == cap);
        uint8_t first = sample_lvc(true, 130);
        assert(first > cap && first < 100);
        for (unsigned i = 0; i < 100; ++i)
        {
            uint8_t next = sample_lvc(true, 130);
            assert(next >= first); first = next;
        }
        assert(first == 100);
        voltage = g_config.low_cut_off_v * 10u;
        assert(sample_lvc(true, 5) == 0); /* Recovery never masks hard cutoff. */
        voltage = 0; assert(sample_lvc(true, 5) == 0);
        voltage = 1001; assert(sample_lvc(true, 5) == 0);
    }
    else if (!strcmp(argv[1], "lvc_hysteresis"))
    {
        uint8_t cap = establish_sag();
        for (unsigned i = 0; i < 100; ++i)
        {
            voltage = i % 2 ? 443 : 480;
            assert(sample_lvc(true, 130) == cap); /* Small rebound breaks dwell. */
        }
        voltage = 480;
        for (unsigned i = 0; i < 7; ++i) assert(sample_lvc(true, 130) == cap);
        assert(sample_lvc(true, 500) == cap); /* Missed sample breaks dwell. */
        for (unsigned i = 0; i < 6; ++i) assert(sample_lvc(true, 130) == cap);
        assert(sample_lvc(false, 5) == cap); /* Throttle/mode change breaks dwell. */
        for (unsigned i = 0; i < 7; ++i) assert(sample_lvc(true, 130) == cap);
        voltage = 0; assert(sample_lvc(true, 5) == 0); /* Invalid input breaks dwell. */
        voltage = 480;
        for (unsigned i = 0; i < 7; ++i) assert(sample_lvc(true, 130) == cap);
        assert(sample_lvc(true, 130) > cap);
    }
    else if (!strncmp(argv[1], "lvc_scope_", 10))
    {
        uint8_t cap = establish_sag();
        if (!strcmp(argv[1], "lvc_scope_legacy"))
            g_config.assist_levels[0][1].flags = ASSIST_FLAG_PAS;
        else if (!strcmp(argv[1], "lvc_scope_adaptive"))
            g_config.assist_levels[0][1].flags &= ~ASSIST_FLAG_PAS_VARIABLE;
        else if (!strcmp(argv[1], "lvc_scope_throttle"))
            throttle = 100;
        else assert(!strcmp(argv[1], "lvc_scope_efficiency"));
        reload_assist_params();
        voltage = 480; run(12000);
        uint8_t recovered = sample_lvc(false, 5);
        if (!strcmp(argv[1], "lvc_scope_efficiency")) assert(recovered == 100);
        else assert(recovered == cap); /* Legacy, Adaptive Power and throttle stay downward-only. */
    }
    else if (!strcmp(argv[1], "lvc_legacy"))
    {
        int32_t filtered = 10000;
        uint32_t last_sample = 0;
        for (unsigned i = 0; i < 5000; ++i)
        {
            if (i == 2500) now = UINT32_MAX - 150;
            now += i % 3 ? 130 : 5;
            voltage = i < 3000 ? 480 : i % 2 ? 440 : 460;
            if (i % 109 == 0) voltage = 0;
            uint8_t expected = 100;
            if (voltage <= g_config.low_cut_off_v * 10u || voltage > 1000) expected = 0;
            else
            {
                if ((uint32_t)(now - last_sample) > 125u)
                {
                    last_sample = now;
                    if ((int32_t)voltage * 10 < filtered)
                        filtered = EXPONENTIAL_FILTER(filtered, (int32_t)voltage * 10, 8);
                }
                if (filtered <= lvc_ramp_down_start_voltage_x100)
                {
                    int32_t bounded = MAX(filtered, lvc_voltage_x100);
                    expected = CLAMP(MAP32(bounded, lvc_ramp_down_end_voltage_x100,
                        lvc_ramp_down_start_voltage_x100, LVC_LOW_CURRENT_PERCENT, 100), 0, 100);
                }
            }
            assert(sample_lvc(false, 0) == expected);
        }
    }
    else if (!strcmp(argv[1], "jitter"))
    {
        g_config.current_ramp_amps_s = 100;
        uint8_t value = 20;
        test_ramp_up(&value, false);
        now = 5; value = 20; test_ramp_up(&value, true);
        now = 10; value = 21; test_ramp_up(&value, true);
        now = 15; value = 20; test_ramp_up(&value, true);
        now = 20; value = 100; test_ramp_up(&value, true);
        unsigned bound = (200u + g_config.max_current_amps - 1u) / g_config.max_current_amps;
        assert(value > 20 && value <= 20 + bound); /* Equality jitter cannot postpone the scheduled step. */
        value = 0; test_ramp_up(&value, true); assert(value == 0);
        for (unsigned smooth = 0; smooth <= 100; ++smooth)
        {
            g_config.current_ramp_smoothness_percent = smooth;
            now = UINT32_MAX - 31; value = 0; test_ramp_up(&value, false);
            uint8_t previous = 0;
            for (unsigned tick = 0; tick < 2000; ++tick)
            {
                now += 5;
                uint8_t target = tick < 1000 ? 30 + tick % 2 : 80;
                value = target; test_ramp_up(&value, true);
                assert(value <= target && value <= previous + bound + 1);
                previous = value;
            }
            assert(value == 80);
            value = 5; test_ramp_up(&value, true); assert(value == 5);
            value = 0; test_ramp_up(&value, true); assert(value == 0);
        }
    }
    else assert(0);
    printf("efficiency_refinement_%s: PASS\n", argv[1]);
    return 0;
}
