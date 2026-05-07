// =============================================================================
//  uart.h — Funciones de UART para impresion serie
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
// =============================================================================

#ifndef UART_H
#define UART_H

#include <stdint.h>

// Inicializacion del UART (115200 baud)
void uart_init(void);

// Salida de un caracter
void uart_putc(char c);

// Salida de string terminado en null
void uart_puts(const char *s);

// Salida de numero hexadecimal de 32 bits con prefijo "0x"
void uart_puthex32(uint32_t v);

// Salida de numero decimal sin signo
void uart_putdec(uint32_t v);

// Salida de numero decimal con signo
void uart_putdec_signed(int32_t v);

// Nueva linea (CRLF)
void uart_nl(void);

// Imprimir "etiqueta: numero<NL>"
void uart_print_dec(const char *label, uint32_t value);
void uart_print_hex(const char *label, uint32_t value);

// Linea de separacion: "===...==="
void uart_separator(void);

#endif // UART_H
