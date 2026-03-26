# RVV-lite sobre PicoRV32 via PCPI — FPGA Artix-7

**Trabajo Final de Graduacion (TFG)**
Escuela de Ingenieria Electronica — Instituto Tecnologico de Costa Rica

Implementacion de un subconjunto funcional de la extension vectorial RISC-V (RVV v1.0)
como coprocesador PCPI del nucleo PicoRV32, sintetizado en una FPGA Nexys A7-100T (Artix-7).

---

## Que hace este proyecto

La extension vectorial RISC-V (RVV) permite procesar multiples datos en paralelo (SIMD).
Este proyecto implementa un subconjunto denominado **RVV-lite** directamente en hardware
sobre una FPGA de bajo costo, conectado al procesador PicoRV32 mediante su interfaz PCPI.

El objetivo es demostrar que es posible acelerar kernels de procesamiento de senales
(producto punto, filtro FIR) con menos de 5,000 LUTs adicionales sobre el nucleo base.

---

## Arquitectura del sistema

```
+--------------------------------------------------+
|                  Nexys A7-100T                   |
|                                                  |
|   +-------------+        +------------------+   |
|   |  PicoRV32   |        |    VPU (RVV-lite)|   |
|   |  RV32I core |<-PCPI->|  vpu_decode.v    |   |
|   |  100 MHz    |        |  vpu_alu.v  (OE2)|   |
|   +------+------+        |  vpu_lsu.v  (OE3)|   |
|          |               +------------------+   |
|          |                                      |
|   +------+------+   +----------+   +--------+   |
|   |  BRAM 64KB  |   |  UART    |   |  GPIO  |   |
|   |  firmware   |   | 115200bd |   |  LEDs  |   |
|   +-------------+   +----------+   +--------+   |
+--------------------------------------------------+

Mapa de memoria:
  0x0000_0000 - 0x0000_FFFF  RAM (BRAM 64 KiB)
  0x1000_0000                GPIO LEDs
  0x2000_0000                UART divisor baudrate
  0x2000_0004                UART TX/RX dato
```

---

## Estado actual

| Etapa | Modulo | Simulacion | Hardware | Descripcion |
|-------|--------|-----------|---------|-------------|
| A | SoC base | — | ✅ | PicoRV32 + BRAM + UART + GPIO |
| B | `pcpi_example.v` | ✅ 14/14 | ✅ 6/6 | Instruccion custom 1 ciclo |
| C | `pcpi_multicycle.v` | ✅ 14/14 | ✅ 6/6 | FSM multiciclo, pcpi_wait sostenido |
| OE1 | `vpu_decode.v` | ✅ 21/21 | ✅ 9/9 | vsetvli/vsetvl, CSRs vl/vtype |
| OE2 | `vpu_alu.v` | ✅ 57/57 | ✅ 23/23 | VALU vectorial + banco 8x128b |
| OE3 | `vpu_lsu.v` | ✅ 28/28 | ✅ 20/20 | vle32/vse32, acceso a memoria |
| OE4 | Integracion | 🔲 | 🔲 | Sintesis completa + benchmarks |

---

## Instrucciones implementadas

### OE2 — VALU vectorial

| Instruccion | Tipo | Operacion | Estado |
|-------------|------|-----------|--------|
| `vadd.vv` | OPIVV | `vd[i] = vs2[i] + vs1[i]` | ✅ sim + HW |
| `vsub.vv` | OPIVV | `vd[i] = vs2[i] - vs1[i]` | ✅ sim + HW |
| `vand.vv` | OPIVV | `vd[i] = vs2[i] & vs1[i]` | ✅ sim + HW |
| `vor.vv`  | OPIVV | `vd[i] = vs2[i] \| vs1[i]` | ✅ sim + HW |
| `vxor.vv` | OPIVV | `vd[i] = vs2[i] ^ vs1[i]` | ✅ sim + HW |
| `vsll.vv` | OPIVV | `vd[i] = vs2[i] << vs1[i][4:0]` | ✅ sim + HW |
| `vsrl.vv` | OPIVV | `vd[i] = vs2[i] >> vs1[i][4:0]` | ✅ sim + HW |
| `vmul.vv` | OPMVV | `vd[i] = vs2[i] * vs1[i]` (32b bajo) | ✅ sim + HW |
| `vredsum.vs` | OPMVV | `vd[0] = vs1[0] + sum(vs2[i], i<vl)` | ✅ sim + HW |

### OE3 — Acceso a memoria vectorial

| Instruccion | Operacion | Estado |
|-------------|-----------|--------|
| `vle32.v vd, (rs1)` | Carga vl palabras de 32b desde mem[rs1+i*4] a vreg[vd] | ✅ sim + HW |
| `vse32.v vs3, (rs1)` | Escribe vl palabras de 32b desde vreg[vs3] a mem[rs1+i*4] | ✅ sim + HW |

Todas las instrucciones respetan `vl` activo (tail elements intactos).

---

## Resultados medidos en hardware

