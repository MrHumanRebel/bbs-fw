/* Exercise the production current response, capturing the actual UART bytes.
 * No display hardware is emulated: this tests the controller's wire output.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define __SDCC 1
#include "../src/firmware/extcom.c"

static uint16_t measured_current_x10;
static uint8_t transmitted[2];
static unsigned transmitted_count;

uint16_t motor_get_battery_current_x10(void)
{
	return measured_current_x10;
}

void uart_write(uint8_t byte)
{
	assert(transmitted_count < sizeof transmitted);
	transmitted[transmitted_count++] = byte;
}

static void request_current(uint16_t current_x10)
{
	measured_current_x10 = current_x10;
	transmitted_count = 0;
	msgbuf[0] = REQUEST_TYPE_BAFANG_READ;
	msgbuf[1] = OPCODE_BAFANG_DISPLAY_READ_CURRENT;
	msg_len = 2;
	assert(process_bafang_display_read_current() == 2);
	assert(transmitted_count == 2);
	assert(transmitted[0] == current_x10 / 5);
	assert(transmitted[1] == transmitted[0]);
}

int main(void)
{
	/* Incomplete requests must not emit a partial response. */
	for (msg_len = 0; msg_len < 2; msg_len++)
	{
		assert(process_bafang_display_read_current() == KEEP);
		assert(transmitted_count == 0);
	}

	/* Every 0.1 A input through the supported 33 A controller range. */
	for (uint16_t current_x10 = 0; current_x10 <= 330; current_x10++)
		request_current(current_x10);
	puts("331 current values (0..33 A): correct half-amp encoding and checksum");

	/* Cross 999 W at 48 V: 20.5 A -> 984 W, 21 A -> 1008 W. */
	request_current(205);
	assert(transmitted[0] == 0x29);
	request_current(210);
	assert(transmitted[0] == 0x2a);
	puts("984 W -> 1008 W at 48 V: current byte 29 -> 2A, no discontinuity");

	/* 56 V * 20 A = 1120 W: no 999 W cap in this response. */
	request_current(200);
	assert(transmitted[0] == 0x28 && transmitted[1] == 0x28);
	puts("1120 W at 56 V / 20 A: UART response 28 28");

	/* 48 V * 26 A = 1248 W, also above the reported ceiling. */
	request_current(260);
	assert(transmitted[0] == 0x34 && transmitted[1] == 0x34);
	puts("1248 W at 48 V / 26 A: UART response 34 34");

	/* Even the complete one-byte protocol range is encodable. */
	request_current(1275);
	assert(transmitted[0] == 0xff && transmitted[1] == 0xff);
	puts("127.5 A: UART response FF FF (protocol maximum)");
	return 0;
}
