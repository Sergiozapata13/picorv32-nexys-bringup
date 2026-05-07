// =============================================================================
//  vpu_kernels.h — Kernels vectoriales reutilizables
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Funciones inline que encapsulan los patrones vectoriales mas comunes
//  para que cada main_*.c los pueda usar sin reproducirlos.
//
//  Todas las funciones usan vsetvli dinamico — manejan N arbitrario.
// =============================================================================

#ifndef VPU_KERNELS_H
#define VPU_KERNELS_H

#include <stdint.h>
#include "vpu_asm.h"

// ─── Operaciones VALU element-wise ───────────────────────────────────────────
//
//  Patron general:
//    while (rem > 0) {
//      vsetvli a0, rem, e32m1
//      vle32.v v2, (pa)
//      vle32.v v3, (pb)
//      vOP.vv v1, v2, v3
//      vse32.v v1, (po)
//      rem -= vl, pa += vl, pb += vl, po += vl
//    }
//
//  Macro generador para evitar duplicar codigo:

#define VPU_ELEMENTWISE_KERNEL(NAME, ASM_OP)                            \
static inline void NAME(volatile int32_t *a,                            \
                        volatile int32_t *b,                            \
                        volatile int32_t *out,                          \
                        int n) {                                        \
    int rem = n;                                                        \
    volatile int32_t *pa = a;                                           \
    volatile int32_t *pb = b;                                           \
    volatile int32_t *po = out;                                         \
    while (rem > 0) {                                                   \
        int vl;                                                         \
        asm volatile (                                                  \
            "mv a1, %4\n"                                               \
            ASM_VSETVLI_A0_A1_E32M1                                     \
            "mv %0, a0\n"                                               \
            "mv a0, %1\n"                                               \
            ASM_VLE32_V2_A0                                             \
            "mv a1, %2\n"                                               \
            ASM_VLE32_V3_A1                                             \
            ASM_OP                                                      \
            "mv a0, %3\n"                                               \
            ASM_VSE32_V1_A0                                             \
            : "=r"(vl)                                                  \
            : "r"((uint32_t)pa), "r"((uint32_t)pb),                     \
              "r"((uint32_t)po), "r"(rem)                               \
            : "a0", "a1", "memory"                                      \
        );                                                              \
        rem -= vl;                                                      \
        pa  += vl;                                                      \
        pb  += vl;                                                      \
        po  += vl;                                                      \
    }                                                                   \
}

// Generar kernels para cada operacion VALU
VPU_ELEMENTWISE_KERNEL(vk_add_vv, ASM_VADD_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_sub_vv, ASM_VSUB_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_and_vv, ASM_VAND_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_or_vv,  ASM_VOR_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_xor_vv, ASM_VXOR_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_sll_vv, ASM_VSLL_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_srl_vv, ASM_VSRL_VV_V1_V2_V3)
VPU_ELEMENTWISE_KERNEL(vk_mul_vv, ASM_VMUL_VV_V1_V2_V3)


// ─── Dot product: a · b (acumulado en v4[0]) ─────────────────────────────────
static inline int32_t vk_dot(volatile int32_t *a, volatile int32_t *b, int n) {
    // Inicializar acumulador v4 = [0,0,0,0]
    asm volatile (
        ASM_INIT_V4_ZERO
        : : : "a0", "memory"
    );

    int rem = n;
    volatile int32_t *pa = a;
    volatile int32_t *pb = b;

    while (rem > 0) {
        int vl;
        asm volatile (
            "mv a1, %2\n"
            ASM_VSETVLI_A0_A1_E32M1
            "mv %0, a0\n"
            "mv a0, %3\n"
            ASM_VLE32_V2_A0
            "mv a1, %4\n"
            ASM_VLE32_V3_A1
            ASM_VMUL_VV_V1_V2_V3
            ASM_VREDSUM_VS_V4_V1
            : "=r"(vl)
            : "r"((uint32_t)pa), "r"(rem),
              "r"((uint32_t)pa), "r"((uint32_t)pb)
            : "a0", "a1", "memory"
        );
        rem -= vl;
        pa  += vl;
        pb  += vl;
    }

    // Leer resultado de v4[0]
    int32_t result;
    asm volatile (
        ASM_VMV_X_S_A0_V4
        "mv %0, a0\n"
        : "=r"(result) : : "a0", "memory"
    );
    return result;
}


// ─── vse32 throughput: escribir N veces el mismo valor ───────────────────────
//  Util para medir el costo puro de vse32 sin influencia de loads
static inline void vk_store_constant(volatile int32_t *out, int32_t value, int n) {
    // Llenar v1 con el valor escalar
    asm volatile (
        "mv a0, %0\n"
        ASM_VMV_V_X_V1_A0
        : : "r"(value) : "a0", "memory"
    );

    int rem = n;
    volatile int32_t *po = out;
    while (rem > 0) {
        int vl;
        asm volatile (
            "mv a1, %1\n"
            ASM_VSETVLI_A0_A1_E32M1
            "mv %0, a0\n"
            "mv a0, %2\n"
            ASM_VSE32_V1_A0
            : "=r"(vl)
            : "r"(rem), "r"((uint32_t)po)
            : "a0", "a1", "memory"
        );
        rem -= vl;
        po  += vl;
    }
}


// ─── Equivalentes escalares ──────────────────────────────────────────────────
//  Para comparacion en los benchmarks

#define SCALAR_ELEMENTWISE_KERNEL(NAME, OP)                             \
static inline void NAME(volatile int32_t *a,                            \
                        volatile int32_t *b,                            \
                        volatile int32_t *out,                          \
                        int n) {                                        \
    for (int i = 0; i < n; i++) out[i] = a[i] OP b[i];                  \
}

SCALAR_ELEMENTWISE_KERNEL(sk_add, +)
SCALAR_ELEMENTWISE_KERNEL(sk_sub, -)
SCALAR_ELEMENTWISE_KERNEL(sk_and, &)
SCALAR_ELEMENTWISE_KERNEL(sk_or,  |)
SCALAR_ELEMENTWISE_KERNEL(sk_xor, ^)

// Shifts requieren conversion de uint32_t para vsrl
static inline void sk_sll(volatile int32_t *a, volatile int32_t *b,
                           volatile int32_t *out, int n) {
    for (int i = 0; i < n; i++)
        out[i] = (int32_t)((uint32_t)a[i] << (b[i] & 0x1F));
}
static inline void sk_srl(volatile int32_t *a, volatile int32_t *b,
                           volatile int32_t *out, int n) {
    for (int i = 0; i < n; i++)
        out[i] = (int32_t)((uint32_t)a[i] >> (b[i] & 0x1F));
}

static inline void sk_mul(volatile int32_t *a, volatile int32_t *b,
                           volatile int32_t *out, int n) {
    for (int i = 0; i < n; i++) out[i] = a[i] * b[i];
}

static inline int32_t sk_dot(volatile int32_t *a, volatile int32_t *b, int n) {
    int32_t acc = 0;
    for (int i = 0; i < n; i++) acc += a[i] * b[i];
    return acc;
}

static inline void sk_store_constant(volatile int32_t *out, int32_t value, int n) {
    for (int i = 0; i < n; i++) out[i] = value;
}

#endif // VPU_KERNELS_H
