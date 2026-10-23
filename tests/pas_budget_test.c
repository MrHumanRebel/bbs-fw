/* Exercise the production adaptive PAS and its existing safety pipeline with mocked I/O. */
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main

/* Frozen v6 rising ramp: existing variants must remain unchanged. */
static void reference_ramp_up(uint8_t* target_current, bool enable) RAM_REENTRANT
{
	static uint8_t curve_start_current;
	static uint8_t curve_target_current;
	static uint8_t curve_output;
	static uint32_t curve_start_ms;
	uint32_t now = system_ms();
	if (!enable || *target_current <= curve_output)
	{
		curve_output = *target_current;
		curve_target_current = *target_current;
		curve_start_ms = now;
	}
	else
	{
		if (*target_current != curve_target_current)
		{
			curve_start_current = curve_output;
			curve_target_current = *target_current;
			curve_start_ms = now;
		}
		uint8_t span = curve_target_current - curve_start_current;
		uint32_t duration_ms = ((uint32_t)span * g_config.max_current_amps * 10u) /
			(MAX(g_config.current_ramp_amps_s, 1u));
		// offset for time overshoot to not accumulate large ramp error
		// Elapsed time also avoids the old one-percent-per-application-tick ceiling.
		uint32_t elapsed = now - curve_start_ms;
		uint16_t progress = duration_ms == 0 || elapsed >= duration_ms ? 1000u :
			(uint16_t)((elapsed * 1000ul) / duration_ms);
		curve_output = curve_start_current + (uint8_t)(((uint32_t)span *
			current_ramp_curve(progress, g_config.current_ramp_smoothness_percent)) / 1000u);
		*target_current = MIN(*target_current, curve_output);
	}
}

