/* Exercise shipped control decisions with mocked sensors; no production recorder. */
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main

static void efficiency_setup(uint8_t budget_steps, uint8_t amps)
{
    setup();
    g_config.max_current_amps = amps;
    g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE;
    g_config.use_speed_sensor = true;
    g_config.assist_levels[0][1].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY | ASSIST_FLAG_THROTTLE;
    g_config.assist_levels[0][1].torque_amplification_factor_x10 = budget_steps;
    pedal_cadence = EFFICIENCY_LOW_CADENCE_X10 - 10;
    start();
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    bool realistic_budget = !strncmp(argv[1], "real_budget_", 12);
    bool budget_fraction = !strcmp(argv[1], "budget_fraction");
    bool clipped_budget = !strcmp(argv[1], "clipped_budget");
    uint8_t budget_steps = budget_fraction ? 25 : !strcmp(argv[1], "real_budget_150") ? 15 :
        !strcmp(argv[1], "real_budget_250") ? 25 :
        !strcmp(argv[1], "real_budget_500") ? 50 : 75;
    uint8_t amps =
#if defined(BBSHD)
        30;
#elif defined(TSDZ2)
        20;
#else
        25;
#endif
    if (clipped_budget)
    {
#if defined(BBSHD)
        budget_steps = 8;
#elif defined(TSDZ2)
        budget_steps = 5;
#else
        budget_steps = 7;
#endif
    }
    efficiency_setup((realistic_budget || budget_fraction || clipped_budget) ? budget_steps : 100,
        (realistic_budget || budget_fraction || clipped_budget) ? amps : 25);
    if (budget_fraction)
    {
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 * 4 / 5;
        pedal_cadence = 239; run(5000);
        assert(!(assist_level_data.pas.efficiency.launch & 0x80u));
        assert(assist_level_data.pas.efficiency.scale == 100);
        pedal_cadence = 240; run(2000);
        assert(assist_level_data.pas.efficiency.launch & 0x80u);
        assert(assist_level_data.pas.efficiency.scale == EFFICIENCY_MIN_SCALE);
        pedal_cadence = 180; run(1000);
        assert(assist_level_data.pas.efficiency.launch & 0x80u);
        pedal_cadence = 179; run(2000);
        assert(!(assist_level_data.pas.efficiency.launch & 0x80u));
        assert(assist_level_data.pas.efficiency.scale == 100);
    }
    else if (clipped_budget)
    {
        /* Full-charge voltage permits this budget, but at 48 V the level's
         * five-percent current ceiling clips its uncadenced request. */
        g_config.assist_levels[0][1].target_current_percent = 5;
        reload_assist_params();
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 * 4 / 5;
        pedal_cadence = 231;
        assert(adaptive_pas_current() == 2); /* 40% of the attainable level current. */
        run(5000);
        assert(assist_level_data.pas.efficiency.launch & 0x80u);
        assert(assist_level_data.pas.efficiency.scale == EFFICIENCY_MIN_SCALE);
        pedal_cadence = 150; run(2000);
        assert(!(assist_level_data.pas.efficiency.launch & 0x80u));
        assert(assist_level_data.pas.efficiency.scale == 100);
    }
    else if (realistic_budget)
    {
        /* At 80% of the speed ceiling, the separate 97% speed trim cannot mask
         * the low-cadence/load gate. These are requested commands, not measured load. */
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 * 4 / 5;
        uint8_t base = adaptive_pas_current();
        assert(voltage == 480 && assist_level_data.level.target_current_percent == 100);
        if (budget_steps == 25)
        {
            assert(base < 40); /* All three 250 W profiles fail the old level-relative gate. */
#if defined(BBS02)
            assert(base == 11); /* Exact audited 48 V / 25 A / 34 rpm example. */
#endif
        }
        run(2995); assert(assist_level_data.pas.efficiency.scale == 100);
        assert(motor_current == base);
        run(2000);
        assert(assist_level_data.pas.efficiency.launch & 0x80u);
        assert(assist_level_data.pas.efficiency.scale == EFFICIENCY_MIN_SCALE);
        assert(motor_current <= base);
        assert((unsigned)motor_current * 100u >= (unsigned)base * EFFICIENCY_MIN_SCALE);
        if (base >= 100u / (100u - EFFICIENCY_MIN_SCALE))
            assert(motor_current < base);
        else
            assert(motor_current == base); /* A sub-step trim must not exceed the profile limit. */
        printf("48 V / %u A / %u W: requested current %u%% -> %u%%, policy %u%%\n",
            amps, budget_steps * 10u, base, motor_current, assist_level_data.pas.efficiency.scale);
        pedal_cadence = 210; run(1000);
        assert(assist_level_data.pas.efficiency.launch & 0x80u); /* Between enter/release fractions. */
        pedal_cadence = 150; run(2000);
        assert(!(assist_level_data.pas.efficiency.launch & 0x80u));
        assert(assist_level_data.pas.efficiency.scale == 100); /* Quarter-budget floor releases shaping. */
        pedal_cadence = EFFICIENCY_LOW_CADENCE_X10 - 10; run(2000);
        assert(assist_level_data.pas.efficiency.scale == EFFICIENCY_MIN_SCALE);
        wheel_speed = 0; run(2000);
        assert(assist_level_data.pas.efficiency.scale == 100 && motor_current == base);
    }
    else if (!strcmp(argv[1], "load_hysteresis"))
    {
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 * 4 / 5;
        uint8_t baseline = adaptive_pas_current();
        run(2900); assert(assist_level_data.pas.efficiency.scale == 100);
        run(4000); assert(assist_level_data.pas.efficiency.scale == EFFICIENCY_MIN_SCALE);
        assert(motor_current <= baseline);
        pedal_cadence = EFFICIENCY_LOW_CADENCE_X10 + 50;
        run(1000); assert(assist_level_data.pas.efficiency.launch & 0x80);
        pedal_cadence = EFFICIENCY_LOW_CADENCE_X10 + 100;
        run(2000); assert(assist_level_data.pas.efficiency.scale == 100);
        wheel_speed = 0; pedal_cadence = EFFICIENCY_LOW_CADENCE_X10 - 10;
        run(6000); assert(assist_level_data.pas.efficiency.scale == 100); /* Hill start has no derating. */
    }
    else if (!strcmp(argv[1], "speed_target"))
    {
        /* 100 km/h global × 20% assist retains the exact existing 20 km/h ceiling. */
        g_config.max_speed_kph = 100;
        g_config.assist_levels[0][1].max_speed_percent = 20;
        app_init();
        assert(assist_level_data.max_wheel_speed_rpm_x10 == global_speed_limit_rpm * 2);
        pedal_cadence = 600;
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 * 98 / 100;
        run(5000); assert(assist_level_data.pas.efficiency.scale == 100); /* No independent speed trim. */
        assert(motor_current < adaptive_pas_current());
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 * 90 / 100;
        run(2000); assert(assist_level_data.pas.efficiency.scale == 100);
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10;
        run(5); assert(motor_current == 0);
    }
    else if (!strcmp(argv[1], "sensors"))
    {
        g_config.use_speed_sensor = false; run(5000);
        assert(assist_level_data.pas.efficiency.scale == 100 && motor_current == adaptive_pas_current());
        pedal_cadence = 0; run(5); assert(motor_current == 0);
        pedal_cadence = 600; run(1000); assert(motor_current > 0);
#if HAS_TORQUE_SENSOR
        torque_valid = false; run(5); assert(motor_current == 0);
        torque_valid = true;
#endif
        pedaling = false; run(5); assert(motor_current == 0);
        pedaling = true; backwards = true; /* Sensor API owns directional filtering. */
        pedaling = false; run(5); assert(motor_current == 0);
        g_config.use_voltage_compensation = (CONTROLLER_EFFICIENCY_PROFILE + 2u) % 6u;
        pedaling = true; run(5000); assert(motor_current == 0);
        throttle = 70; run(1000); assert(motor_current == 70); /* Explicit independent throttle. */
    }
    else if (!strcmp(argv[1], "comfort"))
    {
        g_config.use_voltage_compensation |= SYSTEM_EFFICIENCY_COMFORT;
        g_config.use_speed_sensor = false;
        pedal_cadence = 600; run(3000); uint8_t before = motor_current;
        pedal_cadence = 300; run(5);
        assert(motor_current > adaptive_pas_current() && motor_current <= before);
        assert((uint32_t)motor_current * voltage * g_config.max_current_amps <= 100u * 10000u);
        voltage = 800; run(5); /* Raw recovery must clamp comfort to the updated watt ceiling. */
        assert((uint32_t)motor_current * voltage * g_config.max_current_amps <= 100u * 10000u);
        brake = true; run(5); assert(motor_current == 0);
        brake = false; voltage = 480; pedal_cadence = 600; run(3000);
        pedaling = false; run(5); assert(motor_current == 0);
    }
    else if (!strcmp(argv[1], "clock_mode"))
    {
        now = UINT32_MAX - 1000u;
        wheel_speed = assist_level_data.max_wheel_speed_rpm_x10 * 4 / 5;
        reload_assist_params(); run(6000);
        assert(assist_level_data.pas.efficiency.scale == EFFICIENCY_MIN_SCALE);
        g_config.assist_levels[0][2] = g_config.assist_levels[0][1];
        app_set_assist_level(2);
        assert(assist_level_data.pas.efficiency.scale == 100 && assist_level_data.pas.efficiency.launch == 0);
        g_config.assist_levels[0][3] = g_config.assist_levels[0][1];
        g_config.assist_levels[0][3].flags = ASSIST_FLAG_PAS;
        app_set_assist_level(3); run(3000);
        assert(motor_current <= 100);
    }
    else if (!strcmp(argv[1], "slew"))
    {
        for (unsigned smooth = 0; smooth <= 100; ++smooth)
            for (unsigned rate = 1; rate <= 255; rate += 17)
            {
                g_config.current_ramp_smoothness_percent = smooth;
                g_config.current_ramp_amps_s = rate;
                uint8_t value = 0, previous = 0;
                now = UINT32_MAX - 405u; test_ramp_up(&value, false);
                for (unsigned step = 1; step <= 1600; ++step)
                {
                    now += 5;
                    unsigned target = step < 81 ? 20 : step < 300 ? 100 : step < 450 ? 30 : 80;
                    value = target; test_ramp_up(&value, true);
                    assert(value <= target);
                    unsigned bound = (rate * 2u + g_config.max_current_amps - 1u) / g_config.max_current_amps + 1u;
                    assert(value <= previous + bound); /* 20 ms slew, allowing whole-percent quantization. */
                    if (step > 450) assert(value >= previous);
                    previous = value;
                }
                if (rate >= 18) assert(value == 80);
            }
    }
    else assert(0);
    printf("efficiency_%s: PASS\n", argv[1]);
    return 0;
}
