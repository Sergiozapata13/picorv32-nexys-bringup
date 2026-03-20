// =============================================================================
//  tb_vpu_decode.cpp — Testbench para vpu_decode.v (OE1)
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Compilar:
//    verilator --cc --exe --build --trace \
//      -Wno-UNUSEDSIGNAL -Wno-CASEINCOMPLETE \
//      vpu_decode.v tb_vpu_decode.cpp -o sim_vpu_decode
//    ./obj_dir/sim_vpu_decode
// =============================================================================

#include "Vvpu_decode.h"
#include "verilated.h"
#include <cstdio>
#include <cstdlib>

#define GREEN  "\033[32m"
#define RED    "\033[31m"
#define YELLOW "\033[33m"
#define RESET  "\033[0m"

static int tests_run    = 0;
static int tests_passed = 0;

static void check(const char* name, uint32_t got, uint32_t expected) {
    tests_run++;
    if (got == expected) {
        printf(GREEN "[PASS]" RESET " %s: got 0x%08X\n", name, got);
        tests_passed++;
    } else {
        printf(RED "[FAIL]" RESET " %s: got 0x%08X, expected 0x%08X\n",
               name, got, expected);
    }
}

static void tick(Vvpu_decode* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

// -----------------------------------------------------------------------------
//  Helpers de encoding
//
//  vsetvli rd, rs1, e32m1
//    opcode=1010111, funct3=111, insn[31]=0
//    zimm para e32,m1: vsew=010, vlmul=000 → zimm = 0b00000010000 = 0x010
// -----------------------------------------------------------------------------
static uint32_t encode_vsetvli(int rd, int rs1, uint32_t zimm = 0x010) {
    return (0b0 << 31) |
           ((zimm & 0x7FF) << 20) |
           ((rs1  & 0x1F)  << 15) |
           (0b111           << 12) |
           ((rd   & 0x1F)   << 7)  |
           0b1010111;
}

// vsetvl rd, rs1, rs2
//   insn[31:30]=11, insn[29:25]=00000
static uint32_t encode_vsetvl(int rd, int rs1, int rs2) {
    return (0b11    << 30) |
           (0b00000 << 25) |
           ((rs2 & 0x1F) << 20) |
           ((rs1 & 0x1F) << 15) |
           (0b111         << 12) |
           ((rd  & 0x1F)  << 7)  |
           0b1010111;
}

// -----------------------------------------------------------------------------
//  run_vset — presenta instruccion y espera pcpi_ready
//  Retorna el valor de pcpi_rd cuando ready=1
// -----------------------------------------------------------------------------
static uint32_t run_vset(Vvpu_decode* dut,
                          uint32_t insn, uint32_t rs1, uint32_t rs2 = 0) {
    dut->pcpi_valid = 1;
    dut->pcpi_insn  = insn;
    dut->pcpi_rs1   = rs1;
    dut->pcpi_rs2   = rs2;
    dut->eval();

    uint32_t result = 0xDEADBEEF;
    int timeout = 10;
    while (timeout-- > 0) {
        tick(dut);
        if (dut->pcpi_ready) {
            result = dut->pcpi_rd;
            break;
        }
    }
    dut->pcpi_valid = 0;
    tick(dut);
    return result;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vvpu_decode* dut = new Vvpu_decode;

    printf("=== Testbench: vpu_decode (OE1 — vsetvli/vsetvl) ===\n\n");

    // Reset
    dut->resetn     = 0;
    dut->pcpi_valid = 0;
    dut->pcpi_insn  = 0;
    dut->pcpi_rs1   = 0;
    dut->pcpi_rs2   = 0;
    tick(dut); tick(dut);
    dut->resetn = 1;
    tick(dut);

    // ------------------------------------------------------------------
    //  TEST 1: pcpi_wait combinacional en ciclo 0
    // ------------------------------------------------------------------
    printf("--- Test 1: pcpi_wait combinacional ---\n");
    {
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = encode_vsetvli(10, 10); // vsetvli a0, a0, e32m1
        dut->pcpi_rs1   = 4;
        dut->eval();

        check("pcpi_wait en ciclo 0 (combinacional)", dut->pcpi_wait,  1);
        check("pcpi_ready en ciclo 0 debe ser 0",     dut->pcpi_ready, 0);

        // Completar la instruccion
        int timeout = 5;
        while (!dut->pcpi_ready && timeout-- > 0) tick(dut);
        if (dut->pcpi_ready) tick(dut);
        dut->pcpi_valid = 0;
        tick(dut);
    }

    // ------------------------------------------------------------------
    //  TEST 2: vsetvli con rs1 < VLMAX — caso normal
    //  Pedir 3 elementos, VLMAX=4 → vl=3
    // ------------------------------------------------------------------
    printf("\n--- Test 2: vsetvli rs1=3 < VLMAX=4 ---\n");
    {
        uint32_t rd_val = run_vset(dut, encode_vsetvli(10, 10), 3);
        check("rd = 3 (vl confirmado)",   rd_val,         3);
        check("csr_vl = 3",               dut->csr_vl,    3);
        check("csr_vtype valido (e32m1)", dut->csr_vtype, 0x00000010);
    }

    // ------------------------------------------------------------------
    //  TEST 3: vsetvli con rs1 > VLMAX — saturacion
    //  Pedir 10 elementos, VLMAX=4 → vl=4
    // ------------------------------------------------------------------
    printf("\n--- Test 3: vsetvli rs1=10 > VLMAX=4 (saturacion) ---\n");
    {
        uint32_t rd_val = run_vset(dut, encode_vsetvli(10, 10), 10);
        check("rd = 4 (saturado a VLMAX)", rd_val,      4);
        check("csr_vl = 4",               dut->csr_vl, 4);
    }

    // ------------------------------------------------------------------
    //  TEST 4: vsetvli con rs1 == VLMAX — caso exacto
    // ------------------------------------------------------------------
    printf("\n--- Test 4: vsetvli rs1=4 == VLMAX ---\n");
    {
        uint32_t rd_val = run_vset(dut, encode_vsetvli(10, 10), 4);
        check("rd = 4", rd_val,      4);
        check("csr_vl = 4",          dut->csr_vl, 4);
    }

    // ------------------------------------------------------------------
    //  TEST 5: vsetvli con rs1=0 — consulta de VLMAX sin cambiar vl
    //  vl debe quedar con el valor anterior (4 del test anterior)
    // ------------------------------------------------------------------
    printf("\n--- Test 5: vsetvli rs1=0 (consulta VLMAX, vl no cambia) ---\n");
    {
        // Primero establecer vl=3
        run_vset(dut, encode_vsetvli(10, 10), 3);
        uint32_t vl_before = dut->csr_vl;
        check("vl antes = 3", vl_before, 3);

        // Ahora consultar con rs1=0
        uint32_t rd_val = run_vset(dut, encode_vsetvli(10, 10), 0);
        check("rd = VLMAX = 4",       rd_val,      4);
        check("csr_vl NO cambio = 3", dut->csr_vl, 3); // debe seguir siendo 3
    }

    // ------------------------------------------------------------------
    //  TEST 6: vsetvli con configuracion ilegal (e64 no soportado)
    //  zimm para e64,m1: vsew=011 → zimm = 0b00000011000 = 0x018
    // ------------------------------------------------------------------
    printf("\n--- Test 6: vsetvli e64 (configuracion ilegal -> vill=1) ---\n");
    {
        uint32_t zimm_e64 = 0x018; // vsew=011 = e64
        uint32_t rd_val = run_vset(dut, encode_vsetvli(10, 10, zimm_e64), 4);
        // Cuando vill=1: vtype[31]=1, vl=0
        check("csr_vtype vill=1 (bit31)", dut->csr_vtype, 0x80000000);
    }

    // ------------------------------------------------------------------
    //  TEST 7: vsetvl — vtype viene del registro rs2
    //  rs2 = 0x010 (e32,m1) — mismo que el zimm de vsetvli
    // ------------------------------------------------------------------
    printf("\n--- Test 7: vsetvl rd=a0, rs1=a0, rs2=a1 (vtype en registro) ---\n");
    {
        uint32_t vtype_e32m1 = 0x010; // vsew=010, vlmul=000
        uint32_t rd_val = run_vset(dut,
                                    encode_vsetvl(10, 10, 11), // a0,a0,a1
                                    4,          // rs1 = 4 elementos
                                    vtype_e32m1); // rs2 = vtype
        check("rd = 4",                 rd_val,         4);
        check("csr_vl = 4",             dut->csr_vl,    4);
        check("csr_vtype = 0x010",      dut->csr_vtype, 0x010);
    }

    // ------------------------------------------------------------------
    //  TEST 8: instruccion no RVV — no debe activar la VPU
    // ------------------------------------------------------------------
    printf("\n--- Test 8: instruccion no RVV (opcode diferente) ---\n");
    {
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = 0x00208033; // add x0, x1, x2
        dut->pcpi_rs1   = 4;
        dut->eval();
        check("pcpi_wait=0 para no-RVV",  dut->pcpi_wait,  0);
        check("pcpi_ready=0 para no-RVV", dut->pcpi_ready, 0);
        dut->pcpi_valid = 0;
        tick(dut);
    }

    // ------------------------------------------------------------------
    //  TEST 9: instrucciones consecutivas — vl cambia correctamente
    // ------------------------------------------------------------------
    printf("\n--- Test 9: vsetvli consecutivas ---\n");
    {
        run_vset(dut, encode_vsetvli(10, 10), 2);
        check("vl = 2 tras primera", dut->csr_vl, 2);

        run_vset(dut, encode_vsetvli(10, 10), 4);
        check("vl = 4 tras segunda", dut->csr_vl, 4);

        run_vset(dut, encode_vsetvli(10, 10), 1);
        check("vl = 1 tras tercera", dut->csr_vl, 1);
    }

    // ------------------------------------------------------------------
    //  Resumen
    // ------------------------------------------------------------------
    printf("\n==========================================\n");
    printf("Resultados: %d/%d tests pasaron\n", tests_passed, tests_run);
    if (tests_passed == tests_run)
        printf(GREEN "✓ TODOS LOS TESTS PASARON — OE1 lista para HW\n" RESET);
    else
        printf(RED "✗ HAY FALLAS\n" RESET);
    printf("==========================================\n");

    delete dut;
    return (tests_passed == tests_run) ? 0 : 1;
}
