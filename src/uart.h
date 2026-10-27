/*
 * USART1 transmit-only debug output on PA9, 115200 8N1, polled (no interrupts).
 * Reaches the Mac through the debugger's USB-UART bridge (its RXD pin).
 */
#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);              /* "\n" is sent as "\r\n" */
void uart_puts(const char *s);
void uart_put_u32(uint32_t v);       /* decimal */
void uart_put_hex32(uint32_t v);     /* 8 hex digits, no prefix */

#endif
