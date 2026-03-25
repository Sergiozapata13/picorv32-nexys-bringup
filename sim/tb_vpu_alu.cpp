// =============================================================================
//  tb_vpu_alu.cpp — Testbench para vpu_alu.v (OE2)
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Compilar:
//    verilator --cc --exe --build --trace \
//      -Wno-UNUSEDSIGNAL -Wno-CASEINCOMPLETE -Wno-WIDTHTRUNC \
//      vpu_alu.v tb_vpu_alu.cpp -o sim_vpu_alu
//    ./obj_dir/sim_vpu_alu
// =============================================================================

#include "Vvpu_alu.h"
#include "verilated.h"
#include <cstdio>
#include <cstring>

#define GREEN  "\033[32m"
#define RED    "\033[31m"
#define RESET  "\033[0m"

static int tests_run = 0, tests_passed = 0;

static void check(const char* name, uint32_t got, uint32_t expected) {
    tests_run++;
    if (got == expected) {
        printf(GREEN "[PASS]" RESET " %s\n", name);
        tests_passed++;
    } else {
        printf(RED "[FAIL]" RESET " %s: got 0x%08X, expected 0x%08X\n",
               name, got, expected);
    }
}

static void tick(Vvpu_alu* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

// -----------------------------------------------------------------------------
//  Acceso al banco via puertos de debug
// -----------------------------------------------------------------------------
static uint32_t read_vreg(Vvpu_alu* dut, int reg, int elem) {
    dut->dbg_reg_sel  = reg;
    dut->dbg_elem_sel = elem;
    dut->dbg_we       = 0;
    dut->eval();
    return dut->dbg_rdata;
}

// Escribe UN elemento — hace tick para que quede registrado
static void write_vreg_elem(Vvpu_alu* dut, int reg, int elem, uint32_t val) {
    dut->dbg_reg_sel  = reg;
    dut->dbg_elem_sel = elem;
    dut->dbg_we       = 1;
    dut->dbg_wdata    = val;
    tick(dut);
    dut->dbg_we = 0;
    dut->eval();
}

// Carga 4 elementos en un registro + 1 tick extra para que el banco
// sea visible antes de presentar la instruccion
static void load_vreg(Vvpu_alu* dut, int reg,
                      uint32_t e0, uint32_t e1, uint32_t e2, uint32_t e3) {
    write_vreg_elem(dut, reg, 0, e0);
    write_vreg_elem(dut, reg, 1, e1);
    write_vreg_elem(dut, reg, 2, e2);
    write_vreg_elem(dut, reg, 3, e3);
    // Ciclo extra de estabilizacion
    tick(dut);
}

// -----------------------------------------------------------------------------
//  Encoding instrucciones VALU .vv
//  El modulo extrae vs2 de [22:20], vs1 de [17:15], vd de [9:7]
//  Colocamos los valores en esas posiciones exactas
// -----------------------------------------------------------------------------
static uint32_t encode_valu(uint32_t funct6, int vm,
                             int vd, int vs2, int vs1) {
    return ((funct6 & 0x3F) << 26) |
           ((vm     & 0x1)  << 25) |
           ((vs2    & 0x7)  << 20) |
           ((vs1    & 0x7)  << 15) |
           (0b000            << 12) |
           ((vd     & 0x7)   << 7)  |
           0b1010111;
}

#define FUNCT6_VADD  0b000000
#define FUNCT6_VSUB  0b000010
#define FUNCT6_VAND  0b001001
#define FUNCT6_VOR   0b001010
#define FUNCT6_VXOR  0b001011
#define FUNCT6_VSLL  0b100101
#define FUNCT6_VSRL  0b101000

// -----------------------------------------------------------------------------
//  run_valu — espera que la FSM este en IDLE, presenta instruccion,
//  espera pcpi_ready
// -----------------------------------------------------------------------------
static void run_valu(Vvpu_alu* dut, uint32_t insn) {
    // Asegurar que la FSM esta en IDLE (pcpi_wait=0)
    dut->pcpi_valid = 0;
    dut->eval();
    int guard = 5;
    while (dut->pcpi_wait && guard-- > 0) tick(dut);

    // Presentar instruccion
    dut->pcpi_valid = 1;
    dut->pcpi_insn  = insn;
    dut->eval();

    // Esperar ready
    int timeout = 10;
    while (!dut->pcpi_ready && timeout-- > 0) tick(dut);
    if (dut->pcpi_ready) tick(dut);

    dut->pcpi_valid = 0;
    tick(dut);
    tick(dut); // ciclo extra para estabilizar
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vvpu_alu* dut = new Vvpu_alu;

    printf("=== Testbench: vpu_alu (OE2 - VALU vectorial) ===\n\n");

    // Reset
    dut->resetn       = 0;
    dut->pcpi_valid   = 0;
    dut->pcpi_insn    = 0;
    dut->pcpi_rs1     = 0;
    dut->pcpi_rs2     = 0;
    dut->csr_vl       = 4;
    dut->csr_vtype    = 0x010;
    dut->dbg_we       = 0;
    dut->dbg_wdata    = 0;
    dut->dbg_reg_sel  = 0;
    dut->dbg_elem_sel = 0;
    tick(dut); tick(dut);
    dut->resetn = 1;
    tick(dut);

    // ------------------------------------------------------------------
    //  TEST 1: pcpi_wait combinacional
    // ------------------------------------------------------------------
    printf("--- Test 1: pcpi_wait combinacional ---\n");
    {
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = encode_valu(FUNCT6_VADD, 1, 1, 2, 3);
        dut->eval();
        check("pcpi_wait=1 en ciclo 0",  dut->pcpi_wait,  1);
        check("pcpi_ready=0 en ciclo 0", dut->pcpi_ready, 0);
        int t = 6; while (!dut->pcpi_ready && t-->0) tick(dut);
        if (dut->pcpi_ready) tick(dut);
        dut->pcpi_valid = 0;
        tick(dut); tick(dut);
    }

    // ------------------------------------------------------------------
    //  TEST 2: vadd.vv
    //  v2=[10,20,30,40] + v3=[1,2,3,4] = v1=[11,22,33,44]
    // ------------------------------------------------------------------
    printf("\n--- Test 2: vadd.vv v1, v2, v3 ---\n");
    {
        dut->csr_vl = 4;
        load_vreg(dut, 2, 10, 20, 30, 40);
        load_vreg(dut, 3,  1,  2,  3,  4);
        run_valu(dut, encode_valu(FUNCT6_VADD, 1, 1, 2, 3));

        uint32_t exp[] = {11, 22, 33, 44};
        char name[64];
        for (int i = 0; i < 4; i++) {
            snprintf(name, sizeof(name), "v1[%d] = %u", i, exp[i]);
            check(name, read_vreg(dut, 1, i), exp[i]);
        }
    }

    // ------------------------------------------------------------------
    //  TEST 3: vsub.vv
    //  v2=[100,200,300,400] - v3=[10,20,30,40] = v1=[90,180,270,360]
    // ------------------------------------------------------------------
    printf("\n--- Test 3: vsub.vv v1, v2, v3 ---\n");
    {
        load_vreg(dut, 2, 100, 200, 300, 400);
        load_vreg(dut, 3,  10,  20,  30,  40);
        run_valu(dut, encode_valu(FUNCT6_VSUB, 1, 1, 2, 3));

        uint32_t exp[] = {90, 180, 270, 360};
        char name[64];
        for (int i = 0; i < 4; i++) {
            snprintf(name, sizeof(name), "v1[%d] = %u", i, exp[i]);
            check(name, read_vreg(dut, 1, i), exp[i]);
        }
    }

    // ------------------------------------------------------------------
    //  TEST 4: vand.vv
    // ------------------------------------------------------------------
    printf("\n--- Test 4: vand.vv ---\n");
    {
        load_vreg(dut, 2, 0xFF00FF00, 0xAAAAAAAA, 0xF0F0F0F0, 0xFFFFFFFF);
        load_vreg(dut, 3, 0x00FF00FF, 0x55555555, 0x0F0F0F0F, 0x0000FFFF);
        run_valu(dut, encode_valu(FUNCT6_VAND, 1, 1, 2, 3));
        check("vand[0]=0x00000000", read_vreg(dut,1,0), 0x00000000);
        check("vand[1]=0x00000000", read_vreg(dut,1,1), 0x00000000);
        check("vand[2]=0x00000000", read_vreg(dut,1,2), 0x00000000);
        check("vand[3]=0x0000FFFF", read_vreg(dut,1,3), 0x0000FFFF);
    }

    // ------------------------------------------------------------------
    //  TEST 5: vor.vv
    // ------------------------------------------------------------------
    printf("\n--- Test 5: vor.vv ---\n");
    {
        load_vreg(dut, 2, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
        load_vreg(dut, 3, 0x00FFFFFF, 0xFF00FFFF, 0xFFFF00FF, 0xFFFFFF00);
        run_valu(dut, encode_valu(FUNCT6_VOR, 1, 1, 2, 3));
        check("vor[0]=0xFFFFFFFF", read_vreg(dut,1,0), 0xFFFFFFFF);
        check("vor[1]=0xFFFFFFFF", read_vreg(dut,1,1), 0xFFFFFFFF);
        check("vor[2]=0xFFFFFFFF", read_vreg(dut,1,2), 0xFFFFFFFF);
        check("vor[3]=0xFFFFFFFF", read_vreg(dut,1,3), 0xFFFFFFFF);
    }

    // ------------------------------------------------------------------
    //  TEST 6: vxor.vv
    // ------------------------------------------------------------------
    printf("\n--- Test 6: vxor.vv ---\n");
    {
        load_vreg(dut, 2, 0xFFFFFFFF, 0xAAAAAAAA, 0x0F0F0F0F, 0x12345678);
        load_vreg(dut, 3, 0xFFFFFFFF, 0x55555555, 0xF0F0F0F0, 0x12345678);
        run_valu(dut, encode_valu(FUNCT6_VXOR, 1, 1, 2, 3));
        check("vxor[0]=0x00000000", read_vreg(dut,1,0), 0x00000000);
        check("vxor[1]=0xFFFFFFFF", read_vreg(dut,1,1), 0xFFFFFFFF);
        check("vxor[2]=0xFFFFFFFF", read_vreg(dut,1,2), 0xFFFFFFFF);
        check("vxor[3]=0x00000000", read_vreg(dut,1,3), 0x00000000);
    }

    // ------------------------------------------------------------------
    //  TEST 7: vsll.vv
    // ------------------------------------------------------------------
    printf("\n--- Test 7: vsll.vv ---\n");
    {
        load_vreg(dut, 2, 0x00000001, 0x00000001, 0xFFFFFFFF, 0x00000001);
        load_vreg(dut, 3,          4,           8,           8,          31);
        run_valu(dut, encode_valu(FUNCT6_VSLL, 1, 1, 2, 3));
        check("vsll[0]=0x00000010", read_vreg(dut,1,0), 0x00000010);
        check("vsll[1]=0x00000100", read_vreg(dut,1,1), 0x00000100);
        check("vsll[2]=0xFFFFFF00", read_vreg(dut,1,2), 0xFFFFFF00);
        check("vsll[3]=0x80000000", read_vreg(dut,1,3), 0x80000000);
    }

    // ------------------------------------------------------------------
    //  TEST 7b: vsrl.vv
    // ------------------------------------------------------------------
    printf("\n--- Test 7b: vsrl.vv ---\n");
    {
        load_vreg(dut, 2, 0x80000000, 0x80000000, 0xFFFFFF00, 0x80000000);
        load_vreg(dut, 3,          4,           8,           8,          31);
        run_valu(dut, encode_valu(FUNCT6_VSRL, 1, 1, 2, 3));
        check("vsrl[0]=0x08000000", read_vreg(dut,1,0), 0x08000000);
        check("vsrl[1]=0x00800000", read_vreg(dut,1,1), 0x00800000);
        check("vsrl[2]=0x00FFFFFF", read_vreg(dut,1,2), 0x00FFFFFF);
        check("vsrl[3]=0x00000001", read_vreg(dut,1,3), 0x00000001);
    }

    // ------------------------------------------------------------------
    //  TEST 8: vl=3 — tail intacto
    // ------------------------------------------------------------------
    printf("\n--- Test 8: vl=3 (tail intacto) ---\n");
    {
        dut->csr_vl = 3;
        load_vreg(dut, 1, 0, 0, 0, 0xDEADBEEF);
        load_vreg(dut, 2, 1, 2, 3, 99);
        load_vreg(dut, 3, 1, 1, 1, 99);
        run_valu(dut, encode_valu(FUNCT6_VADD, 1, 1, 2, 3));
        check("vl=3: v1[0]=2",                      read_vreg(dut,1,0), 2);
        check("vl=3: v1[1]=3",                      read_vreg(dut,1,1), 3);
        check("vl=3: v1[2]=4",                      read_vreg(dut,1,2), 4);
        check("vl=3: v1[3]=0xDEADBEEF (intacto)",   read_vreg(dut,1,3), 0xDEADBEEF);
        dut->csr_vl = 4;
    }

    // ------------------------------------------------------------------
    //  TEST 9: mascara v0 (vm=0)
    //  v0[elem_i] = bit0 de vreg[0][32*i] — 1=activo, 0=inactivo
    // ------------------------------------------------------------------
    printf("\n--- Test 9: vadd.vv con mascara vm=0 ---\n");
    {
        dut->csr_vl = 4;
        load_vreg(dut, 0, 1, 0, 1, 0);          // mascara: activo/inactivo
        load_vreg(dut, 1, 0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD); // valor previo vd
        load_vreg(dut, 2, 10, 20, 30, 40);
        load_vreg(dut, 3,  1,  2,  3,  4);
        run_valu(dut, encode_valu(FUNCT6_VADD, 0, 1, 2, 3)); // vm=0
        check("mascara: v1[0]=11 (activo)",        read_vreg(dut,1,0), 11);
        check("mascara: v1[1]=0xBBBB (inactivo)",  read_vreg(dut,1,1), 0xBBBB);
        check("mascara: v1[2]=33 (activo)",        read_vreg(dut,1,2), 33);
        check("mascara: v1[3]=0xDDDD (inactivo)",  read_vreg(dut,1,3), 0xDDDD);
    }

    // ------------------------------------------------------------------
    //  TEST 10: instruccion no VALU — no modifica banco, wait=0
    // ------------------------------------------------------------------
    printf("\n--- Test 10: instruccion no VALU ---\n");
    {
        // Asegurar IDLE
        dut->pcpi_valid = 0;
        tick(dut); tick(dut);

        dut->pcpi_valid = 1;
        dut->pcpi_insn  = 0x00208033; // add x0,x1,x2 — opcode 0110011
        dut->eval();
        check("pcpi_wait=0 para no-VALU",  dut->pcpi_wait,  0);
        tick(dut);
        check("pcpi_ready=0 para no-VALU", dut->pcpi_ready, 0);
        dut->pcpi_valid = 0;
        tick(dut);
    }

    // ------------------------------------------------------------------
    //  TEST 11: overflow 32 bits
    // ------------------------------------------------------------------
    printf("\n--- Test 11: overflow 32 bits ---\n");
    {
        dut->csr_vl = 4;
        load_vreg(dut, 2, 0xFFFFFFFF, 0x80000000, 0x00000001, 0x7FFFFFFF);
        load_vreg(dut, 3, 0x00000001, 0x80000000, 0xFFFFFFFF, 0x00000001);
        run_valu(dut, encode_valu(FUNCT6_VADD, 1, 1, 2, 3));
        check("0xFFFFFFFF+1=0",           read_vreg(dut,1,0), 0x00000000);
        check("0x80000000+0x80000000=0",  read_vreg(dut,1,1), 0x00000000);
        check("0x00000001+0xFFFFFFFF=0",  read_vreg(dut,1,2), 0x00000000);
        check("0x7FFFFFFF+1=0x80000000",  read_vreg(dut,1,3), 0x80000000);
    }
    // ------------------------------------------------------------------
    //  TEST 12: vmul.vv — multiplicacion elemento a elemento
    //  funct3=010 (OPMVV), funct6=100101
    //  vd=v1, vs2=v2, vs1=v3, vm=1
    //  encoding: (100101<<26)|(1<<25)|(2<<20)|(3<<15)|(010<<12)|(1<<7)|1010111
    //            = 0x9621A0D7
    // ------------------------------------------------------------------
    printf("\n--- Test 12: vmul.vv ---\n");
    {
        dut->csr_vl = 4;
        load_vreg(dut, 2, 2, 3, 4, 5);
        load_vreg(dut, 3, 10, 10, 10, 10);
        run_valu(dut, (0b100101<<26)|(1<<25)|(2<<20)|(3<<15)|(0b010<<12)|(1<<7)|0b1010111);

        check("vmul[0] = 2*10 = 20",   read_vreg(dut,1,0), 20);
        check("vmul[1] = 3*10 = 30",   read_vreg(dut,1,1), 30);
        check("vmul[2] = 4*10 = 40",   read_vreg(dut,1,2), 40);
        check("vmul[3] = 5*10 = 50",   read_vreg(dut,1,3), 50);
    }
    {
        // overflow 32 bits
        load_vreg(dut, 2, 0xFFFFFFFF, 0x10000, 3, 0);
        load_vreg(dut, 3, 2,          0x10000, 3, 0);
        run_valu(dut, (0b100101<<26)|(1<<25)|(2<<20)|(3<<15)|(0b010<<12)|(1<<7)|0b1010111);

        check("vmul overflow: 0xFFFFFFFF*2 = 0xFFFFFFFE",
              read_vreg(dut,1,0), 0xFFFFFFFE);
        check("vmul: 0x10000*0x10000 = 0x00000000 (overflow 32b)",
              read_vreg(dut,1,1), 0x00000000);
        check("vmul: 3*3 = 9",
              read_vreg(dut,1,2), 9);
    }

    // ------------------------------------------------------------------
    //  TEST 13: vredsum.vs — reduccion suma
    //  funct3=010 (OPMVV), funct6=000000
    //  vd[0] = vs1[0] + vs2[0] + vs2[1] + vs2[2] + vs2[3]  (si vl=4)
    //  elementos vd[1..3] NO se modifican
    // ------------------------------------------------------------------
    printf("\n--- Test 13: vredsum.vs ---\n");
    {
        dut->csr_vl = 4;
        // v2 = [1, 2, 3, 4]  (vector a reducir)
        // v3 = [100, 99, 98, 97]  (acumulador — solo v3[0]=100 se usa)
        load_vreg(dut, 2, 1, 2, 3, 4);
        load_vreg(dut, 3, 100, 99, 98, 97);
        // v1 antes = [0xDEAD, 0xBEEF, 0xCAFE, 0xBABE]
        load_vreg(dut, 1, 0xDEAD, 0xBEEF, 0xCAFE, 0xBABE);

        run_valu(dut, (0b000000<<26)|(1<<25)|(2<<20)|(3<<15)|(0b010<<12)|(1<<7)|0b1010111);

        // vd[0] = vs1[0] + sum(vs2) = 100 + 1+2+3+4 = 110
        check("vredsum[0] = 100+1+2+3+4 = 110",  read_vreg(dut,1,0), 110);
        // vd[1..3] no deben cambiar
        check("vredsum[1] = 0xBEEF (intacto)",    read_vreg(dut,1,1), 0xBEEF);
        check("vredsum[2] = 0xCAFE (intacto)",    read_vreg(dut,1,2), 0xCAFE);
        check("vredsum[3] = 0xBABE (intacto)",    read_vreg(dut,1,3), 0xBABE);
    }
    {
        // vredsum con vl=3 — solo suma 3 elementos
        dut->csr_vl = 3;
        load_vreg(dut, 2, 10, 20, 30, 99); // vs2[3]=99 no debe sumarse
        load_vreg(dut, 3, 0, 0, 0, 0);     // acumulador = 0

        run_valu(dut, (0b000000<<26)|(1<<25)|(2<<20)|(3<<15)|(0b010<<12)|(1<<7)|0b1010111);

        check("vredsum vl=3: 0+10+20+30 = 60", read_vreg(dut,1,0), 60);
        dut->csr_vl = 4;
    }
    {
        // vredsum util para dot product parcial: acumula en vs1[0]
        // simula: acc = acc + sum(v_prod)
        load_vreg(dut, 2, 5, 5, 5, 5);   // productos = [5,5,5,5]
        load_vreg(dut, 3, 20, 0, 0, 0);  // acc[0] = 20
        load_vreg(dut, 1, 0, 0, 0, 0);

        run_valu(dut, (0b000000<<26)|(1<<25)|(2<<20)|(3<<15)|(0b010<<12)|(1<<7)|0b1010111);

        check("vredsum dot parcial: 20+5+5+5+5 = 40", read_vreg(dut,1,0), 40);
    }


    // ------------------------------------------------------------------
    //  Resumen
    // ------------------------------------------------------------------
    printf("\n==========================================\n");
    printf("Resultados: %d/%d tests pasaron\n", tests_passed, tests_run);
    if (tests_passed == tests_run)
        printf(GREEN "✓ TODOS LOS TESTS PASARON - OE2 lista para HW\n" RESET);
    else
        printf(RED "✗ HAY FALLAS\n" RESET);
    printf("==========================================\n");

    delete dut;
    return (tests_passed == tests_run) ? 0 : 1;
}
