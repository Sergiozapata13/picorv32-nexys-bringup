// =============================================================================
//  tb_vpu_lsu.cpp v2 — Testbench para vpu_lsu.v (OE3)
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Modelo de RAM: mem_ready=1 combinacional cuando mem_valid=1
//  (latencia 0 ciclos — simplifica el testbench y evita carreras)
//
//  Compilar:
//    verilator --cc --exe --build --trace \
//      -Wno-UNUSEDSIGNAL -Wno-CASEINCOMPLETE -Wno-WIDTHEXPAND \
//      vpu_lsu.v tb_vpu_lsu.cpp -o sim_vpu_lsu
//    ./obj_dir/sim_vpu_lsu
// =============================================================================

#include "Vvpu_lsu.h"
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

// ─── Modelo de RAM (256 palabras x 32 bits) ──────────────────────────────────
static uint32_t ram[256] = {0};

static uint32_t ram_read(uint32_t addr)  { return ram[(addr>>2) & 0xFF]; }
static void ram_write(uint32_t addr, uint32_t data, uint8_t wstrb) {
    uint32_t idx = (addr>>2) & 0xFF;
    if (wstrb&1) ram[idx] = (ram[idx]&~0x000000FF)|(data&0x000000FF);
    if (wstrb&2) ram[idx] = (ram[idx]&~0x0000FF00)|(data&0x0000FF00);
    if (wstrb&4) ram[idx] = (ram[idx]&~0x00FF0000)|(data&0x00FF0000);
    if (wstrb&8) ram[idx] = (ram[idx]&~0xFF000000)|(data&0xFF000000);
}

// ─── Banco vectorial simulado ─────────────────────────────────────────────────
static uint32_t vreg[8][4] = {0};

// ─── Evaluar modelo combinacional de RAM y banco ──────────────────────────────
// Llamar despues de cualquier cambio en las salidas del DUT
static void eval_model(Vvpu_lsu* dut) {
    // Puerto de lectura del banco (combinacional)
    dut->vreg_rdata = vreg[dut->vreg_raddr][dut->vreg_relem];

    // Modelo de RAM: ready=1 combinacional cuando valid=1
    if (dut->lsu_mem_valid) {
        dut->lsu_mem_ready = 1;
        if (dut->lsu_mem_wstrb == 0) {
            // Lectura
            dut->lsu_mem_rdata = ram_read(dut->lsu_mem_addr);
        } else {
            // Escritura
            ram_write(dut->lsu_mem_addr, dut->lsu_mem_wdata, dut->lsu_mem_wstrb);
            dut->lsu_mem_rdata = 0;
        }
    } else {
        dut->lsu_mem_ready = 0;
        dut->lsu_mem_rdata = 0;
    }

    dut->eval();
}

// ─── Tick completo ────────────────────────────────────────────────────────────
static void tick(Vvpu_lsu* dut) {
    // Flanco de bajada — evaluar modelo antes
    dut->clk = 0;
    eval_model(dut);

    // Flanco de subida
    dut->clk = 1;
    dut->eval();

    // Post-flanco: actualizar banco si vreg_we=1
    if (dut->vreg_we) {
        vreg[dut->vreg_waddr][dut->vreg_welem] = dut->vreg_wdata;
    }

    // Re-evaluar modelo con nuevas salidas del DUT
    eval_model(dut);
}

// ─── Encodings ───────────────────────────────────────────────────────────────
static uint32_t encode_vle32(int vd, int rs1) {
    return (1<<25)|(0b00000<<20)|((rs1&0x1F)<<15)|(0b110<<12)|((vd&0x7)<<7)|0b0000111;
}
static uint32_t encode_vse32(int vs3, int rs1) {
    return (1<<25)|(0b00000<<20)|((rs1&0x1F)<<15)|(0b110<<12)|((vs3&0x7)<<7)|0b0100111;
}

