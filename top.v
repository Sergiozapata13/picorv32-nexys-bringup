// =============================================================================
//  top.v - SoC PicoRV32 + BRAM + UART + GPIO + VPU (OE2 + OE3)
//  TFG: RVV-lite sobre PicoRV32 - Sergio, TEC
//  Target: Nexys A7-100T (xc7a100tcsg324-1)
//
//  MAPA DE MEMORIA
//  ---------------
//  0x0000_0000 - 0x0000_FFFF   RAM  (BRAM 64 KiB)
//  0x1000_0000                 GPIO LEDs[15:0]
//  0x2000_0000                 UART divisor baud
//  0x2000_0004                 UART dato TX/RX
//
//  VPU: vpu_alu (OE2) + vpu_lsu (OE3)
//  Arbitro de bus: cuando lsu_mem_valid=1, la LSU controla el bus.
//  El CPU esta suspendido (pcpi_wait=1) durante las transacciones LSU.
//
//  FIX vs version anterior: ready_r y rdata_r se enrutan via assign
//  combinacional, no dentro del always block. Esto evita el bug de
//  "lsu_mem_ready <= ready_r" donde ready_r es el valor del ciclo anterior.
// =============================================================================

module top (
    input  wire        CLK100MHZ,
    input  wire        BTNC,
    output reg  [15:0] LED,
    output wire        uart_tx,
    input  wire        uart_rx
);

    // -------------------------------------------------------------------------
    //  Reset
    // -------------------------------------------------------------------------
    reg [3:0] rst_ff = 4'h0;
    always @(posedge CLK100MHZ)
        rst_ff <= {rst_ff[2:0], ~BTNC};
    wire resetn = rst_ff[3];

    // -------------------------------------------------------------------------
    //  Bus de memoria PicoRV32
    // -------------------------------------------------------------------------
    wire        mem_valid;
    wire        mem_instr;
    wire        mem_ready;    // wire — driven por assign al final
    wire [31:0] mem_addr;
    wire [31:0] mem_wdata;
    wire  [3:0] mem_wstrb;
    wire [31:0] mem_rdata;    // wire — driven por assign al final

    wire        mem_la_read;
    wire        mem_la_write;
    wire [31:0] mem_la_addr;
    wire [31:0] mem_la_wdata;
    wire  [3:0] mem_la_wstrb;

    // -------------------------------------------------------------------------
    //  Interfaz PCPI - OR de vpu_alu y vpu_lsu
    // -------------------------------------------------------------------------
    wire        pcpi_valid;
    wire [31:0] pcpi_insn;
    wire [31:0] pcpi_rs1;
    wire [31:0] pcpi_rs2;
    wire        pcpi_wr;
    wire [31:0] pcpi_rd;
    wire        pcpi_wait;
    wire        pcpi_ready;

    wire        alu_wr,    lsu_wr;
    wire [31:0] alu_rd,    lsu_rd;
    wire        alu_wait,  lsu_wait;
    wire        alu_ready, lsu_ready;

    assign pcpi_wr    = alu_wr    | lsu_wr;
    assign pcpi_rd    = alu_ready ? alu_rd : lsu_rd;
    assign pcpi_wait  = alu_wait  | lsu_wait;
    assign pcpi_ready = alu_ready | lsu_ready;

    // -------------------------------------------------------------------------
    //  Bus LSU
    // -------------------------------------------------------------------------
    wire        lsu_mem_valid;
    wire [31:0] lsu_mem_addr;
    wire [31:0] lsu_mem_wdata;
    wire  [3:0] lsu_mem_wstrb;
    wire        lsu_mem_ready;   // wire — driven por assign
    wire [31:0] lsu_mem_rdata;   // wire — driven por assign

    // Arbitro: cuando lsu_mem_valid=1, LSU controla el bus
    wire        eff_valid = lsu_mem_valid ? lsu_mem_valid : mem_valid;
    wire [31:0] eff_addr  = lsu_mem_valid ? lsu_mem_addr  : mem_addr;
    wire [31:0] eff_wdata = lsu_mem_valid ? lsu_mem_wdata : mem_wdata;
    wire  [3:0] eff_wstrb = lsu_mem_valid ? lsu_mem_wstrb : mem_wstrb;

    // -------------------------------------------------------------------------
    //  Banco vectorial - puertos para LSU (usa dbg_* de vpu_alu)
    // -------------------------------------------------------------------------
    wire        lsu_vreg_we;
    wire  [2:0] lsu_vreg_waddr;
    wire  [1:0] lsu_vreg_welem;
    wire [31:0] lsu_vreg_wdata;
    wire  [2:0] lsu_vreg_raddr;
    wire  [1:0] lsu_vreg_relem;
    wire [31:0] lsu_vreg_rdata;

    // -------------------------------------------------------------------------
    //  OE2 - vpu_alu
    // -------------------------------------------------------------------------
    vpu_alu u_vpu_alu (
        .clk          (CLK100MHZ),
        .resetn       (resetn),
        .pcpi_valid   (pcpi_valid),
        .pcpi_insn    (pcpi_insn),
        .pcpi_rs1     (pcpi_rs1),
        .pcpi_rs2     (pcpi_rs2),
        .pcpi_wr      (alu_wr),
        .pcpi_rd      (alu_rd),
        .pcpi_wait    (alu_wait),
        .pcpi_ready   (alu_ready),
        .csr_vl       (32'd4),
        .csr_vtype    (32'h010),
        .dbg_reg_sel  (lsu_vreg_we ? lsu_vreg_waddr : lsu_vreg_raddr),
        .dbg_elem_sel (lsu_vreg_we ? lsu_vreg_welem : lsu_vreg_relem),
        .dbg_rdata    (lsu_vreg_rdata),
        .dbg_we       (lsu_vreg_we),
        .dbg_wdata    (lsu_vreg_wdata)
    );

    // -------------------------------------------------------------------------
    //  OE3 - vpu_lsu
    // -------------------------------------------------------------------------
    vpu_lsu u_vpu_lsu (
        .clk            (CLK100MHZ),
        .resetn         (resetn),
        .pcpi_valid     (pcpi_valid),
        .pcpi_insn      (pcpi_insn),
        .pcpi_rs1       (pcpi_rs1),
        .pcpi_rs2       (pcpi_rs2),
        .pcpi_wr        (lsu_wr),
        .pcpi_rd        (lsu_rd),
        .pcpi_wait      (lsu_wait),
        .pcpi_ready     (lsu_ready),
        .csr_vl         (32'd4),
        .lsu_mem_valid  (lsu_mem_valid),
        .lsu_mem_addr   (lsu_mem_addr),
        .lsu_mem_wdata  (lsu_mem_wdata),
        .lsu_mem_wstrb  (lsu_mem_wstrb),
        .lsu_mem_ready  (lsu_mem_ready),
        .lsu_mem_rdata  (lsu_mem_rdata),
        .vreg_we        (lsu_vreg_we),
        .vreg_waddr     (lsu_vreg_waddr),
        .vreg_welem     (lsu_vreg_welem),
        .vreg_wdata     (lsu_vreg_wdata),
        .vreg_raddr     (lsu_vreg_raddr),
        .vreg_relem     (lsu_vreg_relem),
        .vreg_rdata     (lsu_vreg_rdata)
    );

    wire [31:0] eoi;
    wire        trap;

    // -------------------------------------------------------------------------
    //  PicoRV32
    // -------------------------------------------------------------------------
    picorv32 #(
        .ENABLE_COUNTERS     (1),
        .ENABLE_COUNTERS64   (0),
        .ENABLE_REGS_16_31   (1),
        .ENABLE_REGS_DUALPORT(1),
        .LATCHED_MEM_RDATA   (0),
        .TWO_STAGE_SHIFT     (1),
        .BARREL_SHIFTER      (0),
        .TWO_CYCLE_COMPARE   (0),
        .TWO_CYCLE_ALU       (0),
        .COMPRESSED_ISA      (0),
        .CATCH_MISALIGN      (1),
        .CATCH_ILLINSN       (1),
        .ENABLE_PCPI         (1),
        .ENABLE_MUL          (0),
        .ENABLE_FAST_MUL     (0),
        .ENABLE_DIV          (0),
        .ENABLE_IRQ          (0),
        .ENABLE_IRQ_QREGS    (0),
        .ENABLE_IRQ_TIMER    (0),
        .ENABLE_TRACE        (0),
        .REGS_INIT_ZERO      (1),
        .PROGADDR_RESET      (32'h0000_0000),
        .STACKADDR           (32'h0000_FFFC)
    ) cpu (
        .clk          (CLK100MHZ),
        .resetn       (resetn),
        .trap         (trap),
        .mem_valid    (mem_valid),
        .mem_instr    (mem_instr),
        .mem_ready    (mem_ready),
        .mem_addr     (mem_addr),
        .mem_wdata    (mem_wdata),
        .mem_wstrb    (mem_wstrb),
        .mem_rdata    (mem_rdata),
        .mem_la_read  (mem_la_read),
        .mem_la_write (mem_la_write),
        .mem_la_addr  (mem_la_addr),
        .mem_la_wdata (mem_la_wdata),
        .mem_la_wstrb (mem_la_wstrb),
        .pcpi_valid   (pcpi_valid),
        .pcpi_insn    (pcpi_insn),
        .pcpi_rs1     (pcpi_rs1),
        .pcpi_rs2     (pcpi_rs2),
        .pcpi_wr      (pcpi_wr),
        .pcpi_rd      (pcpi_rd),
        .pcpi_wait    (pcpi_wait),
        .pcpi_ready   (pcpi_ready),
        .irq          (32'b0),
        .eoi          (eoi),
        .trace_valid  (),
        .trace_data   ()
    );

    // -------------------------------------------------------------------------
    //  RAM - 64 KiB BRAM
    // -------------------------------------------------------------------------
    localparam RAM_WORDS = 16384;
    reg [31:0] ram [0:RAM_WORDS-1];
    initial $readmemh("firmware.hex", ram);

    wire [13:0] ram_waddr = eff_addr[15:2];

    // -------------------------------------------------------------------------
    //  Decodificacion de direcciones
    // -------------------------------------------------------------------------
    wire sel_ram      = (eff_addr[31:16] == 16'h0000);
    wire sel_gpio     = (eff_addr        == 32'h1000_0000);
    wire sel_uart     = (eff_addr[31:8]  == 24'h200000);
    wire uart_sel_div = sel_uart && (eff_addr[7:0] == 8'h00);
    wire uart_sel_dat = sel_uart && (eff_addr[7:0] == 8'h04);

    // -------------------------------------------------------------------------
    //  simpleuart
    // -------------------------------------------------------------------------
    wire [31:0] uart_div_do;
    wire [31:0] uart_dat_do;
    wire        uart_dat_wait;

    simpleuart uart_inst (
        .clk         (CLK100MHZ),
        .resetn      (resetn),
        .ser_tx      (uart_tx),
        .ser_rx      (uart_rx),
        .reg_div_we  (uart_sel_div && eff_valid && (|eff_wstrb) ? eff_wstrb : 4'b0),
        .reg_div_di  (eff_wdata),
        .reg_div_do  (uart_div_do),
        .reg_dat_we  (uart_sel_dat && eff_valid && (|eff_wstrb)),
        .reg_dat_re  (uart_sel_dat && eff_valid && (~|eff_wstrb)),
        .reg_dat_di  (eff_wdata),
        .reg_dat_do  (uart_dat_do),
        .reg_dat_wait(uart_dat_wait)
    );

    // -------------------------------------------------------------------------
    //  Logica de respuesta al bus — solo calcula ready y rdata
    //  El enrutamiento a CPU o LSU se hace via assign combinacional abajo
    // -------------------------------------------------------------------------
    reg        ready_r;
    reg [31:0] rdata_r;

    always @(posedge CLK100MHZ) begin
        ready_r <= 1'b0;
        rdata_r <= 32'h0;

        if (eff_valid) begin
            if (sel_ram) begin
                ready_r <= 1'b1;
                rdata_r <= ram[ram_waddr];
                if (eff_wstrb[0]) ram[ram_waddr][ 7: 0] <= eff_wdata[ 7: 0];
                if (eff_wstrb[1]) ram[ram_waddr][15: 8] <= eff_wdata[15: 8];
                if (eff_wstrb[2]) ram[ram_waddr][23:16] <= eff_wdata[23:16];
                if (eff_wstrb[3]) ram[ram_waddr][31:24] <= eff_wdata[31:24];
            end
            else if (sel_gpio) begin
                ready_r <= 1'b1;
                rdata_r <= {16'b0, LED};
                if (|eff_wstrb) LED <= eff_wdata[15:0];
            end
            else if (sel_uart) begin
                if (uart_sel_dat && (|eff_wstrb))
                    ready_r <= ~uart_dat_wait;
                else
                    ready_r <= 1'b1;
                rdata_r <= uart_sel_div ? uart_div_do : uart_dat_do;
            end
            else begin
                ready_r <= 1'b1;
                rdata_r <= 32'hDEAD_BEEF;
            end
        end

        if (trap) LED <= 16'hDEAD;
    end

    // -------------------------------------------------------------------------
    //  Enrutamiento combinacional de ready y rdata
    //
    //  Cuando lsu_mem_valid=1:
    //    - lsu_mem_ready y lsu_mem_rdata reciben ready_r/rdata_r
    //    - mem_ready=0 (CPU suspendido via pcpi_wait de todas formas)
    //    - mem_rdata=0 (no importa, CPU no leerá este ciclo)
    //
    //  Cuando lsu_mem_valid=0:
    //    - mem_ready y mem_rdata reciben ready_r/rdata_r
    //    - lsu_mem_ready=0, lsu_mem_rdata=0
    // -------------------------------------------------------------------------
    assign mem_ready     = lsu_mem_valid ? 1'b0    : ready_r;
    assign mem_rdata     = lsu_mem_valid ? 32'b0   : rdata_r;
    assign lsu_mem_ready = lsu_mem_valid ? ready_r : 1'b0;
    assign lsu_mem_rdata = lsu_mem_valid ? rdata_r : 32'b0;

endmodule
