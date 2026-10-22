/* Sharing the conversion must preserve the two original single-precision formulas. */
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main
int main(void)
{
    setup();
    for (unsigned wheel = 100; wheel <= 400; ++wheel)
    {
        g_config.wheel_size_inch_x10_u16h = wheel >> 8;
        g_config.wheel_size_inch_x10_u16l = wheel & 255;
        for (unsigned speed = 0; speed <= 255; ++speed)
        {
            float radius = wheel * 1.27f;
            uint16_t app_reference = (uint16_t)(25000.f / (3 * 3.14159f * radius) * speed);
            uint16_t display_reference = (uint16_t)(25000.f / (3 * 3.14159f * 1.27f * wheel) * speed);
            uint16_t actual = convert_wheel_speed_kph_to_rpm(speed);
            assert(actual == app_reference && actual == display_reference);
        }
    }
    puts("Shared wheel conversion matches both original formulas for all supported wheel sizes and inputs: PASS");
    return 0;
}
