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
|   +-------------+     +----------------------+  |
|   |  PicoRV32   |     |    vpu_pcpi (OE4)    |  |
|   |  RV32I core |<--->|  vpu_decode  (OE1)   |  |
|   |  100 MHz    | PCPI|  vpu_alu     (OE2)   |  |
|   +------+------+     |  vpu_lsu     (OE3)   |  |
|          |            +----------+-----------+  |
|          |                       | bus LSU       |
|   +------+------+   +----------+ |  +--------+  |
|   |  BRAM 64KB  |<--+ arbitro  +<+  |  UART  |  |
|   |  firmware   |   | bus mem  |    |  GPIO  |  |
|   +-------------+   +----------+    +--------+  |
+--------------------------------------------------+

Mapa de memoria:
  0x0000_0000 - 0x0000_FFFF  RAM (BRAM 64 KiB)
  0x1000_0000                GPIO LEDs
  0x2000_0000                UART divisor baudrate
  0x2000_0004                UART TX/RX dato
```

---

## Estado del proyecto — COMPLETO

| Etapa | Modulo | Simulacion | Hardware | Descripcion |
|-------|--------|-----------|---------|-------------|
| A | SoC base | — | ✅ | PicoRV32 + BRAM + UART + GPIO |
| B | `pcpi_example.v` | ✅ 14/14 | ✅ 6/6 | Instruccion custom 1 ciclo |
| C | `pcpi_multicycle.v` | ✅ 14/14 | ✅ 6/6 | FSM multiciclo, pcpi_wait sostenido |
| OE1 | `vpu_decode.v` | ✅ 21/21 | ✅ 9/9 | vsetvli/vsetvl, CSRs vl/vtype |
| OE2 | `vpu_alu.v` | ✅ 57/57 | ✅ 23/23 | VALU vectorial + banco 8x128b |
| OE3 | `vpu_lsu.v` | ✅ 28/28 | ✅ 20/20 | vle32/vse32, acceso a memoria |
| OE4 | `vpu_pcpi.v` | ✅ | ✅ | Integracion completa + benchmarks |

---

## Instrucciones implementadas

### OE1 — Configuracion vectorial

| Instruccion | Operacion | Estado |
|-------------|-----------|--------|
| `vsetvli rd, rs1, vtypei` | Configura vl = min(rs1, VLMAX), vtype segun vtypei | ✅ sim + HW |
| `vsetvl  rd, rs1, rs2`    | Configura vl = min(rs1, VLMAX), vtype = rs2        | ✅ sim + HW |

### OE2 — VALU vectorial (EEW=32, VLEN=128, VLMAX=4)

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

## Resultados medidos en hardware — Nexys A7-100T a 100 MHz

### Microbenchmarks (latencia por instruccion)

```
Etapa B — custom.add (rd = rs1 + rs2 + 1):
  PCPI 1 ciclo:       32 ciclos/op
  ADD escalar:        33 ciclos/op
  Overhead PCPI fijo: ~30 ciclos

Etapa C — custom.slowmul (rd = rs1 * rs2, FSM 8 ciclos):
  PCPI multiciclo:   47 ciclos/op
  MUL escalar:      238 ciclos/op  (__mulsi3 libgcc)
  Aceleracion:        5x
