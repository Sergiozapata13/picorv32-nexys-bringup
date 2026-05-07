// =============================================================================
//  uart.c — Implementacion de funciones UART
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
// =============================================================================

#include "platform.h"
#include "uart.h"

void uart_init(void) {
    UART_DIV = UART_DIV_115200;
}

void uart_putc(char c) {
    UART_DATA = (uint32_t)(uint8_t)c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

void uart_puthex32(uint32_t v) {
    static const char h[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 7; i >= 0; i--)
        uart_putc(h[(v >> (i * 4)) & 0xF]);
}

void uart_putdec(uint32_t v) {
    if (!v) { uart_putc('0'); return; }
    char buf[12];
    int i = 0;
    while (v) {
        buf[i++] = '0' + (int)(v % 10);
        v /= 10;
    }
    for (int j = i - 1; j >= 0; j--) uart_putc(buf[j]);
}

void uart_putdec_signed(int32_t v) {
    if (v < 0) {
        uart_putc('-');
        uart_putdec((uint32_t)(-v));
    } else {
        uart_putdec((uint32_t)v);
    }
}

void uart_nl(void) {
    uart_putc('\r');
    uart_putc('\n');
}

void uart_print_dec(const char *label, uint32_t value) {
    uart_puts(label);
    uart_putdec(value);
    uart_nl();
}

void uart_print_hex(const char *label, uint32_t value) {
    uart_puts(label);
    uart_puthex32(value);
    uart_nl();
}

void uart_separator(void) {
    uart_puts("==========================================\r\n");
}
