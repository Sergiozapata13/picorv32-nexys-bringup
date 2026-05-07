// =============================================================================
//  bench.h — Infraestructura de medicion estadistica
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Provee la macro BENCH_MEASURE(N_RUNS, expr) que:
//    1. Ejecuta `expr` N_RUNS veces
//    2. Mide rdcycle antes/despues de cada ejecucion
//    3. Calcula media, minimo, maximo y rango
//    4. Devuelve un struct bench_stats_t con los resultados
//
//  Uso:
//    bench_stats_t s = BENCH_MEASURE(10, my_kernel(a, b, N));
//    print_stats("Mi kernel", &s);
// =============================================================================

#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>
#include "platform.h"

#define BENCH_MAX_RUNS 16  // tope de repeticiones por benchmark

// Estructura de resultados estadisticos
typedef struct {
    uint32_t runs[BENCH_MAX_RUNS];  // ciclos de cada corrida
    uint32_t n;                      // numero de corridas validas
    uint32_t mean;                   // media (sum / n)
    uint32_t min_c;                  // ciclo minimo observado
    uint32_t max_c;                  // ciclo maximo observado
    uint32_t range;                  // max - min
    uint32_t sum;                    // suma total
} bench_stats_t;

// Macro de medicion estadistica
// Ejecuta `expr` N_RUNS veces y devuelve un bench_stats_t con las metricas
#define BENCH_MEASURE(N_RUNS, expr) ({                          \
    bench_stats_t _s = {0};                                     \
    _s.n = (N_RUNS);                                            \
    _s.min_c = 0xFFFFFFFFu;                                     \
    _s.max_c = 0;                                               \
    _s.sum = 0;                                                 \
    for (uint32_t _r = 0; _r < (N_RUNS); _r++) {                \
        uint32_t _t0 = rdcycle();                               \
        (expr);                                                 \
        uint32_t _t1 = rdcycle();                               \
        uint32_t _c = _t1 - _t0;                                \
        if (_r < BENCH_MAX_RUNS) _s.runs[_r] = _c;              \
        _s.sum += _c;                                           \
        if (_c < _s.min_c) _s.min_c = _c;                       \
        if (_c > _s.max_c) _s.max_c = _c;                       \
    }                                                            \
    _s.mean  = _s.sum / (N_RUNS);                                \
    _s.range = _s.max_c - _s.min_c;                              \
    _s;                                                          \
})

// Imprime resumen estadistico de un bench_stats_t
//   Etiqueta    : Mi kernel
//   Corridas    : 10
//   Media       : 848 ciclos
//   Min         : 848
//   Max         : 848
//   Rango       : 0  (deterministico)
void bench_print_stats(const char *label, const bench_stats_t *s);

// Imprime las corridas individuales (util para verificar determinismo visualmente)
//   [1]: 848  [2]: 848  [3]: 848  ... [10]: 848
void bench_print_runs(const bench_stats_t *s);

// Imprime comparativa entre escalar y vectorial con porcentaje de mejora
//   Mejora: 64% (escalar 2385 -> vectorial 848)
void bench_print_comparison(const char *name,
                             const bench_stats_t *scalar,
                             const bench_stats_t *vector,
                             uint32_t n_elems);

// Calcula el porcentaje de mejora: (esc - vec) * 100 / esc
// Devuelve 0 si vec >= esc (sin mejora)
uint32_t bench_improvement_pct(uint32_t scalar_cycles, uint32_t vector_cycles);

// Verifica si la mejora cumple el umbral de la hipotesis (>= 30%)
int bench_hypothesis_met(uint32_t scalar_cycles, uint32_t vector_cycles);

#endif // BENCH_H
