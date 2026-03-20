// =============================================================================
//  tb_pcpi_example.cpp — Testbench Verilator para pcpi_example.v
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Compilar y ejecutar:
//    verilator --cc --exe --build pcpi_example.v tb_pcpi_example.cpp -o sim_pcpi
//    ./obj_dir/sim_pcpi
// =============================================================================

#include "Vpcpi_example.h"
#include "verilated.h"
#include <cstdio>
#include <cstdlib>

// Helpers de color para la terminal
#define GREEN "\033[32m"
#define RED   "\033[31m"
#define RESET "\033[0m"

// Genera la instrucción custom.add codificada
// opcode=0001011, funct3=000, funct7=0000000
// Formato R: [31:25]=funct7 [24:20]=rs2 [19:15]=rs1 [14:12]=funct3 [11:7]=rd [6:0]=opcode
static uint32_t encode_custom_add(int rd, int rs1, int rs2) {
    return (0b0000000 << 25) |
           ((rs2 & 0x1F) << 20) |
           ((rs1 & 0x1F) << 15) |
           (0b000 << 12) |
           ((rd  & 0x1F) << 7) |
           0b0001011;
}

// Aplica clock
static void tick(Vpcpi_example* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

// Estado del test
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

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vpcpi_example* dut = new Vpcpi_example;

    printf("=== Testbench: pcpi_example (custom.add rd = rs1 + rs2 + 1) ===\n\n");

    // ------------------------------------------------------------------
    // RESET
    // ------------------------------------------------------------------
    dut->resetn    = 0;
    dut->pcpi_valid = 0;
    dut->pcpi_insn  = 0;
    dut->pcpi_rs1   = 0;
    dut->pcpi_rs2   = 0;
    tick(dut); tick(dut);
    dut->resetn = 1;
    tick(dut);

    // ------------------------------------------------------------------
    // TEST 1: Instrucción custom.add normal
    //   rs1=10, rs2=20 → rd = 10+20+1 = 31
    // ------------------------------------------------------------------
    printf("--- Test 1: custom.add rs1=10, rs2=20 (esperado rd=31) ---\n");
    {
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = encode_custom_add(1, 2, 3); // rd=x1, rs1=x2, rs2=x3
        dut->pcpi_rs1   = 10;
        dut->pcpi_rs2   = 20;
        dut->eval();

        // Ciclo 0: pcpi_valid sube
        // pcpi_wait debe ser 1 en ESTE mismo ciclo (combinacional)
        check("pcpi_wait en ciclo 0 (combinacional)", dut->pcpi_wait, 1);
        check("pcpi_ready en ciclo 0 debe ser 0",     dut->pcpi_ready, 0);

        tick(dut); // avanzar al ciclo 1

        // Ciclo 1: ready debe pulsar
        check("pcpi_ready en ciclo 1", dut->pcpi_ready, 1);
        check("pcpi_wr en ciclo 1",    dut->pcpi_wr,    1);
        check("pcpi_rd = 31",          dut->pcpi_rd,    31);

        // CPU toma el resultado, baja valid
        dut->pcpi_valid = 0;
        tick(dut);

        // Ciclo 2: ready debe volver a 0
        check("pcpi_ready vuelve a 0", dut->pcpi_ready, 0);
        check("pcpi_wait  vuelve a 0", dut->pcpi_wait,  0);
    }

    printf("\n--- Test 2: custom.add con valores límite ---\n");
    {
        // 0xFFFFFFFF + 0 + 1 = 0x00000000 (overflow)
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = encode_custom_add(5, 6, 7);
        dut->pcpi_rs1   = 0xFFFFFFFF;
        dut->pcpi_rs2   = 0;
        dut->eval();
        check("pcpi_wait con rs1=0xFFFFFFFF", dut->pcpi_wait, 1);
        tick(dut);
        check("overflow: rd = 0",   dut->pcpi_rd,    0x00000000);
        check("pcpi_ready = 1",      dut->pcpi_ready, 1);
        dut->pcpi_valid = 0;
        tick(dut);
    }

    printf("\n--- Test 3: instrucción NO reconocida (opcode diferente) ---\n");
    {
        // opcode=0110011 (R-type normal, no custom)
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = 0x00208033; // add x0, x1, x2 — opcode 0110011
        dut->pcpi_rs1   = 5;
        dut->pcpi_rs2   = 5;
        dut->eval();
        check("pcpi_wait = 0 para instrucción no reconocida", dut->pcpi_wait, 0);
        tick(dut);
        check("pcpi_ready = 0 para instrucción no reconocida", dut->pcpi_ready, 0);
        dut->pcpi_valid = 0;
        tick(dut);
    }

    printf("\n--- Test 4: custom.add con rs1=rs2=0 ---\n");
    {
        dut->pcpi_valid = 1;
        dut->pcpi_insn  = encode_custom_add(1, 0, 0);
        dut->pcpi_rs1   = 0;
        dut->pcpi_rs2   = 0;
        dut->eval();
        check("pcpi_wait = 1", dut->pcpi_wait, 1);
        tick(dut);
        check("rd = 0+0+1 = 1", dut->pcpi_rd, 1);
        dut->pcpi_valid = 0;
        tick(dut);
    }

    // ------------------------------------------------------------------
    // Resumen
    // ------------------------------------------------------------------
    printf("\n==========================================\n");
    printf("Resultados: %d/%d tests pasaron\n", tests_passed, tests_run);
    if (tests_passed == tests_run)
        printf(GREEN "✓ TODOS LOS TESTS PASARON\n" RESET);
    else
        printf(RED "✗ HAY FALLAS — revisar protocolo PCPI\n" RESET);
    printf("==========================================\n");

    delete dut;
    return (tests_passed == tests_run) ? 0 : 1;
}
