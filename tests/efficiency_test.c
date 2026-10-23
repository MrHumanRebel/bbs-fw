/* Exercise shipped control decisions with mocked sensors; no production recorder. */
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main

static void efficiency_setup(void)
{
    setup();
    g_config.use_voltage_compensation = CONTROLLER_EFFICIENCY_PROFILE;
    g_config.use_speed_sensor = true;
    g_config.assist_levels[0][1].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY | ASSIST_FLAG_THROTTLE;
    g_config.assist_levels[0][1].torque_amplification_factor_x10 = 100;
    pedal_cadence = EFFICIENCY_LOW_CADENCE_X10 - 10;
    start();
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    efficiency_setup();
    if (!strcmp(argv[1], "load_hysteresis"))
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
        run(5000); assert(assist_level_data.pas.efficiency.scale == EFFICIENCY_MIN_SCALE);
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
