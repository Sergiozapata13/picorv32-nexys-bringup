// =============================================================================
//  bench.c — Implementacion de funciones de impresion estadistica
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
// =============================================================================

#include "bench.h"
#include "uart.h"

void bench_print_stats(const char *label, const bench_stats_t *s) {
    uart_puts("  [");
    uart_puts(label);
    uart_puts("]\r\n");
    uart_print_dec("    Corridas:  ", s->n);
    uart_print_dec("    Media:     ", s->mean);
    uart_print_dec("    Min:       ", s->min_c);
    uart_print_dec("    Max:       ", s->max_c);
    uart_print_dec("    Rango:     ", s->range);
    if (s->range == 0) {
        uart_puts("    Determinismo: SI (todas las corridas iguales)\r\n");
    } else {
        uart_puts("    Determinismo: NO (rango > 0)\r\n");
    }
}

void bench_print_runs(const bench_stats_t *s) {
    uart_puts("    Corridas individuales:\r\n     ");
    uint32_t n = (s->n < BENCH_MAX_RUNS) ? s->n : BENCH_MAX_RUNS;
    for (uint32_t i = 0; i < n; i++) {
        uart_puts(" [");
        uart_putdec(i + 1);
        uart_puts("]=");
        uart_putdec(s->runs[i]);
        if ((i + 1) % 5 == 0 && i + 1 < n) {
            uart_puts("\r\n     ");
        }
    }
    uart_nl();
}

uint32_t bench_improvement_pct(uint32_t scalar_cycles, uint32_t vector_cycles) {
    if (vector_cycles >= scalar_cycles) return 0;
    return (scalar_cycles - vector_cycles) * 100u / scalar_cycles;
}

int bench_hypothesis_met(uint32_t scalar_cycles, uint32_t vector_cycles) {
    return bench_improvement_pct(scalar_cycles, vector_cycles) >= 30u;
}

void bench_print_comparison(const char *name,
                             const bench_stats_t *scalar,
                             const bench_stats_t *vector,
                             uint32_t n_elems) {
    uart_puts("\r\n  --- Comparacion: ");
    uart_puts(name);
    uart_puts(" ---\r\n");

    uart_print_dec("    Escalar (media):   ", scalar->mean);
    uart_print_dec("    Vectorial (media): ", vector->mean);

    if (n_elems > 0) {
        uart_print_dec("    Ciclos/elem esc:   ", scalar->mean / n_elems);
        uart_print_dec("    Ciclos/elem vec:   ", vector->mean / n_elems);
    }

    uint32_t pct = bench_improvement_pct(scalar->mean, vector->mean);
    if (pct > 0) {
        uart_puts("    Mejora:            ");
        uart_putdec(pct);
        uart_puts("%\r\n");
    } else {
        uart_puts("    Mejora:            (sin mejora — vectorial >= escalar)\r\n");
    }

    uart_puts("    Hipotesis (>=30%): ");
    uart_puts(bench_hypothesis_met(scalar->mean, vector->mean)
              ? "CUMPLIDA\r\n" : "NO CUMPLIDA\r\n");
}
