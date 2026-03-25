This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Bare-metal firmware for a PicoRV32 RISC-V soft-core running on a Nexys A7 FPGA board. This is part of a TFG (final degree project) implementing RVV-lite (a lightweight RISC-V Vector extension) on top of PicoRV32. The firmware tests vector memory operations (vle32/vse32) and vector arithmetic (vadd.vv) using raw `.word` encodings injected via inline assembly, since these custom instructions are not in the standard toolchain.

## Build

```bash
./build.sh
```

This compiles with `riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32`, produces `firmware.elf` → `firmware.bin` → `firmware.hex` (via `bin2hex32.py`), then copies `firmware.hex` (and optionally `simpleuart.v`) to the Vivado sources directory at `/mnt/c/FPGA/Picorv32/Picorv32.srcs/sources_1/new`.

**Toolchain required:** `riscv64-unknown-elf-gcc` (part of RISC-V GNU toolchain)

## Memory Map

| Address        | Peripheral         |
|----------------|--------------------|
| `0x00000000`   | RAM (64 KB), firmware loads here |
| `0x0000F000`   | VDATA_BASE — vector test data area in RAM |
| `0x10000000`   | GPIO LEDs (memory-mapped, `volatile uint32_t`) |
| `0x20000000`   | UART divisor register |
| `0x20000004`   | UART data register |

Stack grows down from `0x00010000` (top of 64 KB RAM).

## Architecture

- **`start.S`** — Reset vector at `0x00000000`: initializes SP to `_stack_top`, zeroes `.bss`, calls `main`.
- **`link.ld`** — Single flat RAM region (64 KB at address 0). All sections (`.text`, `.rodata`, `.data`, `.bss`) placed sequentially; stack at the very top.
- **`main.c`** — Test harness. Initializes UART (115200 baud, divisor 868 for 100 MHz clock), then runs 5 tests using custom vector instructions encoded as raw `.word` immediates via `asm volatile`. Outputs results over UART and sets LEDs to `0x00FF` on full pass or `0xDEAD` on failure.
- **`bin2hex32.py`** — Converts `firmware.bin` to little-endian 32-bit word hex format (`firmware.hex`) required by `$readmemh` in Vivado.

## Custom Vector Instruction Encodings

Vector instructions are not recognized by `rv32i` GCC, so they are hand-encoded as `.word` directives:

| Macro          | Encoding       | Operation                        |
|----------------|----------------|----------------------------------|
| `VLE32_V1(base)` | `0x02056087` | `vle32.v v1, (a0)` |
| `VLE32_V2(base)` | `0x02056107` | `vle32.v v2, (a0)` |
| `VLE32_V3(base)` | `0x0205E187` | `vle32.v v3, (a0)` |
| `VSE32_V1(base)` | `0x020560A7` | `vse32.v v1, (a0)` |
| `VSE32_V2(base)` | `0x02056127` | `vse32.v v2, (a0)` |
| `VMVVX_V1(val)`  | `0x5E0550D7` | `vmv.v.x v1, a0`  |
| `VMVVX_V2(val)`  | `0x5E055157` | `vmv.v.x v2, a0`  |
| `VADD_V1_V2_V3()` | `0x022180D7` | `vadd.vv v1, v2, v3` |

The vector unit (VPU) is implemented in the FPGA RTL (not in this repo). Vector length is 4 elements (128-bit total, 4×32-bit).

## FPGA Context

- **Vivado version:** 2025.2
- **Clock:** 100 MHz → UART divisor = 100,000,000 / 115,200 ≈ 868
- **ISA configured in top.v:** `COMPRESSED_ISA=0`, so `-march=rv32i` (not `rv32im`) must match
- **`simpleuart.v`** — UART RTL from the YosysHQ/picorv32 repo; copied to Vivado sources by `build.sh` if present

## UART Output

Connect to the board at 115200 8N1. The firmware prints test results as `[PASS]`/`[FAIL]` lines and a final summary count. LED pattern `0x00FF` = all tests passed; `0xDEAD` = failure.
