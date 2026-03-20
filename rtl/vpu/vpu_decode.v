// =============================================================================
//  vpu_decode.v — OE1: Decodificador PCPI + control vsetvli/vsetvl
//  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
//
//  Este modulo es el punto de entrada de toda instruccion RVV a la VPU.
//  Responsabilidades:
//    1. Detectar si la instruccion es RVV (opcode = 1010111)
//    2. Identificar vsetvli vs vsetvl
//    3. Calcular el nuevo vl = min(rs1, VLMAX)
//    4. Actualizar csr_vl y csr_vtype
//    5. Responder al protocolo PCPI en 2 ciclos
//
//  Diseno con expansion en mente:
//    Cuando se agreguen OE2 (VALU) y OE3 (LSU), este modulo se expandira
//    para delegar instrucciones a los modulos correctos. Los registros
//    csr_vl y csr_vtype son compartidos con toda la VPU.
//
//  PARAMETROS DE LA VPU:
//    VLEN  = 128 bits (longitud del registro vectorial)
//    EEW   = 32  bits (elemento soportado: solo e32)
//    VLMAX = VLEN / EEW = 4 elementos
//
//  CODIFICACION RVV (spec v1.0):
//    Todas las instrucciones RVV usan opcode = 7'b1010111
//    vsetvli: funct3=111, insn[31]=0     — vtype en inmediato zimm[10:0]
//    vsetvl:  funct3=111, insn[31:30]=11 — vtype en registro rs2
//
//  FSM (2 ciclos):
//    IDLE  → detecta instruccion, captura rs1/rs2, calcula vl
//    DONE  → actualiza CSRs, pulsa pcpi_ready=1, vuelve a IDLE
//
//  pcpi_wait es COMBINACIONAL — se aserta en el mismo ciclo que
//  pcpi_valid para evitar el timeout de 16 ciclos del PicoRV32.
// =============================================================================

