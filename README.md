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
