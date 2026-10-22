/* Read the production EEPROM format and exercise the real v5-to-v6 migration. */
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define __SDCC 1
#include "../src/firmware/cfgstore.c"
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
    memset(&g_config, 0, sizeof g_config);
    g_config.max_current_amps = 25;
    g_config.current_ramp_amps_s = 12;
    g_config.assist_levels[1][4].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_VOLTAGE_COMPENSATION;
    g_config.assist_levels[1][4].target_current_percent = 65;
    pages[0][0] = 5; pages[0][1] = 154; pages[0][2] = 0;
    memcpy(pages[0] + 3, &g_config, 154);
    for (unsigned i = 3; i < 157; ++i) pages[0][2] += pages[0][i];
    memset(&g_config, 255, sizeof g_config);
    cfgstore_init();
    assert(g_config.max_current_amps == 25 && g_config.current_ramp_amps_s == 12);
    assert(g_config.use_voltage_compensation == 1 && g_config.current_ramp_smoothness_percent == 0);
    assert(g_config.assist_levels[1][4].flags == ASSIST_FLAG_PAS && g_config.assist_levels[1][4].target_current_percent == 65);
    assert(pages[0][0] == 7 && pages[0][1] == 156);
    g_config.current_ramp_smoothness_percent = 85;
    assert(cfgstore_save_config());
    memset(&g_config, 0, sizeof g_config);
    cfgstore_init(); assert(g_config.use_voltage_compensation == 1 && g_config.current_ramp_smoothness_percent == 85);
    /* v6 preserves every setting, but its retired bit cannot activate Adaptive Power. */
    pages[0][0] = 6;
    pages[0][3 + 34] |= ASSIST_FLAG_VOLTAGE_COMPENSATION;
    pages[0][2] = 0;
    for (unsigned i = 3; i < 159; ++i) pages[0][2] += pages[0][i];
    cfgstore_init();
    assert(pages[0][0] == 7 && g_config.use_voltage_compensation == 1 && g_config.current_ramp_smoothness_percent == 85);
    assert(!(g_config.assist_levels[0][0].flags & ASSIST_FLAG_PAS_ADAPTIVE));
    g_config.assist_levels[1][4].flags = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_ADAPTIVE | ASSIST_FLAG_THROTTLE;
    g_config.assist_levels[1][4].torque_amplification_factor_x10 = 25;
    assert(cfgstore_save_config());
    memset(&g_config, 0, sizeof g_config);
    cfgstore_init();
    assert(g_config.assist_levels[1][4].flags == (ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_ADAPTIVE | ASSIST_FLAG_THROTTLE));
    assert(g_config.assist_levels[1][4].torque_amplification_factor_x10 == 25);
    pages[0][2] ^= 1;
    cfgstore_init(); assert(g_config.use_voltage_compensation == 0 && g_config.current_ramp_smoothness_percent == 0);
    puts("EEPROM v5/v6 migration, adaptive v7 persistence, corrupt-payload defaults: PASS");
    return 0;
}
