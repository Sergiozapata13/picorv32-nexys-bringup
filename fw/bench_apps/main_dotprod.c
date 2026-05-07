// =============================================================================
//  main_dotprod.c — Benchmark dot product con validez estadistica
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Migracion del benchmark dot product de OE4 a la nueva arquitectura modular.
//
//  Diferencias respecto al main.c original de OE4:
//    - Usa headers compartidos (platform.h, uart.h, bench.h, vpu_kernels.h)
//    - 10 corridas por kernel (escalar y vectorial)
//    - Reporta media, min, max, rango y determinismo
//    - Imprime corridas individuales para inspeccion visual
//
//  Resultado esperado: misma mejora del 64% del benchmark original.
//  Si el rango = 0, el sistema es deterministico (resultado adicional valioso).
// =============================================================================

#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS  10
#define N_ELEMS 32

// Vectores de prueba en RAM alta
static volatile int32_t *const vec_a = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const vec_b = (volatile int32_t*)(VDATA_BASE + 0x080);

int main(void) {
    uart_init();
    LEDS = LED_BOOT;

    uart_nl();
    uart_separator();
    uart_puts("  Benchmark: Dot Product (migracion Fase 1)\r\n");
    uart_puts("  N_ELEMS=32, N_RUNS=10\r\n");
    uart_separator();
    uart_nl();

    // ─── Inicializacion de vectores ──────────────────────────────────────────
    // a = [1,2,...,32], b = [1,1,...,1] -> dot = 528
    for (int i = 0; i < N_ELEMS; i++) {
        vec_a[i] = i + 1;
        vec_b[i] = 1;
    }

    // ─── Verificacion de correctitud (1 corrida fuera de medicion) ──────────
    int32_t expected = sk_dot(vec_a, vec_b, N_ELEMS);
    int32_t got_v    = vk_dot(vec_a, vec_b, N_ELEMS);

    uart_print_dec("  Resultado escalar:   ", (uint32_t)expected);
    uart_print_dec("  Resultado vectorial: ", (uint32_t)got_v);
    int correct = (expected == got_v) && (expected == 528);
    uart_puts("  Correcto: "); uart_puts(correct ? "SI" : "NO"); uart_nl();

    if (!correct) {
        uart_puts("\r\n  ERROR: resultados no coinciden\r\n");
        LEDS = LED_FAIL;
        while (1);
    }

    // ─── Medicion estadistica — ESCALAR ──────────────────────────────────────
    uart_nl();
    uart_puts("  Midiendo escalar (");
    uart_putdec(N_RUNS); uart_puts(" corridas)...\r\n");

    volatile int32_t sink_s = 0;  // evitar que GCC optimice el resultado
    bench_stats_t s_esc = BENCH_MEASURE(N_RUNS,
        sink_s = sk_dot(vec_a, vec_b, N_ELEMS)
    );
    bench_print_stats("Escalar", &s_esc);
    bench_print_runs(&s_esc);

    // ─── Medicion estadistica — VECTORIAL ────────────────────────────────────
    uart_nl();
    uart_puts("  Midiendo vectorial (");
    uart_putdec(N_RUNS); uart_puts(" corridas)...\r\n");

    volatile int32_t sink_v = 0;
    bench_stats_t s_vec = BENCH_MEASURE(N_RUNS,
        sink_v = vk_dot(vec_a, vec_b, N_ELEMS)
    );
    bench_print_stats("Vectorial", &s_vec);
    bench_print_runs(&s_vec);

    // ─── Comparacion y conclusion ────────────────────────────────────────────
    bench_print_comparison("Dot product N=32", &s_esc, &s_vec, N_ELEMS);

    // ─── Resumen ─────────────────────────────────────────────────────────────
    uart_nl();
    uart_separator();
    uart_puts("  RESUMEN\r\n");
    uart_separator();

    int hipot_ok    = bench_hypothesis_met(s_esc.mean, s_vec.mean);
    int determ_esc  = (s_esc.range == 0);
    int determ_vec  = (s_vec.range == 0);

    uart_puts("  Correctitud:        "); uart_puts(correct ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo esc:   "); uart_puts(determ_esc ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo vec:   "); uart_puts(determ_vec ? "SI" : "NO"); uart_nl();
    uart_puts("  Hipotesis (>=30%):  "); uart_puts(hipot_ok ? "CUMPLIDA" : "NO CUMPLIDA"); uart_nl();
    uart_separator();

    LEDS = (correct && hipot_ok) ? LED_OK : LED_FAIL;
    while (1);
    return 0;
}
