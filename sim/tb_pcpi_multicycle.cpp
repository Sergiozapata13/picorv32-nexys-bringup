// =============================================================================
//  tb_pcpi_multicycle.cpp — Testbench Verilator para pcpi_multicycle.v
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Compilar:
//    verilator --cc --exe --build --trace \
//      -Wno-UNUSEDSIGNAL -Wno-CASEINCOMPLETE \
//      pcpi_multicycle.v tb_pcpi_multicycle.cpp -o sim_multicycle
//    ./obj_dir/sim_multicycle
// =============================================================================

#include "Vpcpi_multicycle.h"
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

// Aplica un flanco de reloj y evalua
static void tick(Vpcpi_multicycle* dut) {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
}

// -----------------------------------------------------------------------------
//  run_insn — presenta instruccion y espera hasta pcpi_ready=1
//
//  Protocolo correcto:
//    Ciclo 0: pcpi_valid=1, evaluar (pcpi_wait debe ser 1 ya)
//    Ciclos 1..N: tick() hasta que pcpi_ready=1
//    Cuando pcpi_ready=1: leer pcpi_rd EN ESE MISMO CICLO (antes del tick)
//    Ciclo N+1: bajar pcpi_valid, hacer tick
//
//  Retorna numero de ciclos de latencia
// -----------------------------------------------------------------------------
static int run_insn(Vpcpi_multicycle* dut,
                    uint32_t insn, uint32_t rs1, uint32_t rs2,
                    uint32_t* result_out,
                    int*      wait_dropped_out) {
    int cycles       = 0;
    int wait_dropped = 0;

    // Presentar instruccion
    dut->pcpi_valid = 1;
    dut->pcpi_insn  = insn;
    dut->pcpi_rs1   = rs1;
    dut->pcpi_rs2   = rs2;
    dut->eval();   // evaluar combinacional sin tick

    // Esperar hasta pcpi_ready=1
    // Hacemos tick y luego chequeamos — asi leemos la salida
    // al final del ciclo en que ready se aserta
    int timeout = 30;
    while (timeout-- > 0) {
        tick(dut);
        cycles++;

        if (!dut->pcpi_wait && dut->pcpi_valid) {
            wait_dropped = 1;
        }

        if (dut->pcpi_ready) {
            // Leer resultado AHORA — en este ciclo pcpi_ready=1
            *result_out = dut->pcpi_rd;
            break;
        }
    }

    if (wait_dropped_out) *wait_dropped_out = wait_dropped;

    if (timeout <= 0) {
        *result_out = 0xDEADBEEF;
        printf(RED "[TIMEOUT]" RESET " instruccion no completo\n");
    }

    // CPU baja pcpi_valid en el ciclo siguiente
    dut->pcpi_valid = 0;
    tick(dut);

    return cycles;
}

