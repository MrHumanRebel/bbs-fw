/* Configuration safety checks shared by EEPROM loading and UART writes. */
#include "cfgstore.h"
#include "fwconfig.h"
#include "util.h"
#include <stddef.h>

uint8_t config_power_budget_steps(const config_t RAM_XDATA* config, const assist_level_t RAM_XDATA* level) RAM_REENTRANT
{
    uint16_t voltage = EXPAND_U16(config->max_battery_x100v_u16h, config->max_battery_x100v_u16l);
    uint32_t steps = ((uint32_t)voltage * config->max_current_amps * level->target_current_percent) / 100000ul;
    return (uint8_t)(MIN(steps, 255u));
}

bool config_is_valid(const config_t RAM_XDATA* config) RAM_REENTRANT
{
    uint16_t voltage = EXPAND_U16(config->max_battery_x100v_u16h, config->max_battery_x100v_u16l);
    uint16_t wheel = EXPAND_U16(config->wheel_size_inch_x10_u16h, config->wheel_size_inch_x10_u16l);
    uint16_t throttle_start = EXPAND_U16(config->throttle_start_voltage_mv_u16h, config->throttle_start_voltage_mv_u16l);
    uint16_t throttle_end = EXPAND_U16(config->throttle_end_voltage_mv_u16h, config->throttle_end_voltage_mv_u16l);
    if ((uint16_t)(throttle_start - 200u) > 2300u || (uint16_t)(throttle_end - 2500u) > 2500u || throttle_start >= throttle_end ||
        config->speed_sensor_signals == 0 || config->speed_sensor_signals > 10u ||
        (uint8_t)(config->max_current_amps - 5u) > MAX_CONFIG_CURRENT_AMPS - 5u ||
        config->current_ramp_amps_s == 0 || config->use_voltage_compensation > 1u ||
        config->current_ramp_smoothness_percent > 100u || config->low_cut_off_v == 0 ||
        voltage > 10000u || voltage <= (uint16_t)config->low_cut_off_v * 100u + 100u ||
        (uint16_t)(wheel - 100u) > 300u || config->assist_startup_level > 9u ||
        config->pas_keep_current_percent > 100u || config->throttle_start_percent > 100u ||
        config->throttle_global_spd_lim_percent > 100u || config->shift_interrupt_current_threshold_percent > 100u)
        return false;

    const uint8_t RAM_XDATA* cursor = (const uint8_t RAM_XDATA*)config + offsetof(config_t, assist_levels);
    for (uint8_t index = 0; index < 20u; ++index, cursor += sizeof(assist_level_t))
    {
        const assist_level_t RAM_XDATA* level = (const assist_level_t RAM_XDATA*)cursor;
        if (level->target_current_percent > 100u || level->max_throttle_current_percent > 100u ||
            level->max_cadence_percent > 100u || level->max_speed_percent > 100u)
            return false;
        if ((level->flags & ASSIST_FLAG_PAS_ADAPTIVE) &&
            (!(level->flags & ASSIST_FLAG_PAS) ||
            (level->flags & (ASSIST_FLAG_PAS_TORQUE | ASSIST_FLAG_PAS_VARIABLE | ASSIST_FLAG_CRUISE)) ||
            level->torque_amplification_factor_x10 == 0 ||
            level->torque_amplification_factor_x10 > config_power_budget_steps(config, level)))
            return false;
    }
    return true;
}
