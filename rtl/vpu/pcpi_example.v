// =============================================================================
//  pcpi_example.v — Etapa B: Coprocesador PCPI custom de 1 ciclo
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
// =============================================================================
//
//  Instrucción implementada: custom.add
//    Codificación: funct7=0000000, funct3=000, opcode=0001011 (custom-0)
//    Operación:    rd = rs1 + rs2 + 1
//    Uso en C:     asm volatile ("custom.add %0,%1,%2" : "=r"(rd) : "r"(rs1), "r"(rs2));
//    Uso con .word: .word <codificado con rd/rs1/rs2 que elijas>
//
//  NOTAS DE PROTOCOLO PCPI:
//  - pcpi_wait es COMBINACIONAL respecto a pcpi_valid.
//    Esto es crítico: debe asertarse en el mismo ciclo que pcpi_valid sube,
//    de lo contrario el contador de timeout de 16 ciclos comienza a correr.
//  - Para instrucciones de 1 ciclo: pcpi_ready se pulsa 1 ciclo después de
//    detectar la instrucción (registro), pero pcpi_wait ya está alto desde
//    el mismo ciclo de detección.
//  - pcpi_wr indica que pcpi_rd contiene un resultado válido para escribir
//    en el registro destino del CPU.
//
//  REFERENCIA: Patrón análogo a picorv32_pcpi_mul en picorv32.v
// =============================================================================

`timescale 1 ns / 1 ps

module pcpi_example (
    input         clk,
    input         resetn,

    // --- Interfaz PCPI (igual para todos los coprocesadores) ---
    input         pcpi_valid,   // CPU tiene instrucción lista
    input  [31:0] pcpi_insn,    // La instrucción completa (32 bits)
    input  [31:0] pcpi_rs1,     // Valor del registro fuente rs1
    input  [31:0] pcpi_rs2,     // Valor del registro fuente rs2
    output        pcpi_wr,      // 1 = escribir pcpi_rd en rd del CPU
    output [31:0] pcpi_rd,      // Resultado para el registro destino
    output        pcpi_wait,    // 1 = coprocesador procesando, CPU espera
    output        pcpi_ready    // 1 = resultado listo (pulso de 1 ciclo)
);

    // -------------------------------------------------------------------------
    //  Decodificación de instrucción
    //  custom-0: opcode = 7'b000_1011
    //  custom.add: funct7 = 7'b000_0000, funct3 = 3'b000
    // -------------------------------------------------------------------------
    wire is_custom_add =
        pcpi_valid                      &&   // CPU presenta instrucción
        (pcpi_insn[6:0]   == 7'b0001011) &&  // opcode custom-0
        (pcpi_insn[14:12] == 3'b000)     &&  // funct3 = 0
        (pcpi_insn[31:25] == 7'b0000000);    // funct7 = 0

    // -------------------------------------------------------------------------
    //  pcpi_wait — COMBINACIONAL (regla crítica)
    //  Se aserta en el MISMO ciclo que pcpi_valid, sin pasar por flip-flop.
    //  Mientras wait=1, el timeout de 16 ciclos no corre.
    // -------------------------------------------------------------------------
    assign pcpi_wait = is_custom_add;

    // -------------------------------------------------------------------------
    //  Cálculo del resultado — combinacional puro
    //  rd = rs1 + rs2 + 1
    // -------------------------------------------------------------------------
    wire [31:0] result = pcpi_rs1 + pcpi_rs2 + 32'd1;

    // -------------------------------------------------------------------------
    //  Pipeline de 1 ciclo: registrar resultado y señales de salida
    //  En ciclo 0: detectamos la instrucción (is_custom_add=1, wait=1)
    //  En ciclo 1: pulsamos ready=1, wr=1, presentamos rd
    // -------------------------------------------------------------------------
    reg        pcpi_ready_r;
    reg        pcpi_wr_r;
    reg [31:0] pcpi_rd_r;

    always @(posedge clk) begin
        pcpi_ready_r <= 0;  // default: sin resultado
        pcpi_wr_r    <= 0;
        pcpi_rd_r    <= 32'hx;

        if (!resetn) begin
            pcpi_ready_r <= 0;
            pcpi_wr_r    <= 0;
            pcpi_rd_r    <= 0;
        end else if (is_custom_add) begin
            // Capturamos resultado y señalizamos ready en el próximo ciclo
            pcpi_ready_r <= 1;
            pcpi_wr_r    <= 1;
            pcpi_rd_r    <= result;
        end
    end

    assign pcpi_ready = pcpi_ready_r;
    assign pcpi_wr    = pcpi_wr_r;
    assign pcpi_rd    = pcpi_rd_r;

endmodule