// Encoding custom.slowmul: opcode=0101011, funct3=000, funct7=0000000
static uint32_t encode_slowmul(int rd, int rs1, int rs2) {
    return (0b0000000 << 25) |
           ((rs2 & 0x1F) << 20) |
           ((rs1 & 0x1F) << 15) |
           (0b000 << 12) |
           ((rd  & 0x1F) << 7) |
           0b0101011;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vpcpi_multicycle* dut = new Vpcpi_multicycle;

    printf("=== Testbench: pcpi_multicycle (custom.slowmul, 8 ciclos) ===\n\n");

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
        dut->pcpi_insn  = encode_slowmul(10, 10, 11);
        dut->pcpi_rs1   = 3;
        dut->pcpi_rs2   = 4;
        dut->eval();  // evaluar SIN tick

        check("pcpi_wait en ciclo 0 (combinacional)", dut->pcpi_wait,  1);
        check("pcpi_ready en ciclo 0 debe ser 0",     dut->pcpi_ready, 0);

        // Esperar resultado
        uint32_t result = 0;
        int dummy;
        int lat = run_insn(dut, encode_slowmul(10,10,11), 3, 4, &result, &dummy);
        // Nota: run_insn vuelve a presentar la instruccion internamente
        // pero pcpi_valid ya esta en 1 del bloque anterior — reseteamos
        dut->pcpi_valid = 0;
        tick(dut); tick(dut);

        // Correr limpio
        run_insn(dut, encode_slowmul(10,10,11), 3, 4, &result, &dummy);
        check("resultado 3*4 = 12", result, 12);
    }

    // ------------------------------------------------------------------
    //  TEST 2: Latencia
    // ------------------------------------------------------------------
    printf("\n--- Test 2: Latencia de ~10 ciclos ---\n");
    {
        uint32_t result = 0;
        int dummy;

        // Ciclo de inicio limpio
        tick(dut); tick(dut);

        int lat = run_insn(dut, encode_slowmul(10,10,11), 5, 6, &result, &dummy);

        printf(YELLOW "  Latencia medida: %d ciclos\n" RESET, lat);
        check("resultado 5*6 = 30", result, 30);

        tests_run++;
        if (lat >= 9 && lat <= 11) {
            printf(GREEN "[PASS]" RESET " Latencia %d en rango [9-11]\n", lat);
            tests_passed++;
        } else {
            printf(RED "[FAIL]" RESET " Latencia %d fuera de rango [9-11]\n", lat);
        }
    }

    // ------------------------------------------------------------------
    //  TEST 3: pcpi_wait sostenido
    // ------------------------------------------------------------------
    printf("\n--- Test 3: pcpi_wait sostenido durante FSM ---\n");
    {
        uint32_t result = 0;
        int wait_dropped = 0;

        run_insn(dut, encode_slowmul(10,10,11), 7, 8, &result, &wait_dropped);

        tests_run++;
        if (!wait_dropped) {
            printf(GREEN "[PASS]" RESET " pcpi_wait se mantuvo alto toda la FSM\n");
            tests_passed++;
        } else {
            printf(RED "[FAIL]" RESET " pcpi_wait bajo durante procesamiento\n");
        }
        check("resultado 7*8 = 56", result, 56);
    }

    // ------------------------------------------------------------------
    //  TEST 4: Calculos consecutivos
    // ------------------------------------------------------------------
    printf("\n--- Test 4: Calculos consecutivos ---\n");
    {
        struct { uint32_t a, b, exp; } cases[] = {
            {0,     0,   0},
            {1,     1,   1},
            {10,    10,  100},
            {255,   2,   510},
            {0xFFFF, 2,  0x1FFFE},
        };

        for (int i = 0; i < 5; i++) {
            uint32_t result = 0;
            int dummy;
            run_insn(dut, encode_slowmul(10,10,11),
                     cases[i].a, cases[i].b, &result, &dummy);

            char name[64];
            snprintf(name, sizeof(name), "%u * %u = %u",
                     cases[i].a, cases[i].b, cases[i].exp);
            check(name, result, cases[i].exp);

            // Pausa entre instrucciones
            tick(dut);
        }
    }

    // ------------------------------------------------------------------
    //  TEST 5: Instruccion no reconocida
    // ------------------------------------------------------------------
    printf("\n--- Test 5: Instruccion no reconocida ---\n");
    {
        // Esperar que FSM este en IDLE
        tick(dut); tick(dut);

        dut->pcpi_valid = 1;
        dut->pcpi_insn  = 0x00208033; // add x0,x1,x2 — opcode 0110011
        dut->pcpi_rs1   = 5;
        dut->pcpi_rs2   = 5;
        dut->eval();

        check("pcpi_wait=0 para instruccion no reconocida", dut->pcpi_wait,  0);
        tick(dut);
        check("pcpi_ready=0 para instruccion no reconocida", dut->pcpi_ready, 0);

        dut->pcpi_valid = 0;
        tick(dut);
    }

    // ------------------------------------------------------------------
    //  Resumen
    // ------------------------------------------------------------------
    printf("\n==========================================\n");
    printf("Resultados: %d/%d tests pasaron\n", tests_passed, tests_run);
    if (tests_passed == tests_run)
        printf(GREEN "✓ TODOS LOS TESTS PASARON — Etapa C lista\n" RESET);
    else
        printf(RED "✗ HAY FALLAS\n" RESET);
    printf("==========================================\n");

    delete dut;
    return (tests_passed == tests_run) ? 0 : 1;
}
