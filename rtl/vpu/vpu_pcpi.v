// =============================================================================
//  vpu_pcpi.v — OE4: VPU completa (decode + ALU + LSU)
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Integra los tres modulos de la VPU en un unico coprocesador PCPI:
//    vpu_decode  — vsetvli/vsetvl → CSRs vl/vtype (dinamicos)
//    vpu_alu     — 9 instrucciones VALU + banco 8x128b
//    vpu_lsu     — vle32/vse32 con acceso al bus de memoria
//
//  Banco vectorial:
//    El banco fisico (8 x 128b) vive en vpu_alu a traves de sus puertos dbg_*.
//    vpu_lsu accede al banco via los puertos vreg_* que se conectan a dbg_*.
//    Solo uno puede estar activo a la vez (el CPU ejecuta una instruccion
//    a la vez), por lo que no hay conflicto de acceso.
//
//  CSRs:
//    vpu_decode genera csr_vl y csr_vtype dinamicamente.
//    vpu_alu y vpu_lsu los reciben como entradas — ya no son constantes.
//
//  OR de senales PCPI:
//    Los tres modulos comparten el bus PCPI. Sus opcodes son disjuntos:
//      vpu_decode: opcode=1010111, funct3=111  (vsetvli/vsetvl)
//      vpu_alu:    opcode=1010111, funct3=000/010/101
//      vpu_lsu:    opcode=0000111 o 0100111, funct3=110
//    Nunca se activan dos modulos al mismo tiempo.
//
//  Interfaz externa:
//    PCPI estandar (valid/insn/rs1/rs2/wr/rd/wait/ready)
//    + bus de memoria para vpu_lsu (lsu_mem_*)
// =============================================================================

`timescale 1 ns / 1 ps

module vpu_pcpi #(
    parameter VLEN  = 128,
    parameter EEW   = 32,
    parameter NREGS = 8
) (
    input         clk,
    input         resetn,

    // ── Interfaz PCPI (hacia PicoRV32) ───────────────────────────────────────
    input         pcpi_valid,
    input  [31:0] pcpi_insn,
    input  [31:0] pcpi_rs1,
    input  [31:0] pcpi_rs2,
    output        pcpi_wr,
    output [31:0] pcpi_rd,
    output        pcpi_wait,
    output        pcpi_ready,

    // ── Bus de memoria (hacia arbitro en top.v) ───────────────────────────────
    output        lsu_mem_valid,
    output [31:0] lsu_mem_addr,
    output [31:0] lsu_mem_wdata,
    output  [3:0] lsu_mem_wstrb,
    input         lsu_mem_ready,
    input  [31:0] lsu_mem_rdata
);

    // ─── CSRs dinamicos desde vpu_decode ─────────────────────────────────────
    wire [31:0] csr_vl;
    wire [31:0] csr_vtype;

    // ─── Senales PCPI individuales de cada modulo ────────────────────────────
    wire        dec_wr,  alu_wr,  lsu_wr;
    wire [31:0] dec_rd,  alu_rd,  lsu_rd;
    wire        dec_wait, alu_wait, lsu_wait;
    wire        dec_ready, alu_ready, lsu_ready;

    // ─── OR de senales PCPI al CPU ───────────────────────────────────────────
    assign pcpi_wait  = dec_wait  | alu_wait  | lsu_wait;
    assign pcpi_ready = dec_ready | alu_ready | lsu_ready;
    assign pcpi_wr    = dec_wr    | alu_wr    | lsu_wr;
    assign pcpi_rd    = dec_ready ? dec_rd :
                        alu_ready ? alu_rd : lsu_rd;

    // ─── Banco vectorial — puertos LSU (via dbg_* de vpu_alu) ────────────────
    wire        lsu_vreg_we;
    wire  [2:0] lsu_vreg_waddr;
    wire  [1:0] lsu_vreg_welem;
    wire [31:0] lsu_vreg_wdata;
    wire  [2:0] lsu_vreg_raddr;
    wire  [1:0] lsu_vreg_relem;
    wire [31:0] lsu_vreg_rdata;

    // ─── OE1: vpu_decode ─────────────────────────────────────────────────────
    vpu_decode u_decode (
        .clk        (clk),
        .resetn     (resetn),
        .pcpi_valid (pcpi_valid),
        .pcpi_insn  (pcpi_insn),
        .pcpi_rs1   (pcpi_rs1),
        .pcpi_rs2   (pcpi_rs2),
        .pcpi_wr    (dec_wr),
        .pcpi_rd    (dec_rd),
        .pcpi_wait  (dec_wait),
        .pcpi_ready (dec_ready),
        .csr_vl     (csr_vl),      // salida dinamica
        .csr_vtype  (csr_vtype)
    );

    // ─── OE2: vpu_alu ────────────────────────────────────────────────────────
    vpu_alu u_alu (
        .clk          (clk),
        .resetn       (resetn),
        .pcpi_valid   (pcpi_valid),
        .pcpi_insn    (pcpi_insn),
        .pcpi_rs1     (pcpi_rs1),
        .pcpi_rs2     (pcpi_rs2),
        .pcpi_wr      (alu_wr),
        .pcpi_rd      (alu_rd),
        .pcpi_wait    (alu_wait),
        .pcpi_ready   (alu_ready),
        .csr_vl       (csr_vl),    // recibe del decode, no constante
        .csr_vtype    (csr_vtype),
        // Puerto de banco compartido con LSU
        .dbg_reg_sel  (lsu_vreg_we ? lsu_vreg_waddr : lsu_vreg_raddr),
        .dbg_elem_sel (lsu_vreg_we ? lsu_vreg_welem : lsu_vreg_relem),
        .dbg_rdata    (lsu_vreg_rdata),
        .dbg_we       (lsu_vreg_we),
        .dbg_wdata    (lsu_vreg_wdata)
    );

    // ─── OE3: vpu_lsu ────────────────────────────────────────────────────────
    vpu_lsu u_lsu (
        .clk            (clk),
        .resetn         (resetn),
        .pcpi_valid     (pcpi_valid),
        .pcpi_insn      (pcpi_insn),
        .pcpi_rs1       (pcpi_rs1),
        .pcpi_rs2       (pcpi_rs2),
        .pcpi_wr        (lsu_wr),
        .pcpi_rd        (lsu_rd),
        .pcpi_wait      (lsu_wait),
        .pcpi_ready     (lsu_ready),
        .csr_vl         (csr_vl),  // recibe del decode, no constante
        // Bus de memoria
        .lsu_mem_valid  (lsu_mem_valid),
        .lsu_mem_addr   (lsu_mem_addr),
        .lsu_mem_wdata  (lsu_mem_wdata),
        .lsu_mem_wstrb  (lsu_mem_wstrb),
        .lsu_mem_ready  (lsu_mem_ready),
        .lsu_mem_rdata  (lsu_mem_rdata),
        // Banco vectorial
        .vreg_we        (lsu_vreg_we),
        .vreg_waddr     (lsu_vreg_waddr),
        .vreg_welem     (lsu_vreg_welem),
        .vreg_wdata     (lsu_vreg_wdata),
        .vreg_raddr     (lsu_vreg_raddr),
        .vreg_relem     (lsu_vreg_relem),
        .vreg_rdata     (lsu_vreg_rdata)
    );

endmodule
