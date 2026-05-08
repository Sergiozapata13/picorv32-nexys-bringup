// =============================================================================
//  main_vlogic.c — Microbenchmark vand.vv / vor.vv / vxor.vv
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Tres sub-tests en el mismo firmware:
//    AND: a[i]=0xFFFF00FF, b[i]=0x00FFFF00 → out[i]=0x00FF0000
//    OR:  a[i]=0xFF000000, b[i]=0x00FF0000 → out[i]=0xFFFF0000
//    XOR: a[i]=0xAAAAAAAA, b[i]=0x55555555 → out[i]=0xFFFFFFFF
//
//  N=128 elementos.
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

static int run_test(const char *name_esc, const char *name_vec,
                    const char *bench_name,
                    void (*fn_s)(volatile int32_t*, volatile int32_t*,
                                 volatile int32_t*, int),
                    void (*fn_v)(volatile int32_t*, volatile int32_t*,
                                 volatile int32_t*, int),
                    int32_t expected_val) {
    // Correctitud
    fn_s(a, b, out_s, N_ELEMS);
    fn_v(a, b, out_v, N_ELEMS);
    int correct = 1;
    for (int i = 0; i < N_ELEMS; i++)
        if (out_s[i] != out_v[i] || out_s[i] != expected_val) { correct = 0; break; }
    uart_puts("  Correcto: "); uart_puts(correct ? "SI" : "NO"); uart_nl();
    if (!correct) return 0;

    // Medicion escalar
    uart_nl();
    uart_puts("  Midiendo escalar...\r\n");
    bench_stats_t s_esc;
    BENCH_RUN(s_esc, N_RUNS, fn_s(a, b, out_s, N_ELEMS));
    bench_print_stats(name_esc, &s_esc);
    bench_print_runs(&s_esc);

    // Medicion vectorial
    uart_nl();
    uart_puts("  Midiendo vectorial...\r\n");
    bench_stats_t s_vec;
    BENCH_RUN(s_vec, N_RUNS, fn_v(a, b, out_v, N_ELEMS));
    bench_print_stats(name_vec, &s_vec);
    bench_print_runs(&s_vec);

    bench_print_comparison(bench_name, &s_esc, &s_vec, N_ELEMS);

    uart_puts("  Determinismo esc:  "); uart_puts(s_esc.range == 0 ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo vec:  "); uart_puts(s_vec.range == 0 ? "SI" : "NO"); uart_nl();
    int hipot = bench_hypothesis_met(s_esc.mean, s_vec.mean);
    uart_puts("  Hipotesis (>=30%): "); uart_puts(hipot ? "CUMPLIDA" : "NO CUMPLIDA"); uart_nl();
    return correct;
}

int main(void) {
    uart_init();
    LEDS = LED_BOOT;

    uart_nl();
    uart_separator();
    uart_puts("  Microbenchmark: vand / vor / vxor\r\n");
    uart_puts("  N=128, N_RUNS=10\r\n");
    uart_separator();

    int all_ok = 1;

    // ── Sub-test 1: vand.vv ───────────────────────────────────────────────────
    uart_nl();
    uart_puts("  --- Sub-test 1: vand.vv ---\r\n");
    for (int i = 0; i < N_ELEMS; i++) {
        a[i] = (int32_t)0xFFFF00FF;
        b[i] = (int32_t)0x00FFFF00;
    }
    all_ok &= run_test("Escalar (and)", "Vectorial (vand.vv)",
                       "vand.vv N=128", sk_and, vk_and_vv,
                       (int32_t)0x00FF0000);

    // ── Sub-test 2: vor.vv ────────────────────────────────────────────────────
    uart_nl();
    uart_puts("  --- Sub-test 2: vor.vv ---\r\n");
    for (int i = 0; i < N_ELEMS; i++) {
        a[i] = (int32_t)0xFF000000;
        b[i] = (int32_t)0x00FF0000;
    }
    all_ok &= run_test("Escalar (or)", "Vectorial (vor.vv)",
                       "vor.vv N=128", sk_or, vk_or_vv,
                       (int32_t)0xFFFF0000);

    // ── Sub-test 3: vxor.vv ───────────────────────────────────────────────────
    uart_nl();
    uart_puts("  --- Sub-test 3: vxor.vv ---\r\n");
    for (int i = 0; i < N_ELEMS; i++) {
        a[i] = (int32_t)0xAAAAAAAA;
        b[i] = (int32_t)0x55555555;
    }
    all_ok &= run_test("Escalar (xor)", "Vectorial (vxor.vv)",
                       "vxor.vv N=128", sk_xor, vk_xor_vv,
                       (int32_t)0xFFFFFFFF);

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
