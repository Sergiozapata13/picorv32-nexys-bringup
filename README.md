PicoRV32 Bring-up on Nexys A7

Bare-metal firmware test for PicoRV32 running on a Nexys A7 FPGA board.

Features tested:
- Boot from internal RAM
- Memory mapped GPIO at 0x10000000
- LED running pattern test

Build firmware:

./build.sh

Toolchain:
riscv64-unknown-elf-gcc

Baseline funcional PicoRV32 Nexys A7

Estado:
- PicoRV32 ejecutando firmware desde RAM
- GPIO LEDs en 0x10000000
- Patrón de LEDs desplazándose confirmado en hardware

Toolchain:
Vivado 2025.2
riscv64-unknown-elf-gcc

Frecuencia:
100 MHz

Resultados implementación:
WNS: 1.147 ns
LUT: ~10176
FF: ~952
BRAM: 0
Power: ~0.129 W

Fecha: 2026-03-05
