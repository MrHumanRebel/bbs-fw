/* Exercise actual thermistor conversion and invalid raw ADC handling. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sensor_fixture.h"

int main(int argc, char **argv)
{
    assert(argc == 2);
    sensors_init();
    g_config.use_temperature_sensor = TEMPERATURE_SENSOR_CONTR | TEMPERATURE_SENSOR_MOTOR;
#if defined(TSDZ2)
    assert(temperature_contr_x100() == 0 && temperature_motor_x100() == 0);
#else
    if (!strcmp(argv[1], "startup_fault"))
    {
        g_config.use_temperature_sensor = TEMPERATURE_SENSOR_CONTR;
        controller_adc = 0; motor_adc = 1023;
        assert(temperature_contr_x100() == TEMPERATURE_SENSOR_FAULT_X100);
        assert(temperature_motor_x100() == 0); /* Optional motor sensor is not enabled. */
        controller_adc = 1023;
        assert(temperature_contr_x100() == TEMPERATURE_SENSOR_FAULT_X100);
    }
    else if (!strcmp(argv[1], "very_hot"))
    {
        controller_adc = 5;
        assert(temperature_contr_x100() > 8500); /* Lowest accepted NTC input must not wrap negative. */
    }
    else if (!strcmp(argv[1], "disabled"))
    {
        g_config.use_temperature_sensor = 0;
        controller_adc = 0; motor_adc = 1023;
        assert(temperature_contr_x100() == 0 && temperature_motor_x100() == 0);
    }
    else
    {
        assert(temperature_contr_x100() > 2400 && temperature_contr_x100() < 2600);
#if HAS_MOTOR_TEMP_SENSOR
        if (!strcmp(argv[1], "ptc"))
        {
            motor_adc = 168;
            for (unsigned i = 0; i < BBSHD_PTC_LUT_SIZE; ++i)
                assert(thermistor_ptc_bbshd_calculate_temperature(bbshd_ptc_lut[i].x) == bbshd_ptc_lut[i].y);
        }
        int16_t normal = temperature_motor_x100();
        if (!strcmp(argv[1], "ptc")) assert(normal >= 0 && normal < 100);
        else assert(normal > 2400 && normal < 2600);
#else
        assert(temperature_motor_x100() == 0); /* No invented BBS02 motor sensor. */
#endif
        const uint16_t invalid[] = {0, 1, 2, 3, 4, 1022, 1023, 65535};
        for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
        {
            controller_adc = invalid[i]; motor_adc = invalid[i];
            assert(temperature_contr_x100() == TEMPERATURE_SENSOR_FAULT_X100);
#if HAS_MOTOR_TEMP_SENSOR
            assert(temperature_motor_x100() == TEMPERATURE_SENSOR_FAULT_X100);
#else
            assert(temperature_motor_x100() == 0);
#endif
        }
        controller_adc = 677; motor_adc = !strcmp(argv[1], "ptc") ? 168 : 677;
        assert(temperature_contr_x100() != TEMPERATURE_SENSOR_FAULT_X100);
        assert(temperature_motor_x100() != TEMPERATURE_SENSOR_FAULT_X100);
    }
#endif
    printf("Temperature sensor %s: PASS\n", argv[1]);
    return 0;
}
