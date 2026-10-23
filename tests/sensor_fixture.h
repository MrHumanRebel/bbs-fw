/* Mock only sensor GPIO/ADC/timer interfaces; compile the production decoder. */
#include <stdbool.h>
#include <stdint.h>
#if !defined(__SDCC)
#define __SDCC 1
#define __interrupt(vector)
#endif
#include "../src/firmware/cfgstore.h"

static bool pins[6][8];
static uint16_t controller_adc = 677, motor_adc = 677;
config_t g_config;

#if defined(TSDZ2)
#define _TSDZ2_INTERRUPT_H_
#define _TSDZ2_STM_8_H_
#define _TSDZ2_PINS_H_
#define __STM8S_TIM4_H
#define PIN_PAS1 3, 7
#define PIN_PAS2 4, 0
#define PIN_SPEED_SENSOR 0, 1
#define PIN_BRAKE 2, 6
#define GET_PIN_INPUT_STATE_(port, pin) pins[port][pin]
#define GET_PIN_INPUT_STATE(...) GET_PIN_INPUT_STATE_(__VA_ARGS__)
#define SET_PIN_INPUT(...) ((void)0)
#define SET_PIN_INPUT_PULLUP(...) ((void)0)
static struct { uint8_t IER, SR1; } timer4;
#define TIM4 (&timer4)
#define TIM4_IT_UPDATE 1u
/* Native probes call the body as a function; exception entry/exit is excluded. */
#undef __interrupt
#define __interrupt(vector)
#include "../src/firmware/tsdz2/sensors.c"
void timer4_init_sensors(void) {}
void torque_sensor_init(void) {}
void torque_sensor_process(void) {}
#define SENSOR_TICK() isr_timer4_ovf()
#else
#define _STC_15_H_
static bool ET0;
#define GET_PIN_STATE_(port, pin) pins[port][pin]
#define GET_PIN_STATE(...) GET_PIN_STATE_(__VA_ARGS__)
#define SET_PIN_INPUT(...) ((void)0)
#define SET_PIN_QUASI(...) ((void)0)
#include "../src/firmware/bbsx/sensors.c"
void timer0_init_sensors(void) {}
uint16_t adc_get_temperature_contr(void) { return controller_adc; }
uint16_t adc_get_temperature_motor(void) { return motor_adc; }
void eventlog_write_data(uint8_t evt, int16_t data) { (void)evt; (void)data; }
#define SENSOR_TICK() sensors_timer0_isr()
#endif

#define PIN_VALUE_(port, pin) pins[port][pin]
#define PIN_VALUE(...) PIN_VALUE_(__VA_ARGS__)
static void pedal_sample(bool high, bool backward, uint16_t ticks)
{
    PIN_VALUE(PIN_PAS1) = high;
    PIN_VALUE(PIN_PAS2) = backward;
    while (ticks--) SENSOR_TICK();
}
