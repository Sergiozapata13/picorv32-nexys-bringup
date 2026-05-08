// =============================================================================
//  main_fir.c — Benchmark Filtro FIR con validez estadistica
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Mide ciclos para FIR de N=32 coeficientes sobre N=32 muestras.
//  Implementacion: y[i] = sum(h[k] * x[(i+k)%N], k=0..N-1)
//
//  Filtro: h[0]=1, h[1..31]=0 (identidad) → y[i] = x[i]
//  Permite verificacion exacta: la salida debe ser igual a la entrada.
//
//  Mapa de memoria:
//    VDATA_BASE + 0x000  x[32]     senial de entrada  (128 bytes)
//    VDATA_BASE + 0x080  h[32]     coeficientes FIR   (128 bytes)
//    VDATA_BASE + 0x100  y_s[32]   salida escalar     (128 bytes)
//    VDATA_BASE + 0x180  y_v[32]   salida vectorial   (128 bytes)
//    VDATA_BASE + 0x200  buf[32]   buffer deslizante  (128 bytes)
// =============================================================================

#include <stdint.h>
#include "platform.h"
#include "uart.h"
#include "bench.h"
#include "vpu_kernels.h"

#define N_RUNS  10
#define N_ELEMS 32

static volatile int32_t *const x   = (volatile int32_t*)(VDATA_BASE + 0x000);
static volatile int32_t *const h   = (volatile int32_t*)(VDATA_BASE + 0x080);
static volatile int32_t *const y_s = (volatile int32_t*)(VDATA_BASE + 0x100);
static volatile int32_t *const y_v = (volatile int32_t*)(VDATA_BASE + 0x180);
static volatile int32_t *const buf = (volatile int32_t*)(VDATA_BASE + 0x200);

int main(void) {
    uart_init();
    LEDS = LED_BOOT;

    uart_nl();
    uart_separator();
    uart_puts("  Benchmark: FIR (N_COEFS=32, N_MUESTRAS=32)\r\n");
    uart_puts("  N_RUNS=10\r\n");
    uart_separator();
    uart_nl();

    // ─── Inicializacion ───────────────────────────────────────────────────────
    // Señal de entrada: x[i] = i+1
    // Coeficientes: filtro identidad h[0]=1, h[1..31]=0 → y[i] = x[i]
    for (int i = 0; i < N_ELEMS; i++) {
        x[i] = i + 1;
        h[i] = (i == 0) ? 1 : 0;
    }

    // ─── Verificacion de correctitud (fuera de medicion) ─────────────────────
    uart_puts("  Verificando correctitud...\r\n");
    sk_fir(x, h, y_s, N_ELEMS);
    vk_fir(x, h, y_v, buf, N_ELEMS);

    int correct = 1;
    for (int i = 0; i < N_ELEMS; i++) {
        if (y_s[i] != y_v[i]) { correct = 0; break; }
        // Con filtro identidad: y[i] debe ser x[i] = i+1
        if (y_s[i] != (int32_t)(i + 1)) { correct = 0; break; }
    }
    uart_puts("  Correcto: "); uart_puts(correct ? "SI" : "NO"); uart_nl();

    if (!correct) {
        uart_puts("\r\n  ERROR: resultados no coinciden\r\n");
        LEDS = LED_FAIL;
        while (1);
    }

    // ─── Medicion escalar ─────────────────────────────────────────────────────
    uart_nl();
    uart_puts("  Midiendo escalar...\r\n");
    bench_stats_t s_esc;
    BENCH_RUN(s_esc, N_RUNS, sk_fir(x, h, y_s, N_ELEMS));
    bench_print_stats("Escalar", &s_esc);
    bench_print_runs(&s_esc);

    // ─── Medicion vectorial ───────────────────────────────────────────────────
    uart_nl();
    uart_puts("  Midiendo vectorial...\r\n");
    bench_stats_t s_vec;
    BENCH_RUN(s_vec, N_RUNS, vk_fir(x, h, y_v, buf, N_ELEMS));
    bench_print_stats("Vectorial", &s_vec);
    bench_print_runs(&s_vec);

    // ─── Comparacion ─────────────────────────────────────────────────────────
    bench_print_comparison("FIR N=32", &s_esc, &s_vec, N_ELEMS);

    // ─── Resumen ─────────────────────────────────────────────────────────────
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
