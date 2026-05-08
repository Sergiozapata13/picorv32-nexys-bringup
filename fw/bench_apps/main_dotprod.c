#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS  10
#define N_ELEMS 32

static volatile int32_t *const vec_a = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const vec_b = (volatile int32_t*)(VDATA_BASE + 0x080);

int main(void) {
    uart_init();
    LEDS = LED_BOOT;

    uart_nl();
    uart_separator();
    uart_puts("  Benchmark: Dot Product\r\n");
    uart_puts("  N_ELEMS=32, N_RUNS=10\r\n");
    uart_separator();
    uart_nl();

    for (int i = 0; i < N_ELEMS; i++) { vec_a[i] = i+1; vec_b[i] = i+1; }

    // Correctitud
    int32_t expected = sk_dot(vec_a, vec_b, N_ELEMS);
    int32_t got_v    = vk_dot(vec_a, vec_b, N_ELEMS);
    int correct = (expected == got_v) && (expected == 11440);
    uart_print_dec("  Resultado escalar:   ", (uint32_t)expected);
    uart_print_dec("  Resultado vectorial: ", (uint32_t)got_v);
    uart_puts("  Correcto: "); uart_puts(correct ? "SI" : "NO"); uart_nl();
    if (!correct) { LEDS = LED_FAIL; while (1); }

    // Medicion escalar
    uart_nl();
    uart_puts("  Midiendo escalar...\r\n");
    bench_stats_t s_esc;
    BENCH_RUN(s_esc, N_RUNS, sk_dot(vec_a, vec_b, N_ELEMS));
    bench_print_stats("Escalar", &s_esc);
    bench_print_runs(&s_esc);

    // Medicion vectorial
    uart_nl();
    uart_puts("  Midiendo vectorial...\r\n");
    bench_stats_t s_vec;
    BENCH_RUN(s_vec, N_RUNS, vk_dot(vec_a, vec_b, N_ELEMS));
    bench_print_stats("Vectorial", &s_vec);
    bench_print_runs(&s_vec);

    bench_print_comparison("Dot product N=32", &s_esc, &s_vec, N_ELEMS);

    uart_nl();
    uart_separator();
    uart_puts("  RESUMEN\r\n");
    uart_separator();
    int hipot = bench_hypothesis_met(s_esc.mean, s_vec.mean);
    uart_puts("  Correcto:          "); uart_puts(correct      ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo esc:  "); uart_puts(s_esc.range == 0 ? "SI" : "NO"); uart_nl();
    uart_puts("  Determinismo vec:  "); uart_puts(s_vec.range == 0 ? "SI" : "NO"); uart_nl();
    uart_puts("  Hipotesis (>=30%): "); uart_puts(hipot ? "CUMPLIDA" : "NO CUMPLIDA"); uart_nl();
    uart_separator();

    LEDS = (correct && hipot) ? LED_OK : LED_FAIL;
    while (1);
    return 0;
}
