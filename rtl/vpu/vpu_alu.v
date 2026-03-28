// =============================================================================
//  vpu_alu.v - OE2: Banco de registros vectorial + VALU completa
//  TFG: RVV-lite sobre PicoRV32 - Sergio, TEC
//
//  Instrucciones VALU .vv  (funct3=000, OPIVV):
//    vadd.vv  funct6=000000   vd[i] = vs2[i] + vs1[i]
//    vsub.vv  funct6=000010   vd[i] = vs2[i] - vs1[i]
//    vand.vv  funct6=001001   vd[i] = vs2[i] & vs1[i]
//    vor.vv   funct6=001010   vd[i] = vs2[i] | vs1[i]
//    vxor.vv  funct6=001011   vd[i] = vs2[i] ^ vs1[i]
//    vsll.vv  funct6=100101   vd[i] = vs2[i] << vs1[i][4:0]
//    vsrl.vv  funct6=101000   vd[i] = vs2[i] >> vs1[i][4:0]
//
//  Instrucciones OPMVV (funct3=010):
//    vmul.vv    funct6=100101  vd[i] = vs2[i] * vs1[i]  (32x32->32 bajo)
//    vredsum.vs funct6=000000  vd[0] = vs1[0] + sum(vs2[i], i<vl)
//    vmv.v.x    funct6=010111  vd[i] = rs1  (escalar a vector)
//    vmv.x.s    funct6=010000  rd = vs2[0]  (vector a escalar)
//
//  FSM: IDLE -> EXEC -> DONE -> WAIT -> IDLE
//    S_WAIT garantiza estabilidad del banco entre instrucciones consecutivas
//
//  Nota sobre vmul.vv: usa funct3=010 (OPMVV) y funct6=100101
//  Nota sobre vsll.vv: usa funct3=000 (OPIVV) y funct6=100101
//  Sin conflicto - diferente funct3.
// =============================================================================