static void adaptive_setup(void)
{
    setup();
    g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE;
    g_config.assist_levels[0][1].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY | ASSIST_FLAG_THROTTLE;
    g_config.assist_levels[0][1].torque_amplification_factor_x10 = 25;
    pedal_cadence = 600;
    start();
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    adaptive_setup();
    if (!strcmp(argv[1], "budget"))
    {
        assert(sizeof(config_t) == 156 && sizeof(assist_level_t) == 6);
        assert(adaptive_pas_current() == 20); /* 250 W / (48 V * 25 A), floored to 1%. */
        pedal_cadence = 300; assert(adaptive_pas_current() == 10);
        pedal_cadence = 0; assert(adaptive_pas_current() == 5);
        for (unsigned budget = 0; budget <= 255; ++budget)
            for (unsigned cadence = 0; cadence <= 1500; cadence += 15)
                for (unsigned volts = 421; volts <= 1000; volts += 19)
                {
                    assist_level_data.level.torque_amplification_factor_x10 = budget;
                    assist_level_data.level.target_current_percent = (budget * 7) % 101;
                    pedal_cadence = cadence; voltage = volts;
                    uint8_t value = adaptive_pas_current();
                    uint32_t power_x10 = budget * (uint32_t)(cadence < 150 ? 150 : cadence > 600 ? 600 : cadence) / 6;
                    assert(value <= assist_level_data.level.target_current_percent && value <= 100);
                    assert((uint32_t)value * voltage * g_config.max_current_amps <= power_x10 * 100);
                }
    }
    else if (!strcmp(argv[1], "voltage"))
    {
        filtered_voltage_x1000 = 58800;
        assert(adaptive_pas_current() == 17); /* Sag does not immediately raise current. */
        voltage = 600; assert(adaptive_pas_current() == 16); /* Recovery uses the higher raw voltage. */
        voltage = 0; assert(adaptive_pas_current() == 0);
        voltage = 1001; assert(adaptive_pas_current() == 0);
        voltage = 420; assert(adaptive_pas_current() == 0);
        voltage = 480; g_config.max_current_amps = 0; assert(adaptive_pas_current() == 0);
        g_config.max_current_amps = 25; g_config.low_cut_off_v = 0; assert(adaptive_pas_current() == 0);
    }
    else if (!strcmp(argv[1], "coast"))
    {
        g_config.use_pretension = true;
        run(2000); assert(motor_current == 20);
        pedaling = false; run(5); assert(motor_current == 0);
        pedaling = true; run(500); assert(motor_current == 20);
        backwards = true; pedaling = false; run(5); assert(motor_current == 0);
    }
    else if (!strcmp(argv[1], "compensation"))
    {
        run(2000); assert(motor_current == 20);
        g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE;
        run(500); assert(motor_current == 20);
        g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE | SYSTEM_VOLTAGE_COMPENSATION;
        run(500); assert(motor_current == 20);
    }
    else if (!strcmp(argv[1], "throttle"))
    {
        throttle = 80; run(2000); assert(motor_current == 80);
        throttle = 0; run(5); assert(motor_current <= 20);
        run(1000); assert(motor_current == 20);
        voltage = 0; run(5); assert(motor_current == 0);
    }
    else if (!strcmp(argv[1], "profiles"))
    {
        for (uint8_t mode = 0; mode < 2; ++mode)
            for (uint8_t level = 0; level < 10; ++level)
            {
                g_config.assist_levels[mode][level].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY;
                g_config.assist_levels[mode][level].target_current_percent = 100;
                g_config.assist_levels[mode][level].torque_amplification_factor_x10 = 10 + level * 2;
                app_set_operation_mode(mode); app_set_assist_level(level);
                assert(adaptive_pas_current() == (10u + level * 2u) * 1000u / 1200u);
            }
    }
    else if (!strcmp(argv[1], "legacy_ramp"))
    {
        assist_level_data.level.flags = ASSIST_FLAG_PAS;
        uint32_t random = 1234567;
        for (unsigned shape = 0; shape <= 100; ++shape)
            for (unsigned rate = 1; rate <= 255; rate += 17)
            {
                g_config.current_ramp_smoothness_percent = shape;
                g_config.current_ramp_amps_s = rate;
                g_config.max_current_amps = 5 + shape % 26;
                now = UINT32_MAX - 500u;
                for (unsigned step = 0; step < 256; ++step)
                {
                    random = random * 1664525u + 1013904223u;
                    uint8_t actual = (random >> 24) % 101, expected = actual;
                    bool enabled = (random & 7u) != 0;
                    now += 5 + (random & 127u);
                    test_ramp_up(&actual, enabled);
                    reference_ramp_up(&expected, enabled);
                    assert(actual == expected);
                }
            }
    }
    else if (!strcmp(argv[1], "retarget_plateau"))
    {
        g_config.current_ramp_amps_s = 10;
        g_config.current_ramp_smoothness_percent = 100;
        uint8_t value = 0;
        now = 0; test_ramp_up(&value, false);
        for (now = 5; now <= 400; now += 5) { value = 20; test_ramp_up(&value, true); }
        uint8_t previous = value;
        for (now = 405; now <= 455; now += 5) { value = 100; test_ramp_up(&value, true); }
        assert(value > previous); /* No approximately 250 ms freeze after a larger target. */
    }
    else if (!strcmp(argv[1], "moving_ramp"))
    {
        g_config.current_ramp_amps_s = 10;
        g_config.current_ramp_smoothness_percent = 100;
        uint8_t value = 0, previous = 0;
        now = UINT32_MAX - 400u;
        test_ramp_up(&value, false);
        for (unsigned step = 1; step <= 100; ++step)
        {
            now += 5; value = step;
            test_ramp_up(&value, true);
            assert(value >= previous && value <= step);
            previous = value;
        }
        assert(value >= 9); /* A changing target cannot restart the curve forever. */
        now += 2500; value = 100; test_ramp_up(&value, true);
        assert(value <= previous + 2); /* A scheduling pause cannot become an upward spike. */
        for (unsigned step = 0; step < 700; ++step) { now += 5; value = 100; test_ramp_up(&value, true); }
        assert(value == 100);
        value = 0; test_ramp_up(&value, true); assert(value == 0);
        for (unsigned step = 0; step < 200; ++step)
        {
            now += 5; value = 20 + (step & 1u);
            test_ramp_up(&value, true);
            assert(value <= 20 + (step & 1u));
        }
        assert(value >= 19); /* Alternating targets above output must not restart the curve either. */
        value = 0; test_ramp_up(&value, false);
        now += 5; value = 100; test_ramp_up(&value, true);
        now += 2500; value = 20; test_ramp_up(&value, true); assert(value <= 2);
        for (unsigned step = 0; step < 300; ++step) { now += 5; value = 20; test_ramp_up(&value, true); }
        assert(value == 20);
        motor_current = 20;
        g_config.assist_levels[0][2] = g_config.assist_levels[0][1];
        app_set_assist_level(2);
        now += 5; value = 100; test_ramp_up(&value, true);
        assert(value <= 21); /* A level change cannot expose a hidden, already-finished envelope. */
    }
    else if (!strcmp(argv[1], "walk"))
    {
        g_config.use_push_walk = true;
        app_set_assist_level(ASSIST_PUSH); run(2000); assert(motor_current == 10);
        app_set_assist_level(ASSIST_1); run(2000); assert(motor_current == 0);
        pedaling = false; run(5); pedaling = true; run(2000); assert(motor_current == 20);
    }
    else
    {
        g_config.assist_levels[0][1].torque_amplification_factor_x10 = 100;
        reload_assist_params(); run(2000); assert(motor_current == 83);
        if (!strcmp(argv[1], "brake"))
        {
            brake = true; run(5); assert(motor_current == 0);
        }
        else if (!strcmp(argv[1], "speed"))
        {
            g_config.use_speed_sensor = true; wheel_speed = UINT16_MAX;
            run(5); assert(motor_current == 0);
        }
        else if (!strcmp(argv[1], "thermal"))
        {
            temperature = 10000; run(5); assert(motor_current <= MAX_TEMPERATURE_LOW_CURRENT_PERCENT);
        }
        else if (!strcmp(argv[1], "lvc"))
        {
            voltage = 430; run(30000); assert(motor_current <= LVC_LOW_CURRENT_PERCENT);
            voltage = 420; run(5); assert(motor_current == 0);
        }
#if HAS_SHIFT_SENSOR_SUPPORT
        else if (!strcmp(argv[1], "shift"))
        {
            g_config.use_shift_sensor = true;
            g_config.shift_interrupt_duration_ms_u16l = 200;
            shift = true; run(5); assert(motor_current == 0);
        }
#endif
        else assert(!"Unknown adaptive scenario");
    }
    printf("adaptive_%s: PASS\n", argv[1]);
    return 0;
}
