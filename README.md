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
| OE3 | `vpu_lsu.v` | 🔲 | 🔲 | vle32/vse32, acceso a memoria |
| OE4 | Integracion | 🔲 | 🔲 | Sintesis completa + benchmarks |

---

## Instrucciones implementadas (OE2)

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

Todas las instrucciones respetan `vl` activo (tail elements intactos) y predicacion
por mascara `v0` (vm=0).

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
  vadd.vv / vmul.vv / vredsum.vs: verificados en hardware, 23/23 PASS
  Banco 8x128b: lectura y escritura correctas con vl parcial y mascara v0
```

**Hipotesis del TFG:** el sistema completo con VALU procesando 4 elementos en paralelo
deberia alcanzar >= 30% de mejora en ciclos/elemento sobre la implementacion escalar,
con < 5,000 LUTs adicionales sobre el nucleo base.

---

## Hallazgos tecnicos relevantes

### 1. pcpi_wait debe ser combinacional
El PicoRV32 tiene un timeout de 16 ciclos para instrucciones PCPI. Si `pcpi_wait`
se registra (always @posedge clk), hay un ciclo donde `pcpi_valid=1` y `pcpi_wait=0`,
iniciando el contador. Solucion: `assign pcpi_wait = is_my_insn || (state != S_IDLE)`.

### 2. -march=rv32im genera instrucciones div hardware
Con `ENABLE_DIV=0` en PicoRV32, el CPU trapa silenciosamente al encontrar `divu`.
Solucion: compilar con `-march=rv32i -lgcc` para usar division software via `__udivsi3`.

### 3. Vivado cachea resultados de sintesis
Cambios en `firmware.hex` no se reflejan sin reset completo del run.
Procedimiento: `reset_run synth_1` en la consola Tcl antes de resintetizar.

### 4. Captura de operandos en S_IDLE (HT-OE2a)
En una FSM PCPI multiciclo, todos los operandos deben capturarse en registros durante
S_IDLE mientras `pcpi_valid=1`. Calculos que dependen de senales derivadas de `pcpi_valid`
en ciclos posteriores producen siempre cero.

### 5. Instrucciones vectoriales custom y ABI de GCC (HT-OE2b)
Instrucciones vectoriales con `.word` en bloques `asm volatile` separados permiten que
GCC corrompa los registros entre instrucciones. Usar bloques `asm` extendidos con `li`/`mv`
directos y sin llamadas a funciones C intermedias. Patron correcto:

```asm
asm volatile (
    "li a0, <val_vs2>\n"
    ".word <vmv.v.x vs2>\n"
    "li a0, <val_vs1>\n"
    ".word <vmv.v.x vs1>\n"
    ".word <vop.vv vd,vs2,vs1>\n"
    ".word <vmv.x.s a0,vd>\n"
    "mv %0, a0\n"
    : "=r"(result) : : "a0", "memory"
);
```

### 6. Latencia de escritura al banco vectorial (HT-OE2c)
Cuando dos instrucciones PCPI se ejecutan consecutivamente, la segunda puede capturar
el banco antes de que la primera complete su escritura. Solucion: estado `S_WAIT` en
la FSM (IDLE->EXEC->DONE->WAIT->IDLE).

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
│       └── vpu_alu.v           # OE2: VALU vectorial + banco 8x128b
├── sim/                        # Testbenches Verilator
│   ├── tb_pcpi_example.cpp
│   ├── tb_pcpi_multicycle.cpp
│   ├── tb_vpu_decode.cpp
│   ├── tb_vpu_alu.cpp
│   └── Makefile
├── fw/                         # Firmware RISC-V
│   ├── main.c
│   ├── start.S
│   ├── link.ld
│   ├── build.sh
│   └── bin2hex32.py
├── constraints/
│   └── nexys_a7.xdc
└── docs/
    ├── Informe_Avances_RVV_lite.docx
    ├── HT-OE2_Captura_Operandos_PCPI.docx
    └── HT-OE2_Hallazgos_VALU_PCPI.docx
```

---

## Como correr las simulaciones

### Requisitos
- Verilator >= 4.0 (`sudo apt install verilator g++ make`)
- riscv64-unknown-elf-gcc (`sudo apt install gcc-riscv64-unknown-elf`)

### Simulacion de todos los modulos

```bash
cd sim
make all        # corre todos los testbenches
make etapa-b    # solo pcpi_example
make etapa-c    # solo pcpi_multicycle
make oe1        # solo vpu_decode
make oe2        # solo vpu_alu
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
5. Reset completo de sintesis en Vivado Tcl:
   ```tcl
   reset_run synth_1
   launch_runs impl_1 -to_step write_bitstream -jobs 4
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