`timescale 1 ns / 1 ps

module vpu_alu #(
    parameter VLEN  = 128,
    parameter EEW   = 32,
    parameter NREGS = 8
) (
    input         clk,
    input         resetn,

    input         pcpi_valid,
    input  [31:0] pcpi_insn,
    input  [31:0] pcpi_rs1,
    input  [31:0] pcpi_rs2,
    output        pcpi_wr,
    output [31:0] pcpi_rd,
    output        pcpi_wait,
    output        pcpi_ready,

    input  [31:0] csr_vl,
    input  [31:0] csr_vtype,

    // Puertos de debug para testbench Verilator
    input  [2:0]  dbg_reg_sel,
    input  [1:0]  dbg_elem_sel,
    output [31:0] dbg_rdata,
    input         dbg_we,
    input  [31:0] dbg_wdata
);

    localparam VLMAX = VLEN / EEW;  // 4

    // -------------------------------------------------------------------------
    //  Banco de registros - 8 x 128 bits
    // -------------------------------------------------------------------------
    reg [127:0] vreg [0:NREGS-1];

    integer init_i;
    initial begin
        for (init_i = 0; init_i < NREGS; init_i = init_i + 1)
            vreg[init_i] = 128'b0;
    end

    assign dbg_rdata = vreg[dbg_reg_sel][dbg_elem_sel*32 +: 32];

    // -------------------------------------------------------------------------
    //  Decodificacion
    // -------------------------------------------------------------------------
    wire is_rvv   = (pcpi_insn[6:0] == 7'b1010111);
    wire [5:0] funct6 = pcpi_insn[31:26];
    wire       vm_in  = pcpi_insn[25];
    wire [2:0] vs2_in = pcpi_insn[22:20];
    wire [2:0] vs1_in = pcpi_insn[17:15];
    wire [2:0] vd_in  = pcpi_insn[9:7];

    // OPIVV - funct3=000 (.vv aritmetico)
    wire is_ivv   = is_rvv && (pcpi_insn[14:12] == 3'b000);
    wire is_vadd  = is_ivv && pcpi_valid && (funct6 == 6'b000000);
    wire is_vsub  = is_ivv && pcpi_valid && (funct6 == 6'b000010);
    wire is_vand  = is_ivv && pcpi_valid && (funct6 == 6'b001001);
    wire is_vor   = is_ivv && pcpi_valid && (funct6 == 6'b001010);
    wire is_vxor  = is_ivv && pcpi_valid && (funct6 == 6'b001011);
    wire is_vsll  = is_ivv && pcpi_valid && (funct6 == 6'b100101);
    wire is_vsrl  = is_ivv && pcpi_valid && (funct6 == 6'b101000);
    wire is_ivv_any = is_vadd|is_vsub|is_vand|is_vor|is_vxor|is_vsll|is_vsrl;

    // OPMVV - funct3=010
    wire is_mvv   = is_rvv && (pcpi_insn[14:12] == 3'b010);
    wire is_vmul    = is_mvv && pcpi_valid && (funct6 == 6'b100101); // vmul.vv
    wire is_vredsum = is_mvv && pcpi_valid && (funct6 == 6'b000000); // vredsum.vs
    wire is_vmvxs   = is_mvv && pcpi_valid && (funct6 == 6'b010000); // vmv.x.s

    // OPF (funct3=101) - vmv.v.x
    wire is_opf   = is_rvv && (pcpi_insn[14:12] == 3'b101);
    wire is_vmvvx = is_opf && pcpi_valid && (funct6 == 6'b010111); // vmv.v.x

    wire is_valu_any = is_ivv_any | is_vmul;
    wire is_any      = is_valu_any | is_vredsum | is_vmvxs | is_vmvvx;

    // -------------------------------------------------------------------------
    //  Registros capturados en S_IDLE
    // -------------------------------------------------------------------------
    reg [5:0]   funct6_r;
    reg         vm_r;
    reg [2:0]   vs2_r, vs1_r, vd_r;
    reg [31:0]  vl_r;
    reg [127:0] op_a_r;    // snapshot de vreg[vs2]
    reg [127:0] op_b_r;    // snapshot de vreg[vs1]
    reg [31:0]  rs1_r;     // valor escalar (para vmv.v.x)
    reg         is_vmvvx_r;
    reg         is_vmvxs_r;
    reg         is_vredsum_r;
    reg         is_vmul_r;
    reg [31:0]  scalar_result_r;
    reg         has_scalar_r;

    // -------------------------------------------------------------------------
    //  Calculo combinacional OPIVV sobre operandos registrados
    // -------------------------------------------------------------------------
    wire [31:0] res_ivv [0:3];
    genvar g;
    generate
        for (g = 0; g < 4; g = g+1) begin : elem
            wire [31:0] a = op_a_r[32*g +: 32];
            wire [31:0] b = op_b_r[32*g +: 32];
            assign res_ivv[g] =
                (funct6_r == 6'b000000) ? (a +  b)      :
                (funct6_r == 6'b000010) ? (a -  b)      :
                (funct6_r == 6'b001001) ? (a &  b)      :
                (funct6_r == 6'b001010) ? (a |  b)      :
                (funct6_r == 6'b001011) ? (a ^  b)      :
                (funct6_r == 6'b100101) ? (a << b[4:0]) :
                (funct6_r == 6'b101000) ? (a >> b[4:0]) :
                                          32'b0;
        end
    endgenerate

    // vmul.vv - multiplicacion 32x32 bits, resultado bajo 32 bits
    // Vivado sintetiza esto con DSPs de la Artix-7
    wire [31:0] res_mul [0:3];
    generate
        for (g = 0; g < 4; g = g+1) begin : mul_elem
            assign res_mul[g] = op_a_r[32*g +: 32] * op_b_r[32*g +: 32];
        end
    endgenerate

    // vredsum.vs - reduccion suma
    // vd[0] = vs1[0] + suma(vs2[i] para i en 0..vl-1)
    // Calculo combinacional sobre op_a_r (vs2) y op_b_r (vs1)
    wire [31:0] vred_sum =
        op_b_r[31:0] +                                      // vs1[0] (acumulador)
        op_a_r[31:0] +                                      // vs2[0]
        (vl_r > 1 ? op_a_r[63:32]  : 32'b0) +             // vs2[1] si vl>1
        (vl_r > 2 ? op_a_r[95:64]  : 32'b0) +             // vs2[2] si vl>2
        (vl_r > 3 ? op_a_r[127:96] : 32'b0);              // vs2[3] si vl>3

    // -------------------------------------------------------------------------
    //  FSM - IDLE -> EXEC -> DONE -> WAIT -> IDLE
    //  + estado RESET para limpiar el banco vectorial en cada resetn
    // -------------------------------------------------------------------------
    localparam S_RESET = 3'd0;
    localparam S_IDLE  = 3'd1;
    localparam S_EXEC  = 3'd2;
    localparam S_DONE  = 3'd3;
    localparam S_WAIT  = 3'd4;

    reg [2:0] state;

    reg [2:0]  rst_cnt;   // contador para limpiar 8 registros en reset
    reg        ready_r;

    integer i;

    always @(posedge clk) begin
        ready_r <= 0;

        if (dbg_we)
            vreg[dbg_reg_sel][dbg_elem_sel*32 +: 32] <= dbg_wdata;

        if (!resetn) begin
            state           <= S_RESET;
            rst_cnt         <= 3'd0;
            scalar_result_r <= 32'b0;
            has_scalar_r    <= 0;
        end else begin

            // Estado de reset: limpiar banco registro a registro
            if (state == S_RESET) begin
                vreg[rst_cnt] <= 128'b0;
                if (rst_cnt == 3'd7)
                    state <= S_IDLE;
                else
                    rst_cnt <= rst_cnt + 3'd1;
            end else
            case (state)

                S_IDLE: begin
                    has_scalar_r <= 0;
                    if (is_any) begin
                        funct6_r     <= funct6;
                        vm_r         <= vm_in;
                        vs2_r        <= vs2_in;
                        vs1_r        <= vs1_in;
                        vd_r         <= vd_in;
                        vl_r         <= csr_vl;
                        op_a_r       <= vreg[vs2_in];
                        op_b_r       <= vreg[vs1_in];
                        rs1_r        <= pcpi_rs1;
                        is_vmvvx_r   <= is_vmvvx;
                        is_vmvxs_r   <= is_vmvxs;
                        is_vredsum_r <= is_vredsum;
                        is_vmul_r    <= is_vmul;
                        state        <= S_EXEC;
                    end
                end

                S_EXEC: begin
                    if (is_vmvvx_r) begin
                        // vmv.v.x: copiar rs1 a todos los elementos < vl
                        for (i = 0; i < 4; i = i+1)
                            if (i < vl_r)
                                vreg[vd_r][32*i +: 32] <= rs1_r;

                    end else if (is_vmvxs_r) begin
                        // vmv.x.s: devolver elemento[0] de vs2 al CPU
                        scalar_result_r <= op_a_r[31:0];
                        has_scalar_r    <= 1;

                    end else if (is_vredsum_r) begin
                        // vredsum.vs: solo escribe elemento[0] del destino
                        vreg[vd_r][31:0] <= vred_sum;
                        // elementos [3:1] del destino no se tocan

                    end else if (is_vmul_r) begin
                        // vmul.vv: multiplicacion elemento a elemento
                        for (i = 0; i < 4; i = i+1)
                            if ((i < vl_r) && (vm_r || vreg[0][32*i]))
                                vreg[vd_r][32*i +: 32] <= res_mul[i];

                    end else begin
                        // OPIVV: vadd, vsub, vand, vor, vxor, vsll, vsrl
                        for (i = 0; i < 4; i = i+1)
                            if ((i < vl_r) && (vm_r || vreg[0][32*i]))
                                vreg[vd_r][32*i +: 32] <= res_ivv[i];
                    end
                    state <= S_DONE;
                end

                S_DONE: begin
                    ready_r <= 1;
                    state   <= S_WAIT;
                end

                S_WAIT: begin
                    state <= S_IDLE;
                end

                default: state <= S_IDLE;
            endcase
        end
    end

    // -------------------------------------------------------------------------
    //  Salidas PCPI
    // -------------------------------------------------------------------------
    assign pcpi_wait  = is_any ||
                        (state == S_RESET) ||
                        (state == S_EXEC)  ||
                        (state == S_DONE);
    assign pcpi_ready = ready_r;
    assign pcpi_wr    = ready_r && has_scalar_r;
    assign pcpi_rd    = scalar_result_r;

endmodule
