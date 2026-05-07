// =============================================================================
//  vpu_asm.h — Encodings .word de las instrucciones RVV-lite
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  GCC con -march=rv32i no reconoce las instrucciones vectoriales,
//  por lo que se emiten directamente con .word <encoding hexadecimal>.
//
//  Este header centraliza todos los encodings para que cada main_*.c
//  pueda incluirlos sin reproducir la documentacion de bits.
//
//  Los encodings asumen los siguientes registros base:
//    a0 (x10): puntero/escalar primario
//    a1 (x11): puntero/escalar secundario
// =============================================================================

#ifndef VPU_ASM_H
#define VPU_ASM_H

// ─── Configuracion vectorial ─────────────────────────────────────────────────
// vsetvli a0, a1, e32m1
//   funct3=111, zimm[10:0]=0x010 (SEW=32, LMUL=1)
//   rs1=a1, rd=a0
#define ASM_VSETVLI_A0_A1_E32M1   ".word 0x0105F557\n"

// ─── Loads/Stores vectoriales ────────────────────────────────────────────────
// vle32.v v1, (a0)   = 0x02056087
// vle32.v v2, (a0)   = 0x02056107
// vle32.v v3, (a1)   = 0x0205E187   ← usa a1 como base (HT-OE3c)
// vle32.v v4, (a0)   = 0x02056207
// vse32.v v1, (a0)   = 0x020560A7
// vse32.v v2, (a0)   = 0x02056127
// vse32.v v4, (a0)   = 0x02056227
#define ASM_VLE32_V1_A0   ".word 0x02056087\n"
#define ASM_VLE32_V2_A0   ".word 0x02056107\n"
#define ASM_VLE32_V3_A1   ".word 0x0205E187\n"
#define ASM_VLE32_V4_A0   ".word 0x02056207\n"
#define ASM_VSE32_V1_A0   ".word 0x020560A7\n"
#define ASM_VSE32_V2_A0   ".word 0x02056127\n"
#define ASM_VSE32_V4_A0   ".word 0x02056227\n"

// ─── Movimientos escalar-vector ──────────────────────────────────────────────
// vmv.v.x v1, a0  = 0x5E0550D7
// vmv.v.x v2, a0  = 0x5E055157
// vmv.v.x v4, a0  = 0x5E055257
// vmv.x.s a0, v1  = 0x42102557
// vmv.x.s a0, v4  = 0x42402557
#define ASM_VMV_V_X_V1_A0  ".word 0x5E0550D7\n"
#define ASM_VMV_V_X_V2_A0  ".word 0x5E055157\n"
#define ASM_VMV_V_X_V4_A0  ".word 0x5E055257\n"
#define ASM_VMV_X_S_A0_V1  ".word 0x42102557\n"
#define ASM_VMV_X_S_A0_V4  ".word 0x42402557\n"

// ─── VALU — operaciones vd, vs2, vs1 (formato OPIVV) ─────────────────────────
// Plantilla: funct6 + 1 + vs2 + vs1 + 000 + vd + 1010111
// vadd.vv v1, v2, v3  = 0x022180D7   (funct6=000000)
// vsub.vv v1, v2, v3  = 0x0A2180D7   (funct6=000010)
// vand.vv v1, v2, v3  = 0x262180D7   (funct6=001001)
// vor.vv  v1, v2, v3  = 0x2A2180D7   (funct6=001010)
// vxor.vv v1, v2, v3  = 0x2E2180D7   (funct6=001011)
// vsll.vv v1, v2, v3  = 0x962180D7   (funct6=100101)
// vsrl.vv v1, v2, v3  = 0xA22180D7   (funct6=101000)
#define ASM_VADD_VV_V1_V2_V3  ".word 0x022180D7\n"
#define ASM_VSUB_VV_V1_V2_V3  ".word 0x0A2180D7\n"
#define ASM_VAND_VV_V1_V2_V3  ".word 0x262180D7\n"
#define ASM_VOR_VV_V1_V2_V3   ".word 0x2A2180D7\n"
#define ASM_VXOR_VV_V1_V2_V3  ".word 0x2E2180D7\n"
#define ASM_VSLL_VV_V1_V2_V3  ".word 0x962180D7\n"
#define ASM_VSRL_VV_V1_V2_V3  ".word 0xA22180D7\n"

// ─── VALU — multiplicacion y reduccion (formato OPMVV, funct3=010) ───────────
// vmul.vv v1, v2, v3      = 0x9621A0D7
// vredsum.vs v4, v1, v4   = 0x02122257
#define ASM_VMUL_VV_V1_V2_V3   ".word 0x9621A0D7\n"
#define ASM_VREDSUM_VS_V4_V1   ".word 0x02122257\n"

// ─── Helpers de patrones comunes ─────────────────────────────────────────────

// Inicializar v4 = [0,0,0,0] como acumulador
// Costo: 2 instrucciones (li + vmv.v.x)
#define ASM_INIT_V4_ZERO        \
    "li a0, 0\n"                \
    ASM_VMV_V_X_V4_A0

// Patron de carga doble: v2 desde a0, v3 desde a1
// Asume que a0 y a1 ya tienen las direcciones base
#define ASM_LOAD_V2_V3          \
    ASM_VLE32_V2_A0             \
    ASM_VLE32_V3_A1

#endif // VPU_ASM_H