Mediciones sobre Nexys A7-100T a 100 MHz via UART (rdcycle):

```
Etapa B — custom.add (rd = rs1 + rs2 + 1):
  PCPI 1 ciclo:    32 ciclos/op
  ADD escalar:     33 ciclos/op
  Overhead PCPI fijo: ~30 ciclos

Etapa C — custom.slowmul (rd = rs1 * rs2, latencia FSM 8 ciclos):
  PCPI multiciclo: 47 ciclos/op
  MUL escalar:    238 ciclos/op   (__mulsi3 de libgcc)
  Aceleracion:     5x

OE1 — vsetvli (configuracion vectorial):
  Latencia:       < 5 ciclos
  Patron de loop vectorial N=10, VLMAX=4: 3 iteraciones (4+4+2) OK

OE2 — VALU vectorial (4 elementos en paralelo):
  vadd.vv / vmul.vv / vredsum.vs: 23/23 tests PASS en hardware
  Banco 8x128b: lectura y escritura correctas con vl parcial y mascara v0

OE3 — Acceso a memoria vectorial:
  vle32.v / vse32.v: 20/20 tests PASS en hardware
  Patron benchmark: vle32 + vadd.vv + vse32 verificado en hardware
  Roundtrip completo: carga, operacion y almacenamiento de vectores de 4 elementos
```

**Hipotesis del TFG:** el sistema completo con VALU procesando 4 elementos en paralelo
deberia alcanzar >= 30% de mejora en ciclos/elemento sobre la implementacion escalar,
con < 5,000 LUTs adicionales sobre el nucleo base.

---

## Hallazgos tecnicos relevantes

### HT-B — pcpi_wait debe ser combinacional
El PicoRV32 tiene un timeout de 16 ciclos para instrucciones PCPI. Si `pcpi_wait`
se registra, hay un ciclo donde `pcpi_valid=1` y `pcpi_wait=0`, iniciando el contador.
Solucion: `assign pcpi_wait = is_my_insn || (state != S_IDLE)`.

### HT-C — Capturar operandos antes de que pcpi_valid baje
En una FSM PCPI multiciclo, el CPU baja `pcpi_valid` en cuanto el coprocesador aserta
`pcpi_wait`. Todos los campos de la instruccion y valores de registros deben capturarse
en S_IDLE antes de transicionar a S_EXEC.

### HT-OE2a — Calcular resultados con operandos registrados
Calculos que dependen de senales derivadas de `pcpi_valid` en ciclos posteriores a S_IDLE
producen siempre cero. Usar operandos capturados en registros durante S_IDLE.

### HT-OE2b — Instrucciones vectoriales custom y ABI de GCC
Instrucciones vectoriales con `.word` en bloques `asm volatile` separados permiten que
GCC corrompa los registros entre instrucciones. Usar bloques `asm` extendidos con `li`/`mv`
directos y sin llamadas a funciones C intermedias. Patron correcto:

```asm
asm volatile (
    "mv a0, %0\n"          // cargar base en a0
    ".word <vle32.v vd>\n" // instruccion vectorial usa a0
    "mv a0, %1\n"
    ".word <vle32.v vd2>\n"// si dos loads, usar a0/a1 distintos
    : : "r"(base1), "r"(base2) : "a0", "a1", "memory"
);
```

### HT-OE2c — Estado S_WAIT entre instrucciones consecutivas
Cuando dos instrucciones PCPI se ejecutan consecutivamente, la segunda puede capturar
el banco vectorial antes de que la primera complete su escritura. Solucion: estado
`S_WAIT` en la FSM (IDLE -> EXEC -> DONE -> WAIT -> IDLE).

### HT-OE3a — Senales de handshake fuera de los defaults del always block
`lsu_mem_valid` en los defaults del `always` block causa que el CPU quede colgado:
la senal se resetea a 0 cada ciclo antes de que S_WAIT_READY pueda mantenerla alta.
Solucion: manejar `lsu_mem_valid` explicitamente en cada estado de la FSM.

### HT-OE3b — Ready prematuro por transaccion pendiente del CPU
Cuando `lsu_mem_valid` sube por primera vez (elemento 0), el CPU puede tener
`mem_valid=1` pendiente (prefetch). El arbitro de bus presenta esa transaccion a la
RAM, que responde con `ready=1` — pero ese ready corresponde al CPU, no a la LSU.
Solucion: registrar `lsu_mem_valid` un ciclo atras (`lsu_valid_prev`) y usar ese
registro para enrutar `lsu_mem_ready`.

```verilog
reg lsu_valid_prev;
always @(posedge clk) lsu_valid_prev <= lsu_mem_valid;

assign lsu_mem_ready = lsu_valid_prev ? ready_r : 1'b0;
assign lsu_mem_rdata = lsu_valid_prev ? rdata_r : 32'b0;
```

