/* Pulse spacing, direction and timeout tests use the shipping ISR body. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sensor_fixture.h"

static void establish(void)
{
    sensors_init();
    for (unsigned i = 0; i < 5; ++i)
    {
        pedal_sample(false, false, 99);
        pedal_sample(true, false, 1);
    }
    assert(pas_is_pedaling_forwards() && pas_period_length == 99);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    establish();
    if (!strcmp(argv[1], "spacing"))
    {
        for (unsigned distance = 2; distance <= 51; ++distance)
        {
            establish();
            uint16_t count = pas_get_pulse_counter();
            pedal_sample(false, false, distance - 1);
            pedal_sample(true, true, 1); /* A fast opposite-direction edge is also ignored. */
            assert(pas_get_pulse_counter() == count && pas_period_length == 99);
            assert(pas_is_pedaling_forwards() && !pas_is_pedaling_backwards());
            /* Do not manufacture another rising edge when the rejected pulse stays high. */
            pedal_sample(true, true, 60);
            assert(pas_get_pulse_counter() == count);
        }
    }
    else if (!strcmp(argv[1], "startup"))
    {
        sensors_init();
        pedal_sample(false, false, 1); pedal_sample(true, true, 1);
        assert(pas_get_pulse_counter() == 0 && pas_get_cadence_rpm_x10() == 0);
        pedal_sample(true, true, 100);
        assert(pas_get_pulse_counter() == 0); /* Held input never becomes a delayed edge. */
    }
    else if (!strcmp(argv[1], "recovery"))
    {
        uint16_t count = pas_get_pulse_counter();
        pedal_sample(false, false, 19); pedal_sample(true, true, 1);
        pedal_sample(false, false, 79); pedal_sample(true, false, 1);
        assert(pas_period_length == 99 && pas_get_pulse_counter() == count + 1);
        assert(pas_is_pedaling_forwards());
    }
    else if (!strcmp(argv[1], "boundary"))
    {
        pedal_sample(false, false, 51);
        pedal_sample(true, false, 1);
        assert(pas_period_length == 51); /* Existing >50-tick convention retained. */
        assert(pas_get_cadence_rpm_x10() == (6000000ul / PAS_PULSES_REVOLUTION) / 51u);
    }
    else if (!strcmp(argv[1], "direction"))
    {
        pedal_sample(false, true, 99);
        pedal_sample(true, true, 1);
        assert(pas_is_pedaling_backwards() && pas_get_pulse_counter() == 0);
        pedal_sample(false, true, 99); pedal_sample(true, true, 1);
        assert(pas_get_pulse_counter() == 1);
        pedal_sample(false, false, 99); pedal_sample(true, false, 1);
        assert(pas_is_pedaling_forwards() && pas_get_pulse_counter() == 0);
    }
    else if (!strcmp(argv[1], "rejected_timeout"))
    {
        pas_set_stop_delay(5);
        /* Rejected edges must not freeze the existing stop timeout. */
        for (unsigned i = 0; i < 26; ++i)
        {
            pedal_sample(false, false, 1);
            pedal_sample(true, true, 1);
        }
        assert(!pas_is_pedaling_forwards() && !pas_is_pedaling_backwards());
        assert(pas_get_cadence_rpm_x10() == 0 && pas_get_pulse_counter() == 0);
    }
    else if (!strcmp(argv[1], "stop_restart"))
    {
        for (unsigned high = 0; high < 2; ++high)
        {
            establish();
            pedal_sample(high, false, 65535); pedal_sample(high, false, 100);
            assert(pas_period_counter == 65535 && !pas_is_pedaling_forwards());
            pedal_sample(false, false, 99); pedal_sample(true, false, 1);
            assert(!pas_is_pedaling_forwards()); /* First edge after a long stop measures no cadence. */
            pedal_sample(false, false, 99); pedal_sample(true, false, 1);
            assert(pas_is_pedaling_forwards() && pas_period_length == 99);
        }
    }
    else assert(0);
    printf("PAS sensor %s: PASS\n", argv[1]);
    return 0;
}
