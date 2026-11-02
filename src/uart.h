/*
 * USART1 transmit-only debug output on PA9, 115200 8N1, polled (no interrupts).
 * Reaches the Mac through the debugger's USB-UART bridge (its RXD pin).
 */
#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
/* Output is queued (511 usable bytes); call poll continuously in main.
 * Flush drains queued bytes into the transmitter; thread mode only. */
void uart_poll(void);
void uart_flush(void);
void uart_put_i32(int32_t v);        /* signed decimal */
void uart_putc(char c);              /* "\n" is sent as "\r\n" */
void uart_puts(const char *s);
void uart_put_u32(uint32_t v);       /* decimal */

#endif
