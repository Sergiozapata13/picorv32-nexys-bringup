// =============================================================================
//  main_vsub.c — Microbenchmark vsub.vv vs resta escalar
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Operacion: out[i] = a[i] - b[i], i=0..N-1
//  N=128 elementos para amortizar el overhead PCPI fijo.
//
//  Datos: a[i] = i+200, b[i] = i → out[i] = 200 (constante, verificable)
// =============================================================================

#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS  10
#define N_ELEMS 128

static volatile int32_t *const a     = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const b     = (volatile int32_t*)(VDATA_BASE + 0x200);
static volatile int32_t *const out_s = (volatile int32_t*)(VDATA_BASE + 0x400);
static volatile int32_t *const out_v = (volatile int32_t*)(VDATA_BASE + 0x600);

int main(void) {
    uart_init();
    LEDS = LED_BOOT;

    uart_nl();
    uart_separator();
    uart_puts("  Microbenchmark: vsub.vv\r\n");
    uart_puts("  N=128, N_RUNS=10\r\n");
    uart_separator();
    uart_nl();

    // a[i]=i+200, b[i]=i → out[i]=200
    for (int i = 0; i < N_ELEMS; i++) {
        a[i] = i + 200;
        b[i] = i;
    }

    // Correctitud
    sk_sub(a, b, out_s, N_ELEMS);
    vk_sub_vv(a, b, out_v, N_ELEMS);
    int correct = 1;
    for (int i = 0; i < N_ELEMS; i++)
        if (out_s[i] != out_v[i] || out_s[i] != 200) { correct = 0; break; }
    uart_puts("  Correcto: "); uart_puts(correct ? "SI" : "NO"); uart_nl();
    if (!correct) { LEDS = LED_FAIL; while (1); }

    // Medicion
    uart_nl();
    uart_puts("  Midiendo escalar...\r\n");
    bench_stats_t s_esc;
    BENCH_RUN(s_esc, N_RUNS, sk_sub(a, b, out_s, N_ELEMS));
    bench_print_stats("Escalar (sub)", &s_esc);
    bench_print_runs(&s_esc);

    uart_nl();
    uart_puts("  Midiendo vectorial (vsub.vv)...\r\n");
    bench_stats_t s_vec;
    BENCH_RUN(s_vec, N_RUNS, vk_sub_vv(a, b, out_v, N_ELEMS));
    bench_print_stats("Vectorial (vsub.vv)", &s_vec);
    bench_print_runs(&s_vec);

    bench_print_comparison("vsub.vv N=128", &s_esc, &s_vec, N_ELEMS);

    uart_nl();
    uart_separator();
    uart_puts("  RESUMEN\r\n");
    uart_separator();
    int hipot = bench_hypothesis_met(s_esc.mean, s_vec.mean);
    uart_puts("  Correcto:          "); uart_puts(correct          ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo esc:  "); uart_puts(s_esc.range == 0 ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo vec:  "); uart_puts(s_vec.range == 0 ? "SI" : "NO"); uart_nl();
    uart_puts("  Hipotesis (>=30%): "); uart_puts(hipot ? "CUMPLIDA" : "NO CUMPLIDA"); uart_nl();
    uart_separator();

    LEDS = (correct && hipot) ? LED_OK : LED_FAIL;
    while (1);
    return 0;
}
