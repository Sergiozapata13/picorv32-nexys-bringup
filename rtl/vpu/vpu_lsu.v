// =============================================================================
//  vpu_lsu.v — OE3: Load/Store Unit vectorial
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Instrucciones:
//    vle32.v vd,  (rs1)  — carga  vl palabras de 32b desde mem a vreg[vd]
//    vse32.v vs3, (rs1)  — escribe vl palabras de 32b desde vreg[vs3] a mem
//
//  FSM para LOAD:  IDLE -> REQ -> WAIT_READY -> [repetir vl] -> DONE -> WAIT
//  FSM para STORE: IDLE -> READ_VREG -> REQ -> WAIT_READY -> [...] -> DONE -> WAIT
//
//  FIX v3: lsu_mem_valid NO esta en los defaults del always block.
//  Se maneja explicitamente en cada estado para que permanezca alto
//  durante toda la transaccion de memoria (S_REQ y S_WAIT_READY),
//  hasta que lsu_mem_ready=1 lo baje.
// =============================================================================

`timescale 1 ns / 1 ps

module vpu_lsu #(
    parameter VLEN  = 128,
    parameter EEW   = 32,
    parameter NREGS = 8
) (
    input         clk,
    input         resetn,

    // Interfaz PCPI
    input         pcpi_valid,
    input  [31:0] pcpi_insn,
    input  [31:0] pcpi_rs1,
    input  [31:0] pcpi_rs2,
    output        pcpi_wr,
    output [31:0] pcpi_rd,
    output        pcpi_wait,
    output        pcpi_ready,

    // Estado vectorial
    input  [31:0] csr_vl,

    // Bus de memoria (mantenido alto hasta mem_ready)
    output reg        lsu_mem_valid,
    output reg [31:0] lsu_mem_addr,
    output reg [31:0] lsu_mem_wdata,
    output reg  [3:0] lsu_mem_wstrb,
    input             lsu_mem_ready,
    input      [31:0] lsu_mem_rdata,

    // Banco vectorial — escritura (load)
    output reg        vreg_we,
    output reg  [2:0] vreg_waddr,
    output reg  [1:0] vreg_welem,
    output reg [31:0] vreg_wdata,

    // Banco vectorial — lectura (store)
    output reg  [2:0] vreg_raddr,
    output reg  [1:0] vreg_relem,
    input      [31:0] vreg_rdata
);

    localparam VLMAX = VLEN / EEW;  // 4

    // ── Decodificacion ────────────────────────────────────────────────────────
    wire is_load_fp  = (pcpi_insn[6:0] == 7'b0000111);
    wire is_store_fp = (pcpi_insn[6:0] == 7'b0100111);
    wire is_w32      = (pcpi_insn[14:12] == 3'b110);

    wire is_vle32 = is_load_fp  && is_w32 && pcpi_valid;
    wire is_vse32 = is_store_fp && is_w32 && pcpi_valid;
    wire is_vlsu  = is_vle32 || is_vse32;

    wire [2:0] vreg_idx_in = pcpi_insn[9:7];

    // ── Registros capturados en IDLE ──────────────────────────────────────────
    reg        is_load_r;
    reg [2:0]  vreg_idx_r;
    reg [31:0] base_addr_r;
    reg [31:0] vl_r;
    reg [1:0]  elem_r;
    reg [31:0] store_data_r;

    // ── FSM ───────────────────────────────────────────────────────────────────
    localparam S_IDLE       = 3'd0;
    localparam S_READ_VREG  = 3'd1;
    localparam S_REQ        = 3'd2;
    localparam S_WAIT_READY = 3'd3;
    localparam S_DONE       = 3'd4;
    localparam S_WAIT       = 3'd5;

    reg [2:0] state;
    reg       ready_r;

    always @(posedge clk) begin
        // ── Defaults ─────────────────────────────────────────────────────────
        // NOTA: lsu_mem_valid NO tiene default aqui — se maneja
        // explicitamente en cada estado para que permanezca alto
        // durante toda la transaccion.
        ready_r <= 0;
        vreg_we <= 0;

        if (!resetn) begin
            state         <= S_IDLE;
            lsu_mem_valid <= 0;
            vreg_we       <= 0;
        end else begin
            case (state)

                S_IDLE: begin
                    lsu_mem_valid <= 0;  // asegurar que esta bajo en reposo
                    if (is_vlsu) begin
                        is_load_r   <= is_vle32;
                        vreg_idx_r  <= vreg_idx_in;
                        base_addr_r <= pcpi_rs1;
                        vl_r        <= csr_vl;
                        elem_r      <= 2'd0;

                        if (is_vle32) begin
                            state <= S_REQ;
                        end else begin
                            vreg_raddr <= vreg_idx_in;
                            vreg_relem <= 2'd0;
                            state      <= S_READ_VREG;
                        end
                    end
                end

                S_READ_VREG: begin
                    lsu_mem_valid <= 0;  // todavia no hay transaccion
                    // vreg_rdata ya es estable — capturar dato del banco
                    store_data_r <= vreg_rdata;
                    state        <= S_REQ;
                end

                S_REQ: begin
                    // Presentar transaccion y MANTENER valid=1 hasta que ready llegue
                    lsu_mem_valid <= 1;
                    lsu_mem_addr  <= base_addr_r + {28'b0, elem_r, 2'b00};

                    if (is_load_r) begin
                        lsu_mem_wstrb <= 4'b0000;
                        lsu_mem_wdata <= 32'b0;
                    end else begin
                        lsu_mem_wstrb <= 4'b1111;
                        lsu_mem_wdata <= store_data_r;
                    end

                    state <= S_WAIT_READY;
                end

                S_WAIT_READY: begin
                    // Mantener lsu_mem_valid=1 hasta recibir ready
                    if (lsu_mem_ready) begin
                        // Transaccion completada — bajar valid
                        lsu_mem_valid <= 0;

                        if (is_load_r) begin
                            vreg_we    <= 1;
                            vreg_waddr <= vreg_idx_r;
                            vreg_welem <= elem_r;
                            vreg_wdata <= lsu_mem_rdata;
                        end

                        if ({30'b0, elem_r} + 1 < vl_r) begin
                            elem_r <= elem_r + 2'd1;

                            if (is_load_r) begin
                                state <= S_REQ;
                            end else begin
                                vreg_raddr <= vreg_idx_r;
                                vreg_relem <= elem_r + 2'd1;
                                state      <= S_READ_VREG;
                            end
                        end else begin
                            state <= S_DONE;
                        end
                    end
                    // Si no hay ready: NO tocar lsu_mem_valid, mantener=1
                    // (el registro retiene su valor al no asignarse)
                end

                S_DONE: begin
                    lsu_mem_valid <= 0;
                    ready_r       <= 1;
                    state         <= S_WAIT;
                end

                S_WAIT: begin
                    lsu_mem_valid <= 0;
                    state         <= S_IDLE;
                end

                default: begin
                    lsu_mem_valid <= 0;
                    state         <= S_IDLE;
                end
            endcase
        end
    end

    // ── pcpi_wait COMBINACIONAL ───────────────────────────────────────────────
    assign pcpi_wait  = is_vlsu ||
                        (state == S_READ_VREG)  ||
                        (state == S_REQ)        ||
                        (state == S_WAIT_READY) ||
                        (state == S_DONE);

    assign pcpi_ready = ready_r;
    assign pcpi_wr    = 1'b0;
    assign pcpi_rd    = 32'b0;

endmodule
