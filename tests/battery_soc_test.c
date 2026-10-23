/* SOC tests inspect only the existing voltage-based display estimate. */
#ifdef TEST_SW102_MAP
#include "../src/firmware/fwconfig.h"
#undef BATTERY_PERCENT_MAP
#define BATTERY_PERCENT_MAP BATTERY_PERCENT_MAP_SW102
#endif
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main

static void tick(unsigned duration)
{
    for (unsigned ms = 0; ms < duration; ms += 5)
    {
        now += 5;
        battery_process();
        assert(battery_get_percent() <= 100 && battery_get_mapped_percent() <= 100);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    setup(); voltage = 540; battery_init(); tick(5);
    uint8_t initial = battery_get_percent();
    if (!strcmp(argv[1], "map"))
    {
        battery_percent = SOC_LOADED | 35u;
        assert(battery_get_percent() == 35);
#if BATTERY_PERCENT_MAP == BATTERY_PERCENT_MAP_SW102
        assert(battery_get_mapped_percent() == 20);
#else
        assert(battery_get_mapped_percent() == 35);
#endif
    }
    else if (!strcmp(argv[1], "continuous"))
    {
        motor_current = 50; measured_battery_current = 100;
        voltage = 480; tick(9995); assert(battery_get_percent() == initial);
        tick(10); assert(battery_get_percent() == initial - 1);
        tick(60000); assert(battery_get_percent() < initial - 5);
        uint8_t loaded = battery_get_percent();
        voltage = 580; tick(30000); assert(battery_get_percent() <= loaded);
        motor_current = 0; measured_battery_current = 0;
        tick(1995); assert(battery_get_percent() <= loaded);
        tick(10); assert(battery_get_percent() > loaded);
    }
    else if (!strcmp(argv[1], "actual_current"))
    {
        motor_current = 0; measured_battery_current = 100;
        voltage = 480; tick(3000); assert(battery_get_percent() == initial);
        measured_battery_current = 5; tick(1995); assert(battery_get_percent() == initial);
        tick(10); assert(battery_get_percent() < initial);
    }
    else if (!strcmp(argv[1], "transient"))
    {
        motor_current = 70; measured_battery_current = 150;
        voltage = 430; tick(500); voltage = 540; tick(6000);
        assert(battery_get_percent() == initial);
        motor_current = 0; measured_battery_current = 0;
        tick(3000); assert(battery_get_percent() == initial);
    }
    else if (!strcmp(argv[1], "invalid_wrap"))
    {
        battery_init(); voltage = 1001; tick(10000);
        assert(!first_reading_done && battery_get_percent() == 70);
        voltage = 0; tick(10000); assert(!first_reading_done);
        now = UINT32_MAX - 1000u; voltage = 540; tick(5);
        motor_current = 50; voltage = 480;
        initial = battery_get_percent(); tick(10010);
        assert(battery_get_percent() == initial - 1);
        g_config.max_battery_x100v_u16h = 0; g_config.max_battery_x100v_u16l = 0;
        g_config.low_cut_off_v = 0; battery_init(); tick(10000);
        assert(!first_reading_done && battery_get_percent() == 70);
    }
    else assert(0);
    printf("Battery SOC %s: PASS\n", argv[1]);
    return 0;
}
