// =============================================================================
//  pcpi_multicycle.v — Etapa C: Coprocesador PCPI multiciclo con FSM
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Instruccion: custom.slowmul  rd = rs1 * rs2  (latencia: 8 ciclos)
//  Codificacion: funct7=0000000, funct3=000, opcode=0101011 (custom-1)
//
//  FSM:
//    IDLE    → detecta instruccion, captura operandos, va a WORKING
//    WORKING → cuenta 8 ciclos con pcpi_wait=1
//    DONE    → calcula resultado, pulsa pcpi_ready=1 por 1 ciclo → IDLE
//
//  CORRECCION v2:
//  - pcpi_wait combinacional basado en state registrado, no en is_slowmul
//    durante WORKING/DONE. Esto evita que instrucciones no reconocidas
//    activen pcpi_wait cuando la FSM ya termino.
//  - Resultado calculado en WORKING ultimo ciclo, presentado en DONE.
//  - pcpi_ready se aserta en DONE y se baja al volver a IDLE.
// =============================================================================

`timescale 1 ns / 1 ps

module pcpi_multicycle (
    input         clk,
    input         resetn,

    input         pcpi_valid,
    input  [31:0] pcpi_insn,
    input  [31:0] pcpi_rs1,
    input  [31:0] pcpi_rs2,
    output        pcpi_wr,
    output [31:0] pcpi_rd,
    output        pcpi_wait,
    output        pcpi_ready
);

    // -------------------------------------------------------------------------
    //  Decodificacion
    // -------------------------------------------------------------------------
    wire is_slowmul =
        pcpi_valid                       &&
        (pcpi_insn[6:0]   == 7'b0101011) &&
        (pcpi_insn[14:12] == 3'b000)     &&
        (pcpi_insn[31:25] == 7'b0000000);

    // -------------------------------------------------------------------------
    //  FSM
    // -------------------------------------------------------------------------
    localparam S_IDLE    = 2'd0;
    localparam S_WORKING = 2'd1;
    localparam S_DONE    = 2'd2;

    reg [1:0]  state;
    reg [2:0]  counter;
    reg [31:0] operand_a;
    reg [31:0] operand_b;
    reg [31:0] result_r;
    reg        ready_r;
    reg        wr_r;

    always @(posedge clk) begin
        ready_r <= 0;
        wr_r    <= 0;

        if (!resetn) begin
            state     <= S_IDLE;
            counter   <= 0;
            operand_a <= 0;
            operand_b <= 0;
            result_r  <= 0;
        end else begin
            case (state)

                S_IDLE: begin
                    if (is_slowmul) begin
                        operand_a <= pcpi_rs1;
                        operand_b <= pcpi_rs2;
                        counter   <= 3'd7;  // 8 ciclos: 7 downto 0
                        state     <= S_WORKING;
                    end
                end

                S_WORKING: begin
                    if (counter == 0) begin
                        // Ultimo ciclo de trabajo: calcular resultado
                        result_r <= operand_a * operand_b;
                        state    <= S_DONE;
                    end else begin
                        counter <= counter - 1;
                    end
                end

                S_DONE: begin
                    // Presentar resultado y pulsar ready por 1 ciclo
                    ready_r <= 1;
                    wr_r    <= 1;
                    state   <= S_IDLE;
                end

                default: state <= S_IDLE;

            endcase
        end
    end

    // -------------------------------------------------------------------------
    //  pcpi_wait — combinacional
    //
    //  Alto cuando:
    //    - Detectamos is_slowmul en IDLE (mismo ciclo que pcpi_valid sube)
    //    - Estamos en WORKING (procesando)
    //    - Estamos en DONE (resultado listo, esperando que CPU lo tome)
    //
    //  La clave: usamos el state REGISTRADO para WORKING y DONE,
    //  pero anadimos is_slowmul para el ciclo de transicion IDLE→WORKING
    //  donde state todavia vale S_IDLE.
    // -------------------------------------------------------------------------
    assign pcpi_wait = is_slowmul ||
                       (state == S_WORKING) ||
                       (state == S_DONE);

    assign pcpi_ready = ready_r;
    assign pcpi_wr    = wr_r;
    assign pcpi_rd    = result_r;

endmodule