// ─── Ejecutar instruccion LSU ─────────────────────────────────────────────────
static void run_lsu(Vvpu_lsu* dut, uint32_t insn, uint32_t rs1) {
    // Esperar IDLE
    dut->pcpi_valid = 0;
    eval_model(dut);
    int guard = 20;
    while (dut->pcpi_wait && guard-- > 0) tick(dut);

    // Presentar instruccion
    dut->pcpi_valid = 1;
    dut->pcpi_insn  = insn;
    dut->pcpi_rs1   = rs1;
    eval_model(dut);

    // Esperar ready
    int timeout = 100;
    while (!dut->pcpi_ready && timeout-- > 0) tick(dut);
    if (dut->pcpi_ready) tick(dut);

    dut->pcpi_valid = 0;
    tick(dut); tick(dut);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vvpu_lsu* dut = new Vvpu_lsu;

    printf("=== Testbench: vpu_lsu (OE3 — vle32/vse32) ===\n\n");

    // Reset
    dut->resetn        = 0;
    dut->pcpi_valid    = 0;
    dut->pcpi_insn     = 0;
    dut->pcpi_rs1      = 0;
    dut->pcpi_rs2      = 0;
    dut->csr_vl        = 4;
    dut->lsu_mem_ready = 0;
    dut->lsu_mem_rdata = 0;
    dut->vreg_rdata    = 0;
    tick(dut); tick(dut);
    dut->resetn = 1;
    tick(dut);

    // ─── TEST 1: pcpi_wait combinacional ─────────────────────────────────────
    printf("--- Test 1: pcpi_wait combinacional ---\n");
    {
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = encode_vle32(1, 10);
        dut->pcpi_rs1   = 0x100;
        eval_model(dut);
        check("pcpi_wait=1 en ciclo 0",  dut->pcpi_wait,  1);
        check("pcpi_ready=0 en ciclo 0", dut->pcpi_ready, 0);
        int t = 50; while (!dut->pcpi_ready && t-->0) tick(dut);
        if (dut->pcpi_ready) tick(dut);
        dut->pcpi_valid = 0;
        tick(dut); tick(dut);
    }

    // ─── TEST 2: vle32 — 4 elementos ─────────────────────────────────────────
    printf("\n--- Test 2: vle32.v v1, (a0) — 4 elementos desde RAM[0x00] ---\n");
    {
        dut->csr_vl = 4;
        ram[0]=0x11111111; ram[1]=0x22222222;
        ram[2]=0x33333333; ram[3]=0x44444444;

        run_lsu(dut, encode_vle32(1, 10), 0x00);

        check("vreg[1][0]=0x11111111", vreg[1][0], 0x11111111);
        check("vreg[1][1]=0x22222222", vreg[1][1], 0x22222222);
        check("vreg[1][2]=0x33333333", vreg[1][2], 0x33333333);
        check("vreg[1][3]=0x44444444", vreg[1][3], 0x44444444);
    }

    // ─── TEST 3: vle32 con vl=3 — tail intacto ───────────────────────────────
    printf("\n--- Test 3: vle32.v v2 — vl=3, tail intacto ---\n");
    {
        dut->csr_vl = 3;
        ram[0]=0xAAAA0000; ram[1]=0xBBBB0001;
        ram[2]=0xCCCC0002; ram[3]=0xDDDD0003;
        vreg[2][3] = 0xDEADBEEF;

        run_lsu(dut, encode_vle32(2, 10), 0x00);

        check("vreg[2][0]=0xAAAA0000",            vreg[2][0], 0xAAAA0000);
        check("vreg[2][1]=0xBBBB0001",            vreg[2][1], 0xBBBB0001);
        check("vreg[2][2]=0xCCCC0002",            vreg[2][2], 0xCCCC0002);
        check("vreg[2][3]=0xDEADBEEF (intacto)",  vreg[2][3], 0xDEADBEEF);
        dut->csr_vl = 4;
    }

    // ─── TEST 4: vse32 — 4 elementos ─────────────────────────────────────────
    printf("\n--- Test 4: vse32.v v3, (a0) — 4 elementos a RAM[0x40] ---\n");
    {
        dut->csr_vl = 4;
        vreg[3][0]=0xCAFE0001; vreg[3][1]=0xCAFE0002;
        vreg[3][2]=0xCAFE0003; vreg[3][3]=0xCAFE0004;
        ram[0x10]=ram[0x11]=ram[0x12]=ram[0x13]=0;

        run_lsu(dut, encode_vse32(3, 10), 0x40);

        check("RAM[0x40]=0xCAFE0001", ram[0x10], 0xCAFE0001);
        check("RAM[0x44]=0xCAFE0002", ram[0x11], 0xCAFE0002);
        check("RAM[0x48]=0xCAFE0003", ram[0x12], 0xCAFE0003);
        check("RAM[0x4C]=0xCAFE0004", ram[0x13], 0xCAFE0004);
    }

    // ─── TEST 5: vse32 con vl=2 ───────────────────────────────────────────────
    printf("\n--- Test 5: vse32.v v3 — vl=2, solo 2 palabras ---\n");
    {
        dut->csr_vl = 2;
        vreg[3][0]=0x12345678; vreg[3][1]=0x9ABCDEF0;
        ram[0x20]=ram[0x21]=ram[0x22]=ram[0x23]=0xFFFFFFFF;

        run_lsu(dut, encode_vse32(3, 10), 0x80);

        check("RAM[0x80]=0x12345678",           ram[0x20], 0x12345678);
        check("RAM[0x84]=0x9ABCDEF0",           ram[0x21], 0x9ABCDEF0);
        check("RAM[0x88]=0xFFFFFFFF (intacto)", ram[0x22], 0xFFFFFFFF);
        check("RAM[0x8C]=0xFFFFFFFF (intacto)", ram[0x23], 0xFFFFFFFF);
        dut->csr_vl = 4;
    }

    // ─── TEST 6: roundtrip vle32 -> vse32 -> vle32 ───────────────────────────
    printf("\n--- Test 6: roundtrip vle32 -> vse32 -> vle32 ---\n");
    {
        dut->csr_vl = 4;
        ram[0x30]=0xDEAD0001; ram[0x31]=0xDEAD0002;
        ram[0x32]=0xDEAD0003; ram[0x33]=0xDEAD0004;

        run_lsu(dut, encode_vle32(4, 10), 0xC0);
        ram[0x34]=ram[0x35]=ram[0x36]=ram[0x37]=0;
        run_lsu(dut, encode_vse32(4, 10), 0xD0);
        run_lsu(dut, encode_vle32(5, 10), 0xD0);

        char name[64];
        for (int i = 0; i < 4; i++) {
            snprintf(name, sizeof(name), "roundtrip v5[%d]=0xDEAD%04X", i, i+1);
            check(name, vreg[5][i], 0xDEAD0001 + i);
        }
    }

    // ─── TEST 7: vle32 con vl=1 ───────────────────────────────────────────────
    printf("\n--- Test 7: vle32.v — vl=1, solo 1 elemento ---\n");
    {
        dut->csr_vl = 1;
        ram[0] = 0xABCD1234;
        vreg[6][0]=vreg[6][1]=vreg[6][2]=vreg[6][3]=0x55555555;

        run_lsu(dut, encode_vle32(6, 10), 0x00);

        check("vreg[6][0]=0xABCD1234",            vreg[6][0], 0xABCD1234);
        check("vreg[6][1]=0x55555555 (intacto)",   vreg[6][1], 0x55555555);
        check("vreg[6][2]=0x55555555 (intacto)",   vreg[6][2], 0x55555555);
        check("vreg[6][3]=0x55555555 (intacto)",   vreg[6][3], 0x55555555);
        dut->csr_vl = 4;
    }

    // ─── TEST 8: instruccion no LSU ───────────────────────────────────────────
    printf("\n--- Test 8: instruccion no LSU ---\n");
    {
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = 0x00208033; // add x0,x1,x2
        eval_model(dut);
        check("pcpi_wait=0 para no-LSU",  dut->pcpi_wait,  0);
        tick(dut);
        check("pcpi_ready=0 para no-LSU", dut->pcpi_ready, 0);
        dut->pcpi_valid = 0;
        tick(dut);
    }

    // ─── Resumen ─────────────────────────────────────────────────────────────
    printf("\n==========================================\n");
    printf("Resultados: %d/%d tests pasaron\n", tests_passed, tests_run);
    if (tests_passed == tests_run)
        printf(GREEN "✓ TODOS LOS TESTS PASARON — OE3 lista para HW\n" RESET);
    else
        printf(RED "✗ HAY FALLAS\n" RESET);
    printf("==========================================\n");

    delete dut;
    return (tests_passed == tests_run) ? 0 : 1;
}