`timescale 1 ns / 1 ps

module vpu_decode #(
    parameter VLEN  = 128,   // bits por registro vectorial
    parameter EEW   = 32     // bits por elemento (solo e32 soportado)
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

    // Estado vectorial — exportado para OE2/OE3
    output [31:0] csr_vl,
    output [31:0] csr_vtype
);

    // -------------------------------------------------------------------------
    //  Constantes derivadas de parametros
    //  VLMAX = cuantos elementos de EEW bits caben en VLEN bits
    // -------------------------------------------------------------------------
    localparam VLMAX = VLEN / EEW;  // = 4 para VLEN=128, EEW=32

    // -------------------------------------------------------------------------
    //  Decodificacion de campos de la instruccion
    //
    //  Formato de vsetvli (tipo I extendido):
    //    [31]    = 0          distingue de vsetvl
    //    [30:20] = zimm[10:0] inmediato con vtype
    //    [19:15] = rs1        numero de elementos pedidos
    //    [14:12] = 111        funct3
    //    [11:7]  = rd         registro destino
    //    [6:0]   = 1010111    opcode RVV
    //
    //  Formato de vsetvl (tipo R):
    //    [31:30] = 11         identifica vsetvl
    //    [29:25] = 00000      funct7 parcial
    //    [24:20] = rs2        registro con vtype
    //    [19:15] = rs1
    //    [14:12] = 111
    //    [11:7]  = rd
    //    [6:0]   = 1010111
    // -------------------------------------------------------------------------
    wire is_rvv     = (pcpi_insn[6:0]   == 7'b1010111);
    wire is_vsetcfg = is_rvv && (pcpi_insn[14:12] == 3'b111);
    wire is_vsetvli = is_vsetcfg && (pcpi_insn[31]    == 1'b0);
    wire is_vsetvl  = is_vsetcfg && (pcpi_insn[31:30] == 2'b11);
    wire is_vset    = (is_vsetvli || is_vsetvl) && pcpi_valid;

    // Extraer campos utiles
    wire [10:0] zimm    = pcpi_insn[30:20]; // vtype inmediato (vsetvli)
    wire [4:0]  rd_addr = pcpi_insn[11:7];  // registro destino

    // -------------------------------------------------------------------------
    //  Calculo combinacional de vtype candidato
    //
    //  vtype[2:0]  = vlmul — agrupamiento de registros
    //  vtype[5:3]  = vsew  — ancho del elemento
    //  vtype[31]   = vill  — configuracion ilegal
    //
    //  Solo soportamos vsew=010 (e32) y vlmul=000 (m1).
    //  Cualquier otra combinacion activa vill=1.
    // -------------------------------------------------------------------------
    // Para vsetvli: vtype viene del zimm
    // Para vsetvl:  vtype viene de pcpi_rs2
    wire [31:0] vtype_req = is_vsetvli ? {21'b0, zimm} : pcpi_rs2;

    wire [2:0] vsew_req  = vtype_req[5:3];
    wire [2:0] vlmul_req = vtype_req[2:0];

    // vill = 1 si la configuracion no es soportada
    // Solo soportamos vsew=010 (32 bits) y vlmul=000 (LMUL=1)
    wire vill = (vsew_req  != 3'b010) ||
                (vlmul_req != 3'b000);

    wire [31:0] vtype_new = vill ? 32'h8000_0000 :  // bit31=vill
                                   {26'b0, vsew_req, vlmul_req};

    // -------------------------------------------------------------------------
    //  Calculo combinacional de vl nuevo
    //
    //  Tres casos segun la especificacion RVV v1.0:
    //    rs1 != 0             → vl = min(rs1, VLMAX)
    //    rs1 == 0, rd != x0   → vl no cambia (consulta de VLMAX)
    //    rs1 == 0, rd == x0   → vl no cambia
    // -------------------------------------------------------------------------
    wire rs1_is_zero = (pcpi_rs1 == 32'b0);
    wire rd_is_x0    = (rd_addr  == 5'b0);

    wire [31:0] vl_new = rs1_is_zero         ? 32'd0        :  // no cambia (ver FSM)
                         (pcpi_rs1 > VLMAX)  ? VLMAX        :
                                               pcpi_rs1;

    // Valor de rd a devolver al CPU
    wire [31:0] rd_val = rs1_is_zero ? VLMAX : vl_new;

    // -------------------------------------------------------------------------
    //  FSM — 2 ciclos
    // -------------------------------------------------------------------------
    localparam S_IDLE = 1'b0;
    localparam S_DONE = 1'b1;

    reg        state;
    reg [31:0] csr_vl_r;
    reg [31:0] csr_vtype_r;
    reg [31:0] result_r;
    reg        ready_r;
    reg        wr_r;

    always @(posedge clk) begin
        ready_r <= 0;
        wr_r    <= 0;

        if (!resetn) begin
            state      <= S_IDLE;
            csr_vl_r   <= 32'b0;
            csr_vtype_r<= 32'b0;
            result_r   <= 32'b0;
        end else begin
            case (state)

                S_IDLE: begin
                    if (is_vset) begin
                        // Actualizar CSRs
                        csr_vtype_r <= vtype_new;

                        // vl solo cambia si rs1 != 0
                        if (!rs1_is_zero)
                            csr_vl_r <= vl_new;
                        // si rs1 == 0, csr_vl_r no cambia

                        // Capturar resultado para rd
                        result_r <= rd_val;

                        state <= S_DONE;
                    end
                end

                S_DONE: begin
                    // Pulsar ready por 1 ciclo y volver a IDLE
                    ready_r <= 1;
                    wr_r    <= !rd_is_x0; // solo escribir rd si no es x0
                    state   <= S_IDLE;
                end

            endcase
        end
    end

    // -------------------------------------------------------------------------
    //  pcpi_wait — COMBINACIONAL
    //  Alto cuando detectamos is_vset (ciclo 0) o cuando estamos en DONE
    // -------------------------------------------------------------------------
    assign pcpi_wait  = is_vset || (state == S_DONE);
    assign pcpi_ready = ready_r;
    assign pcpi_wr    = wr_r;
    assign pcpi_rd    = result_r;

    // Exportar CSRs para uso de OE2/OE3
    assign csr_vl    = csr_vl_r;
    assign csr_vtype = csr_vtype_r;

endmodule
