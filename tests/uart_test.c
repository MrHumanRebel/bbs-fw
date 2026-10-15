/* Run the real BBS UART driver and ISR code against mock registers. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define __SDCC 1
#define __data
#define _STC_15_H_
#define _BBSX_INTERRUPT_H_
#define INTERRUPT_USING(name, vector, bank) void name(void)
#define GET_PORT_NUM_(port, pin) port
#define GET_PORT_NUM(...) GET_PORT_NUM_(__VA_ARGS__)
#define SET_PIN_QUASI(...) ((void)0)
static uint8_t AUXR1, AUXR, PCON, SM1, SM0, SM2, ES, REN, P_SW2, S2CON, IE2, TR1;
static uint8_t SBUF, S2BUF, RI, TI;
#include "../src/firmware/bbsx/uart.c"
void timer1_init_uart1(uint32_t rate) { assert(rate == 1200); TR1 = 1; }
void timer2_init_uart2(uint32_t rate) { assert(rate == 4800); AUXR |= 16; }
void watchdog_yeild(void) {}

static void receive(unsigned port, uint8_t byte)
{
	if (port == 1) { SBUF = byte; RI = 1; isr_uart1(); assert(RI == 0); }
	else { S2BUF = byte; S2CON |= 1; isr_uart2(); assert(!(S2CON & 1)); }
}
static void transmit_done(unsigned port)
{
	if (port == 1) { TI = 1; isr_uart1(); assert(TI == 0); }
	else { S2CON |= 2; isr_uart2(); assert(!(S2CON & 2)); }
}

int main(void)
{
	uart_open(1200); uart_motor_open(4800);
	assert(RX1_BUFFER_SIZE == 64 && TX1_BUFFER_SIZE == 32);
	assert(RX2_BUFFER_SIZE == 16 && TX2_BUFFER_SIZE == 16);
	for (unsigned port = 1; port <= 2; port++)
	{
		unsigned rx_size = port == 1 ? RX1_BUFFER_SIZE : RX2_BUFFER_SIZE;
		unsigned tx_size = port == 1 ? TX1_BUFFER_SIZE : TX2_BUFFER_SIZE;
		/* Wrap many times, preserve FIFO ordering and reject overflow. */
		for (unsigned round = 0; round < 100; round++)
		{
			for (unsigned i = 0; i < rx_size - 1; i++) receive(port, i + round);
			receive(port, 255);
			assert((port == 1 ? uart_available() : uart_motor_available()) == rx_size - 1);
			for (unsigned i = 0; i < rx_size - 1; i++)
				assert((port == 1 ? uart_read() : uart_motor_read()) == (uint8_t)(i + round));
			assert((port == 1 ? uart_available() : uart_motor_available()) == 0);

			/* One byte in hardware plus the entire usable software ring. */
			for (unsigned i = 0; i < tx_size; i++)
			{
				if (port == 1) uart_write(i + round); else uart_motor_write(i + round);
			}
			for (unsigned i = 0; i < tx_size; i++)
			{
				assert((port == 1 ? SBUF : S2BUF) == (uint8_t)(i + round));
				transmit_done(port);
			}
			assert(!(port == 1 ? tx1_sending : tx2_sending));
			if (port == 1) uart_flush(); else uart_motor_flush();
		}
	}
	/* Simultaneous RX/TX interrupts must service both directions. */
	uart_write(42); uart_write(43); SBUF = 77; RI = TI = 1; isr_uart1();
	assert(uart_read() == 77 && SBUF == 43); transmit_done(1);
	uart_close(); uart_motor_close();
	assert(REN == 0 && TR1 == 0 && !(S2CON & 16) && !(AUXR & 16));
	puts("UART1/UART2 capacity, FIFO, overflow, wrap, TX completion, full duplex, close: PASS");
	return 0;
}