### HT-OE3c — Multi-load requiere registros base distintos
Dos instrucciones `vle32` consecutivas que usen el mismo registro base (`a0`) en
bloques `asm` separados permiten que GCC reutilice `a0` para calcular la segunda
direccion. Usar `a0` para el primer vector y `a1` para el segundo en un bloque
`asm` unificado.

---

## Estructura del repositorio

```
.
├── rtl/
│   ├── core/                   # PicoRV32, simpleuart (dependencias)
│   └── vpu/                    # Modulos VPU propios
│       ├── pcpi_example.v      # Etapa B: PCPI 1 ciclo
│       ├── pcpi_multicycle.v   # Etapa C: PCPI multiciclo FSM
│       ├── vpu_decode.v        # OE1: decodificador vsetvli/vsetvl
│       ├── vpu_alu.v           # OE2: VALU vectorial + banco 8x128b
│       └── vpu_lsu.v           # OE3: Load/Store vectorial vle32/vse32
├── sim/                        # Testbenches Verilator
│   ├── tb_pcpi_example.cpp
│   ├── tb_pcpi_multicycle.cpp
│   ├── tb_vpu_decode.cpp
│   ├── tb_vpu_alu.cpp
│   ├── tb_vpu_lsu.cpp
│   └── Makefile
├── fw/                         # Firmware RISC-V
│   ├── main.c
│   ├── start.S
│   ├── link.ld
│   ├── build.sh
│   └── bin2hex32.py
├── top.v                       # Top-level SoC con arbitro de bus OE2+OE3
├── constraints/
│   └── nexys_a7.xdc
└── docs/
    ├── Informe_Avances_RVV_lite.docx
    ├── HT-OE2_Captura_Operandos_PCPI.docx
    ├── HT-OE2_Hallazgos_VALU_PCPI.docx
    └── HT-OE3_Hallazgos_LSU_PCPI.docx
```

---

## Como correr las simulaciones

### Requisitos
- Verilator >= 4.0 (`sudo apt install verilator g++ make`)
- riscv64-unknown-elf-gcc (`sudo apt install gcc-riscv64-unknown-elf`)

### Simulacion de todos los modulos

```bash
cd sim
make all        # corre todos los testbenches en orden
make etapa-b    # solo pcpi_example
make etapa-c    # solo pcpi_multicycle
make oe1        # solo vpu_decode
make oe2        # solo vpu_alu
make oe3        # solo vpu_lsu
```

### Compilar firmware

```bash
cd fw
./build.sh      # compila y copia firmware.hex al proyecto Vivado
```

---

## Como correr en hardware

1. Abrir proyecto en Vivado (Nexys A7-100T, xc7a100tcsg324-1)
2. Agregar sources: `rtl/core/picorv32.v`, `rtl/core/simpleuart.v`, `rtl/vpu/*.v`, `top.v`
3. Agregar constraints: `constraints/nexys_a7.xdc`
4. Compilar firmware: `cd fw && ./build.sh`
5. Sintetizar e implementar en Vivado Tcl:
   ```tcl
   reset_run synth_1
   reset_run impl_1
   launch_runs synth_1 -jobs 6
   wait_on_run synth_1
   launch_runs impl_1 -to_step write_bitstream -jobs 6
   wait_on_run impl_1
   ```
6. Program Device
7. Abrir terminal serie: COM_X, 115200 baud, 8N1

---

## Configuracion del CPU

```verilog
picorv32 #(
    .ENABLE_PCPI         (1),  // Habilita interfaz de coprocesador
    .ENABLE_MUL          (0),  // Off — VPU maneja todo via PCPI
    .ENABLE_DIV          (0),  // Off — usar -lgcc para division software
    .ENABLE_REGS_DUALPORT(1),  // rs1+rs2 en el mismo ciclo (requerido por PCPI)
    .COMPRESSED_ISA      (0),  // rv32i — consistente con -march=rv32i
    .ENABLE_COUNTERS     (1),  // rdcycle disponible para benchmarks
    .PROGADDR_RESET      (32'h0000_0000),
    .STACKADDR           (32'h0000_FFFC)
)
```

---

## Toolchain

```bash
# Compilar firmware
riscv64-unknown-elf-gcc \
    -march=rv32i -mabi=ilp32 -O2 -nostdlib \
    -Wl,-T,link.ld start.S main.c -lgcc \
    -o firmware.elf

# Convertir a hex para $readmemh
riscv64-unknown-elf-objcopy -O binary firmware.elf firmware.bin
python3 bin2hex32.py
```

---

## Referencias

- RISC-V "V" Vector Extension Specification v1.0 — RISC-V International, 2021
- PicoRV32 — Claire Wolf, YosysHQ (https://github.com/YosysHQ/picorv32)
- Jacobs et al., "RISC-V V Vector Extension with Reduced Register File", ISVLSI 2024
- Nexys A7 Reference Manual — Digilent, 2022

---

*TFG desarrollado en el Laboratorio de Diseno Digital, Escuela de Ingenieria Electronica, TEC.*
*Curso: IE-0499 Proyecto Electrico — 2026*
