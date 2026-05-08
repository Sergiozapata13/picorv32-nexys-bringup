#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>
#include "platform.h"

#define BENCH_MAX_RUNS 16

typedef struct {
    uint32_t runs[BENCH_MAX_RUNS];
    uint32_t n;
    uint32_t mean;
    uint32_t min_c;
    uint32_t max_c;
    uint32_t range;
    uint32_t sum;
} bench_stats_t;

// Sin inicializar runs[] — GCC -O2 convierte el loop en memset que causa trap
// Los elementos runs[0..n-1] se escriben antes de leerse, no necesitan init
#define BENCH_INIT(s, nruns) do {       \
    (s).n     = (nruns);                \
    (s).min_c = 0xFFFFFFFFu;            \
    (s).max_c = 0;                      \
    (s).sum   = 0;                      \
    (s).mean  = 0;                      \
    (s).range = 0;                      \
} while(0)

#define BENCH_RUN(s, nruns, expr) do {              \
    BENCH_INIT((s), (nruns));                        \
    for (uint32_t _r = 0; _r < (nruns); _r++) {     \
        uint32_t _t0 = rdcycle();                    \
        (void)(expr);                                \
        uint32_t _t1 = rdcycle();                    \
        uint32_t _c = _t1 - _t0;                     \
        if (_r < BENCH_MAX_RUNS) (s).runs[_r] = _c; \
        (s).sum += _c;                               \
        if (_c < (s).min_c) (s).min_c = _c;         \
        if (_c > (s).max_c) (s).max_c = _c;         \
    }                                                \
    (s).mean  = (s).sum / (nruns);                   \
    (s).range = (s).max_c - (s).min_c;               \
} while(0)

void bench_print_stats(const char *label, const bench_stats_t *s);
void bench_print_runs(const bench_stats_t *s);
void bench_print_comparison(const char *name,
                             const bench_stats_t *scalar,
                             const bench_stats_t *vector,
                             uint32_t n_elems);
uint32_t bench_improvement_pct(uint32_t scalar_cycles, uint32_t vector_cycles);
int bench_hypothesis_met(uint32_t scalar_cycles, uint32_t vector_cycles);

#endif
