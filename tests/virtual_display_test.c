/* Real app, battery and protocol handlers with the existing host I/O fixture. */
#define main existing_fixture_main
#include "pas_voltage_test.c"
#undef main
#include "../src/firmware/extcom.c"
#include "../src/firmware/config_limits.c"
static uint8_t tx[512];
static unsigned tx_count;
static uint8_t rx[512];
static unsigned rx_pos, rx_len, config_saves;
pstate_t g_pstate;
bool cfgstore_save_pstate(void) { return true; }
uint8_t uart_available(void) { unsigned n = rx_len - rx_pos; return n > 255 ? 255 : n; }
uint8_t uart_read(void) { assert(rx_pos < rx_len); return rx[rx_pos++]; }
bool cfgstore_save_config(void) { ++config_saves; return true; }
bool cfgstore_reset_config(void) { return false; }
void eventlog_set_enabled(bool enabled) { (void)enabled; }
int16_t motor_calibrate_battery_voltage(uint16_t voltage_x100) { (void)voltage_x100; return 0; }
void uart_write(uint8_t byte) { assert(tx_count < sizeof tx); tx[tx_count++] = byte; }

static void command(uint8_t enable, uint8_t level, uint8_t demand, bool accepted)
{
	uint8_t request[] = {2, 0xf4, 7, enable, level, demand, 0};
	memcpy(msgbuf, request, sizeof request); msgbuf[6] = compute_checksum(msgbuf, 6);
	msg_len = 7; tx_count = 0;
	assert(process_write_virtual_control() == 7);
	assert(tx_count == 5 && tx[0] == 2 && tx[1] == 0xf4 && tx[2] == 7 && tx[3] == accepted);
	assert(tx[4] == compute_checksum(tx, 4));
}

