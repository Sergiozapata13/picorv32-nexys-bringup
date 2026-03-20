#!/bin/bash
set -e

riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -O2 -nostdlib \
  -Wl,-T,link.ld start.S main.c -o firmware.elf

riscv64-unknown-elf-objcopy -O binary firmware.elf firmware.bin

python3 bin2hex32.py

cp firmware.hex /mnt/c/FPGA/Picorv32/Picorv32.srcs/sources_1/new/

echo "Firmware compilado y copiado a Vivado."
