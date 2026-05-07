// =============================================================================
//  platform.h — Definiciones del SoC PicoRV32 + VPU
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Hardware target: Nexys A7-100T a 100 MHz
//  Mapa de memoria:
//    0x0000_0000 - 0x0000_FFFF   RAM (BRAM 64 KiB)
//    0x1000_0000                 GPIO LEDs[15:0]
//    0x2000_0000                 UART divisor baud
//    0x2000_0004                 UART dato TX/RX
// =============================================================================

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

// ─── Periféricos ─────────────────────────────────────────────────────────────
#define LEDS      (*(volatile uint32_t*)0x10000000)
#define UART_DIV  (*(volatile uint32_t*)0x20000000)
#define UART_DATA (*(volatile uint32_t*)0x20000004)

// Divisor para 115200 baud a 100 MHz: 100_000_000 / 115200 ≈ 868
#define UART_DIV_115200  868

// ─── Memoria de datos para benchmarks ────────────────────────────────────────
// Zona alta de RAM, lejos del stack en 0xFFFC y del codigo en 0x0000
#define VDATA_BASE  0x0000E000

// ─── Frecuencia del sistema ──────────────────────────────────────────────────
#define SYS_FREQ_HZ  100000000U   // 100 MHz
#define NS_PER_CYCLE 10           // 10 ns por ciclo

// ─── Indicadores LED de estado ───────────────────────────────────────────────
#define LED_BOOT    0x0001    // arranque
#define LED_OK      0x00FF    // todos los benchmarks correctos
#define LED_FAIL    0xDEAD    // algun benchmark fallo

// ─── rdcycle: leer contador de ciclos del CPU ────────────────────────────────
static inline uint32_t rdcycle(void) {
    uint32_t c;
    asm volatile ("rdcycle %0" : "=r"(c));
    return c;
}

#endif // PLATFORM_H