```

### Benchmarks OE4 — Dot product y FIR, N=32 elementos

| Kernel | Ciclos escalar | Ciclos vectorial | Ciclos/elem esc | Ciclos/elem vec | **Mejora** |
|--------|---------------|-----------------|----------------|----------------|-----------|
| Dot product | 2,385 | 848 | 74 | 26 | **64%** |
| FIR (N=32 coefs) | 189,113 | 71,636 | 5,909 | 2,238 | **62%** |

**Hipotesis del TFG verificada:** mejora >= 30% en ciclos/elemento — **cumplida con 62-64%.**

---

## Hallazgos tecnicos relevantes

Un conjunto de 9 hallazgos documentados durante el desarrollo, organizados por capa:

### RTL — Protocolo PCPI

**HT-B — pcpi_wait debe ser combinacional**
El PicoRV32 tiene un timeout de 16 ciclos para instrucciones PCPI. Si `pcpi_wait`
se registra, hay un ciclo donde `pcpi_valid=1` y `pcpi_wait=0`, iniciando el contador.
Solucion: `assign pcpi_wait = is_my_insn || (state != S_IDLE)`.

**HT-C — Capturar operandos antes de que pcpi_valid baje**
En una FSM PCPI multiciclo, el CPU baja `pcpi_valid` en cuanto el coprocesador aserta
`pcpi_wait`. Todos los campos de la instruccion y valores de registros deben capturarse
en S_IDLE antes de transicionar a S_EXEC.

**HT-OE2a — Calcular resultados con operandos registrados**
Calculos que dependen de senales derivadas de `pcpi_valid` en ciclos posteriores a S_IDLE
producen siempre cero porque `pcpi_valid` ya bajo.

**HT-OE2c — Estado S_WAIT entre instrucciones consecutivas**
Cuando dos instrucciones PCPI se ejecutan sin ciclos escalares entre ellas, la segunda
puede capturar el banco vectorial antes de que la primera complete su escritura.
Solucion: estado `S_WAIT` en la FSM (IDLE → EXEC → DONE → WAIT → IDLE).

**HT-OE3a — Senales de handshake fuera de los defaults del always block**
`lsu_mem_valid` en los defaults del `always` block causa que el CPU quede colgado —
la senal se resetea a 0 cada ciclo antes de que `S_WAIT_READY` pueda mantenerla alta.
Solucion: manejar `lsu_mem_valid` explicitamente en cada estado de la FSM.

**HT-OE4 — Reset del banco vectorial via FSM**
El banco vectorial (`reg [127:0] vreg[0:7]`) no se resetea con el bloque `initial`
cuando el usuario presiona reset — `initial` solo se ejecuta al cargar el bitstream.
Valores residuales entre resets contaminan resultados. Solucion: estado `S_RESET`
en la FSM que limpia los 8 registros secuencialmente (8 ciclos) antes de aceptar
instrucciones.

### Interfaz de bus

**HT-OE3b — Ready prematuro por transaccion pendiente del CPU**
Cuando `lsu_mem_valid` sube por primera vez, el CPU puede tener `mem_valid=1` pendiente
(prefetch). El arbitro presenta esa transaccion a la RAM y el ready resultante contamina
el primer elemento del vector. Solucion: `lsu_valid_prev` en `top.v`.

```verilog
reg lsu_valid_prev;
always @(posedge clk) lsu_valid_prev <= lsu_mem_valid;
assign lsu_mem_ready = lsu_valid_prev ? ready_r : 1'b0;
```

### Firmware / ABI

**HT-OE2b — Instrucciones vectoriales custom y ABI de GCC**
Instrucciones vectoriales con `.word` en bloques `asm volatile` separados permiten que
GCC corrompa los registros entre instrucciones. Usar bloques `asm` extendidos con `li`/`mv`
directos y sin llamadas a funciones C intermedias.

**HT-OE3c — Multi-load requiere registros base distintos**
Dos instrucciones `vle32` consecutivas que usen el mismo registro base en bloques
`asm` separados permiten que GCC reutilice ese registro para calcular la segunda
direccion. Usar `a0` para el primer vector y `a1` para el segundo en un bloque unificado.

### Herramientas

**Vivado — reset completo requerido para cambios en RTL**
```tcl
reset_run synth_1
reset_run impl_1
launch_runs synth_1 -jobs 6
wait_on_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 6
wait_on_run impl_1
```
Para cambios solo en firmware (main.c), basta con `reset_run synth_1` antes de impl_1.

**Toolchain — -march=rv32im genera instrucciones div hardware**
Con `ENABLE_DIV=0` en PicoRV32, el CPU trapa al encontrar `divu`.
Solucion: compilar con `-march=rv32i -lgcc`.

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
│       ├── vpu_lsu.v           # OE3: Load/Store vectorial vle32/vse32
│       └── vpu_pcpi.v          # OE4: wrapper VPU completa
├── sim/                        # Testbenches Verilator
│   ├── tb_pcpi_example.cpp
│   ├── tb_pcpi_multicycle.cpp
│   ├── tb_vpu_decode.cpp
│   ├── tb_vpu_alu.cpp
│   ├── tb_vpu_lsu.cpp
│   └── Makefile
├── fw/                         # Firmware RISC-V
│   ├── main.c                  # OE4: benchmarks dot product y FIR
│   ├── start.S
│   ├── link.ld
│   ├── build.sh
│   └── bin2hex32.py
├── top.v                       # Top-level SoC con vpu_pcpi + arbitro de bus
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
5. Sintetizar e implementar:
   ```tcl
   reset_run synth_1
   reset_run impl_1
   launch_runs synth_1 -jobs 6
   wait_on_run synth_1
   launch_runs impl_1 -to_step write_bitstream -jobs 6
   wait_on_run impl_1
   ```
6. Program Device
7. Terminal serie: COM_X, 115200 baud, 8N1
8. Resultado esperado: LEDs 0-7 encendidos, benchmarks impresos por UART

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
riscv64-unknown-elf-gcc \
    -march=rv32i -mabi=ilp32 -O2 -nostdlib \
    -Wl,-T,link.ld start.S main.c -lgcc \
    -o firmware.elf

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