int main(void)
{
	setup(); measured_battery_current = 260; g_config.max_current_amps = MAX_CONFIG_CURRENT_AMPS; start(); pedaling = false;
	command(1, 1, 50, false); /* Zero first, never arm at positive throttle. */
	command(1, 1, 0, true);
	command(1, 1, 50, true); run(500); assert(motor_current == 50);
	command(1, 10, 100, false); assert(app_virtual_throttle() == 50);
	command(1, 1, 101, false); assert(app_virtual_throttle() == 50);
	msgbuf[6] ^= 1; assert(process_write_virtual_control() == DISCARD);
	assert(app_virtual_throttle() == 50);
	msg_len = 6; assert(process_write_virtual_control() == KEEP);
	assert(app_virtual_throttle() == 50);
	puts("arm, valid/invalid commands, checksum, partial frames: PASS");
	pedaling = true; command(1, 1, 0, true); run(5);
	assert(motor_current == 0); /* Motor-driven pedal pulses cannot take over. */
	pedaling = false;
	puts("throttle release stops current and inhibits stale PAS: PASS");
	command(0, 0, 0, true); run(5); assert(motor_current == 0 && app_get_assist_level() == 0);
	run(1100); command(1, 1, 0, true); command(1, 1, 80, true);
	run(1495); assert(app_virtual_throttle() == 80);
	run(5); assert(app_virtual_throttle() == 0 && motor_current == 0 && app_get_assist_level() == 0);
	command(1, 1, 80, false);
	puts("stop and autonomous 1500 ms lease expiry: PASS");
	command(1, 1, 0, true); command(1, 1, 50, true);
	now += 1500; command(1, 1, 50, false); /* Before app_process runs. */
	assert(app_virtual_throttle() == 0);
	now = UINT32_MAX - 500; command(1, 1, 0, true); command(1, 1, 50, true);
	run(1500); assert(app_virtual_throttle() == 0 && motor_current == 0);
	puts("late command cannot renew expired lease; clock wrap: PASS");
	command(1, 1, 0, true); command(1, 1, 100, true);
	brake = true; run(500); assert(motor_current == 0); brake = false;
	g_config.use_speed_sensor = true; wheel_speed = 65535;
	run(100); assert(motor_current == 0);
	puts("physical brake and speed limit remain effective: PASS");
	voltage = 480; wheel_speed = 1000;
	msgbuf[0] = 1; msgbuf[1] = 4; msgbuf[2] = 5; msg_len = 3; tx_count = 0;
	assert(process_read_status() == 3 && tx_count == 15);
	assert(tx[2] == 1 && tx[3] == 1 && tx[4] == 224); /* 48.0 V */
	assert(tx[5] == 1 && tx[6] == 4); /* 26.0 A -> 1248 W */
	assert(tx[7] == 3 && tx[8] == 232); /* 100.0 wheel RPM */
	assert(tx[9] == 1 && tx[10] == 24); /* 28.0 inch wheel */
	assert(tx[14] == compute_checksum(tx, 14));
	msgbuf[2] ^= 1; tx_count = 0;
	assert(process_read_status() == DISCARD && tx_count == 0);
	puts("telemetry layout, big-endian fields and checksum: PASS");
	/* Coalesced display writes and a two-byte read keep their boundaries. */
	const uint8_t display_frames[] = {0x16,0x0b,0x01,0x22, 0x11,0x0a,
		0x16,0x0b,0x0b,0x2c, 0x16,0x0c,0x04,0x26};
	memcpy(rx, display_frames, sizeof display_frames);
	rx_pos = 0; rx_len = sizeof display_frames; msg_len = 0; discard_until_ms = 0; tx_count = 0;
	extcom_process();
	assert(msg_len == 0 && app_get_assist_level() == 2 && operation_mode == OPERATION_MODE_SPORT);
	assert(tx_count == 2 && tx[0] == 52 && tx[1] == 52);
	puts("coalesced Bafang PAS/mode writes and current read: PASS");
	assert(sizeof(config_t) == 156 && sizeof(msgbuf) == 161);
	/* A maximum configuration write followed immediately by telemetry. */
	rx[0] = 2; rx[1] = 0xf1; rx[2] = CONFIG_VERSION; rx[3] = sizeof(config_t);
	memcpy(rx + 4, &g_config, sizeof g_config);
	rx[5] = MAX_CONFIG_CURRENT_AMPS; /* Max Current, second byte of the configuration payload. */
	rx[160] = compute_checksum(rx, 160);
	rx[161] = 1; rx[162] = 4; rx[163] = 5;
	rx_pos = 0; rx_len = 164; msg_len = 0; discard_until_ms = 0; tx_count = 0;
	extcom_process();
	assert(config_saves == 1 && g_config.max_current_amps == MAX_CONFIG_CURRENT_AMPS && tx_count == 19);
	assert(tx[0] == 2 && tx[1] == 0xf1 && tx[2] == 1);
	assert(tx[4] == 1 && tx[5] == 4);
	/* The same maximum request, one byte per application call. */
	rx_pos = 0; rx_len = 0; msg_len = 0; tx_count = 0;
	for (unsigned i = 0; i < 161; i++) { rx_len = i + 1; extcom_process(); }
	assert(config_saves == 2 && tx_count == 4 && msg_len == 0);
	rx[5] = 28; /* Corrupt payload without updating its checksum. */
	rx_pos = 0; rx_len = 161; tx_count = 0; extcom_process();
	assert(config_saves == 2 && g_config.max_current_amps == MAX_CONFIG_CURRENT_AMPS && tx_count == 0);
	puts("161-byte config buffer, pipelining, fragmentation, corrupt write rejection: PASS");
    now += DISCARD_TIMEOUT_MS + 1u;
    memcpy(rx + 4, &g_config, sizeof g_config);
    rx[2] = CONFIG_VERSION;
    rx[4 + 34 + 6] = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY;
    rx[4 + 154] = CONTROLLER_EFFICIENCY_PROFILE;
    rx[4 + 34 + 6 + 5] = 25;
    rx[160] = compute_checksum(rx, 160);
    rx_pos = 0; rx_len = 161; tx_count = 0; extcom_process();
    assert(config_saves == 3 && tx_count == 4 && tx[2] == 1);
    assert(g_config.assist_levels[0][1].torque_amplification_factor_x10 == 25);
    now += DISCARD_TIMEOUT_MS + 1u;
    rx[4 + 34 + 6] = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY;
    rx[4 + 154] = CONTROLLER_EFFICIENCY_PROFILE | SYSTEM_EFFICIENCY_COMFORT;
    rx[160] = compute_checksum(rx, 160);
    rx_pos = 0; rx_len = 161; tx_count = 0; extcom_process();
    assert(config_saves == 4 && tx_count == 4 && tx[2] == 1);
    assert(g_config.assist_levels[0][1].flags == (ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY));
    for (unsigned invalid = 0; invalid < 4; ++invalid)
    {
        rx[2] = invalid == 2 ? 6 : CONFIG_VERSION;
        rx[4 + 34 + 6] = ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_ADAPTIVE | (invalid == 1 ? ASSIST_FLAG_PAS_TORQUE : 0);
        rx[4 + 34 + 6 + 5] = invalid == 0 ? 0 : 25;
        rx[160] = compute_checksum(rx, 160);
        rx_pos = 0; rx_len = 161; tx_count = 0; extcom_process();
        assert(config_saves == 4 && tx_count == 4 && tx[2] == 0);
        assert(g_config.assist_levels[0][1].flags == (ASSIST_FLAG_PAS | ASSIST_FLAG_PAS_EFFICIENCY));
        assert(g_config.assist_levels[0][1].torque_amplification_factor_x10 == 25);
    }
    puts("Sole Efficiency writes, retired encoding, invalid budget/variant and old-version write rejection: PASS");
    for (unsigned invalid = 0; invalid < 3; ++invalid)
    {
        memcpy(rx + 4, &g_config, sizeof g_config);
        rx[2] = CONFIG_VERSION;
        if (invalid == 0) rx[5] = MAX_CONFIG_CURRENT_AMPS + 1u;
        if (invalid == 1) rx[4 + 34 + 6 + 5] = 255;
        if (invalid == 2) rx[4 + 34 + 6 + 1] = 101;
        rx[160] = compute_checksum(rx, 160);
        rx_pos = 0; rx_len = 161; tx_count = 0; extcom_process();
        assert(config_saves == 4 && tx_count == 4 && tx[2] == 0);
        assert(g_config.max_current_amps == MAX_CONFIG_CURRENT_AMPS);
        assert(g_config.assist_levels[0][1].torque_amplification_factor_x10 == 25);
    }
    throttle = 100; pedaling = true; run(1000); assert(motor_current == 0);
    puts("Unsafe UART settings rejected; configuration write stops and locks motor until restart: PASS");
	return 0;
}
