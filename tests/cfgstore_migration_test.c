/* Read the production EEPROM format and exercise the real v5-to-v6 migration. */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define __SDCC 1
#include "../src/firmware/cfgstore.c"
#include "../src/firmware/config_limits.c"
static uint8_t pages[2][256];
static int selected;
bool eeprom_select_page(int page) { selected = page; return page >= 0 && page < 2; }
int eeprom_read_byte(int offset) { return pages[selected][offset]; }
bool eeprom_erase_page(void) { memset(pages[selected], 255, 256); return true; }
bool eeprom_write_byte(int offset, uint8_t value) { pages[selected][offset] = value; return true; }
bool eeprom_end_write(void) { return true; }
void eventlog_write(uint8_t event) { (void)event; }
void eventlog_write_data(uint8_t event, int16_t value) { (void)event; (void)value; }
int main(void)
{
    memset(pages, 255, sizeof pages);
    load_default_config();
    assert(g_config.use_temperature_sensor == (HAS_CONTROLLER_TEMP_SENSOR ? TEMPERATURE_SENSOR_CONTR : 0));
    g_config.use_temperature_sensor = TEMPERATURE_SENSOR_CONTR | TEMPERATURE_SENSOR_MOTOR;
    g_config.max_current_amps = MAX_CONFIG_CURRENT_AMPS;
    g_config.current_ramp_amps_s = 12;
    g_config.assist_levels[1][4].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_VOLTAGE_COMPENSATION;
    g_config.assist_levels[1][4].target_current_percent = 65;
    pages[0][0] = 5; pages[0][1] = 154; pages[0][2] = 0;
    memcpy(pages[0] + 3, &g_config, 154);
    for (unsigned i = 3; i < 157; ++i) pages[0][2] += pages[0][i];
    memset(&g_config, 255, sizeof g_config);
    cfgstore_init();
    assert(g_config.max_current_amps == MAX_CONFIG_CURRENT_AMPS && g_config.current_ramp_amps_s == 12);
    assert(g_config.use_temperature_sensor == (TEMPERATURE_SENSOR_CONTR | TEMPERATURE_SENSOR_MOTOR));
    assert(g_config.use_voltage_compensation == (1u | CONTROLLER_EFFICIENCY_PROFILE) && g_config.current_ramp_smoothness_percent == 0);
    assert(g_config.assist_levels[1][4].flags == ASSIST_FLAG_PAS && g_config.assist_levels[1][4].target_current_percent == 65);
    assert(pages[0][0] == CONFIG_VERSION && pages[0][1] == 156);
    g_config.current_ramp_smoothness_percent = 85;
    assert(cfgstore_save_config());
    memset(&g_config, 0, sizeof g_config);
    cfgstore_init(); assert(g_config.use_voltage_compensation == (1u | CONTROLLER_EFFICIENCY_PROFILE) && g_config.current_ramp_smoothness_percent == 85);
    /* v6 preserves every setting, but its retired bit cannot activate Adaptive Power. */
    pages[0][0] = 6; pages[0][3 + 154] = 1;
    pages[0][3 + 34] |= ASSIST_FLAG_VOLTAGE_COMPENSATION;
    pages[0][2] = 0;
    for (unsigned i = 3; i < 159; ++i) pages[0][2] += pages[0][i];
    cfgstore_init();
    assert(pages[0][0] == CONFIG_VERSION && g_config.use_voltage_compensation == (1u | CONTROLLER_EFFICIENCY_PROFILE) && g_config.current_ramp_smoothness_percent == 85);
    assert(!(g_config.assist_levels[0][0].flags & ASSIST_FLAG_PAS_ADAPTIVE));
    g_config.assist_levels[1][4].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_ADAPTIVE | ASSIST_FLAG_THROTTLE;
    g_config.assist_levels[1][4].torque_amplification_factor_x10 = 25;
    assert(cfgstore_save_config());
    memset(&g_config, 0, sizeof g_config);
    cfgstore_init();
    assert(g_config.assist_levels[1][4].flags == (ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY | ASSIST_FLAG_THROTTLE));
    assert(g_config.assist_levels[1][4].torque_amplification_factor_x10 == 25);
    /* v7 power budgets migrate to the sole Efficiency controller, retaining all
     * current/power settings, selecting the compiled profile and comfort OFF. */
    pages[0][3 + 34 + 14 * 6] &= ~ASSIST_FLAG_PAS_VARIABLE;
    pages[0][0] = 7; pages[0][3 + 154] = 1; pages[0][2] = 0;
    for (unsigned i = 3; i < 159; ++i) pages[0][2] += pages[0][i];
    cfgstore_init();
    assert(pages[0][0] == CONFIG_VERSION && (g_config.use_voltage_compensation & SYSTEM_EFFICIENCY_COMFORT) == 0);
    assert(g_config.assist_levels[1][4].flags == (ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY | ASSIST_FLAG_THROTTLE));
    config_t valid = g_config;
    assert(config_is_valid(&valid));
    /* A valid checksum cannot turn an invalid v7 flag combination into opt-in. */
    pages[0][0] = 7; pages[0][3 + 154] = 1;
    pages[0][3 + 34 + 14 * 6] |= ASSIST_FLAG_PAS_VARIABLE;
    pages[0][2] = 0;
    for (unsigned i = 3; i < 159; ++i) pages[0][2] += pages[0][i];
    cfgstore_init();
    for (unsigned mode = 0; mode < 2; ++mode)
        for (unsigned level = 0; level < 10; ++level)
            assert((g_config.assist_levels[mode][level].flags & ASSIST_FLAG_PAS_EFFICIENCY) != ASSIST_FLAG_PAS_EFFICIENCY);
    g_config = valid;

    for (unsigned amps = 0; amps <= 255; ++amps)
    {
        g_config = valid;
        g_config.max_current_amps = amps;
        g_config.assist_levels[1][4].torque_amplification_factor_x10 = 1;
        assert(config_is_valid(&g_config) == (amps >= 5 && amps <= MAX_CONFIG_CURRENT_AMPS));
    }
    g_config = valid;
    for (unsigned level = 0; level < 10; ++level)
        for (unsigned mode = 0; mode < 2; ++mode)
        {
            assist_level_t *item = &g_config.assist_levels[mode][level];
            *item = valid.assist_levels[1][4];
            uint8_t maximum = config_power_budget_steps(&g_config, item);
            item->torque_amplification_factor_x10 = maximum;
            assert(config_is_valid(&g_config));
            item->torque_amplification_factor_x10 = maximum + 1u;
            assert(!config_is_valid(&g_config));
            *item = valid.assist_levels[mode][level];
        }
    for (unsigned profile = 0; profile < 4; ++profile)
    {
        g_config = valid;
        g_config.use_voltage_compensation = (profile << 1) | SYSTEM_EFFICIENCY_COMFORT;
        g_config.assist_levels[1][4].flags |= ASSIST_FLAG_PAS_VARIABLE;
        assert(config_is_valid(&g_config) == ((profile << 1) == CONTROLLER_EFFICIENCY_PROFILE));
    }
    g_config = valid; g_config.use_voltage_compensation |= 0x10; assert(!config_is_valid(&g_config));
    g_config = valid;
    g_config.max_battery_x100v_u16h = (g_config.low_cut_off_v * 100u) >> 8;
    g_config.max_battery_x100v_u16l = (uint8_t)(g_config.low_cut_off_v * 100u);
    assert(!config_is_valid(&g_config));
    g_config = valid;
    g_config.assist_levels[1][4].torque_amplification_factor_x10 = 255;
    assert(write_config()); /* Simulate a valid-checksum old v7 oversized budget. */
    cfgstore_init();
    assert(g_config.assist_levels[1][4].torque_amplification_factor_x10 ==
        config_power_budget_steps(&g_config, &g_config.assist_levels[1][4]));
    assert(g_config.current_ramp_smoothness_percent == 85);
    g_config = valid;
    g_config.max_current_amps = MAX_CONFIG_CURRENT_AMPS + 1u;
    assert(write_config()); /* Simulate semantically corrupt EEPROM with a correct checksum. */
    cfgstore_init();
    assert(g_config.max_current_amps <= MAX_CONFIG_CURRENT_AMPS && config_is_valid(&g_config));
    puts("Controller current ceilings, all twenty budget ceilings, EEPROM normalization and semantic validation: PASS");
    pages[0][2] ^= 1;
    cfgstore_init(); assert(g_config.use_voltage_compensation == CONTROLLER_EFFICIENCY_PROFILE && g_config.current_ramp_smoothness_percent == 0);
    puts("EEPROM v5/v6 migration, adaptive v7 persistence, corrupt-payload defaults: PASS");
    return 0;
}
