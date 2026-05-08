// =============================================================================
//  main_vshift.c — Microbenchmark vsll.vv / vsrl.vv
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Dos sub-tests:
//    SLL: a[i]=1, b[i]=i%32 → out[i] = 1 << (i%32)
//    SRL: a[i]=0x80000000, b[i]=i%32 → out[i] = 0x80000000 >> (i%32)
//
//  N=128 elementos. El modulo %32 garantiza shifts validos (0-31).
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
    uart_puts("  Microbenchmark: vsll.vv / vsrl.vv\r\n");
    uart_puts("  N=128, N_RUNS=10\r\n");
    uart_separator();

    int all_ok = 1;

    // ── Sub-test 1: vsll.vv ───────────────────────────────────────────────────
    uart_nl();
    uart_puts("  --- Sub-test 1: vsll.vv ---\r\n");

    // a[i]=1, b[i]=i%32 → out[i] = 1 << (i%32)
    for (int i = 0; i < N_ELEMS; i++) {
        a[i] = 1;
        b[i] = i % 32;
    }

    sk_sll(a, b, out_s, N_ELEMS);
    vk_sll_vv(a, b, out_v, N_ELEMS);
    int correct_sll = 1;
    for (int i = 0; i < N_ELEMS; i++)
        if (out_s[i] != out_v[i]) { correct_sll = 0; break; }
    uart_puts("  Correcto: "); uart_puts(correct_sll ? "SI" : "NO"); uart_nl();

    if (correct_sll) {
        uart_nl();
        uart_puts("  Midiendo escalar...\r\n");
        bench_stats_t s_esc;
        BENCH_RUN(s_esc, N_RUNS, sk_sll(a, b, out_s, N_ELEMS));
        bench_print_stats("Escalar (sll)", &s_esc);
        bench_print_runs(&s_esc);

        uart_nl();
        uart_puts("  Midiendo vectorial (vsll.vv)...\r\n");
        bench_stats_t s_vec;
        BENCH_RUN(s_vec, N_RUNS, vk_sll_vv(a, b, out_v, N_ELEMS));
        bench_print_stats("Vectorial (vsll.vv)", &s_vec);
        bench_print_runs(&s_vec);

        bench_print_comparison("vsll.vv N=128", &s_esc, &s_vec, N_ELEMS);
        uart_puts("  Determinismo esc:  "); uart_puts(s_esc.range == 0 ? "SI" : "NO"); uart_nl();
        uart_puts("  Determinismo vec:  "); uart_puts(s_vec.range == 0 ? "SI" : "NO"); uart_nl();
        uart_puts("  Hipotesis (>=30%): ");
        uart_puts(bench_hypothesis_met(s_esc.mean, s_vec.mean) ? "CUMPLIDA" : "NO CUMPLIDA");
        uart_nl();
    } else { all_ok = 0; }

    // ── Sub-test 2: vsrl.vv ───────────────────────────────────────────────────
    uart_nl();
    uart_puts("  --- Sub-test 2: vsrl.vv ---\r\n");

    // a[i]=0x80000000, b[i]=i%32 → out[i] = 0x80000000 >> (i%32)
    for (int i = 0; i < N_ELEMS; i++) {
        a[i] = (int32_t)0x80000000;
        b[i] = i % 32;
    }

    sk_srl(a, b, out_s, N_ELEMS);
    vk_srl_vv(a, b, out_v, N_ELEMS);
    int correct_srl = 1;
    for (int i = 0; i < N_ELEMS; i++)
        if (out_s[i] != out_v[i]) { correct_srl = 0; break; }
    uart_puts("  Correcto: "); uart_puts(correct_srl ? "SI" : "NO"); uart_nl();

    if (correct_srl) {
        uart_nl();
        uart_puts("  Midiendo escalar...\r\n");
        bench_stats_t s_esc;
        BENCH_RUN(s_esc, N_RUNS, sk_srl(a, b, out_s, N_ELEMS));
        bench_print_stats("Escalar (srl)", &s_esc);
        bench_print_runs(&s_esc);

        uart_nl();
        uart_puts("  Midiendo vectorial (vsrl.vv)...\r\n");
        bench_stats_t s_vec;
        BENCH_RUN(s_vec, N_RUNS, vk_srl_vv(a, b, out_v, N_ELEMS));
        bench_print_stats("Vectorial (vsrl.vv)", &s_vec);
        bench_print_runs(&s_vec);

        bench_print_comparison("vsrl.vv N=128", &s_esc, &s_vec, N_ELEMS);
        uart_puts("  Determinismo esc:  "); uart_puts(s_esc.range == 0 ? "SI" : "NO"); uart_nl();
        uart_puts("  Determinismo vec:  "); uart_puts(s_vec.range == 0 ? "SI" : "NO"); uart_nl();
        uart_puts("  Hipotesis (>=30%): ");
        uart_puts(bench_hypothesis_met(s_esc.mean, s_vec.mean) ? "CUMPLIDA" : "NO CUMPLIDA");
        uart_nl();
    } else { all_ok = 0; }

    // ── Resumen global ────────────────────────────────────────────────────────
    uart_nl();
    uart_separator();
    uart_puts("  RESUMEN GLOBAL\r\n");
    uart_separator();
    uart_puts("  Todos correctos: "); uart_puts(all_ok ? "SI" : "NO"); uart_nl();
    uart_separator();

    LEDS = all_ok ? LED_OK : LED_FAIL;
    while (1);
    return 0;
}
