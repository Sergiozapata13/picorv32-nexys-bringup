# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Verilog/C++ simulation of an RVV-lite (RISC-V Vector extension) coprocessor subsystem for PicoRV32, implemented as a TFG (Bachelor's Thesis). The coprocessor communicates with PicoRV32 via the PCPI protocol. Simulation uses Verilator.

## Build & Test Commands

```bash
make all        # Run all testbenches
make etapa-b    # Compile & test pcpi_example (single-cycle custom.add)
make etapa-c    # Compile & test pcpi_multicycle (8-cycle custom.slowmul)
make oe1        # Compile & test vpu_decode (vsetvli/vsetvl)
make oe2        # Compile & test vpu_alu (vector arithmetic + register file)
make oe3        # Compile & test vpu_lsu (vector load/store)
make clean      # Remove obj_dir/
```

Verilator flags used: `--cc --exe --build --trace -Wno-UNUSEDSIGNAL -Wno-CASEINCOMPLETE -Wno-WIDTHTRUNC`

## Architecture

### PCPI Protocol

All modules implement the same interface signals: `pcpi_valid`, `pcpi_insn`, `pcpi_rs1`, `pcpi_rs2`, `pcpi_wr`, `pcpi_rd`, `pcpi_wait`, `pcpi_ready`.

Critical constraint: **`pcpi_wait` must be asserted combinationally** (same cycle as `pcpi_valid`) to prevent PicoRV32's 16-cycle timeout from triggering. `pcpi_ready` is a 1-cycle pulse; `pcpi_wr` signals when `pcpi_rd` holds a valid scalar result.

### Module Responsibilities

| Module | Stage | Purpose |
|--------|-------|---------|
| `pcpi_example.v` | Etapa B | 1-cycle `custom.add`: rd = rs1 + rs2 + 1 |
| `pcpi_multicycle.v` | Etapa C | 8-cycle FSM `custom.slowmul`: rd = rs1 × rs2 |
| `vpu_decode.v` | OE1 | Decode `vsetvli`/`vsetvl`, manage CSR state (csr_vl, csr_vtype) |
| `vpu_alu.v` | OE2 | Vector arithmetic on 8×128-bit register file |
| `vpu_lsu.v` | OE3 | Element-by-element vector load/store via memory handshake |

### Vector Register File (OE2)

8 registers × 128 bits = 4 elements × 32 bits each. Parameters: VLEN=128, EEW=32, VLMAX=4. Only `e32m1` vtype is valid; others set `vill=1`.

OE2 supports: `vadd.vv`, `vsub.vv`, `vand.vv`, `vor.vv`, `vxor.vv`, `vsll.vv`, `vsrl.vv`, `vmul.vv`, `vredsum.vs`, `vmv.x.s`, `vmv.v.x`. Masking is via `vm` bit using vreg[0] as predicate.

OE2 FSM: IDLE → EXEC → DONE → WAIT → IDLE. OE3 FSM: IDLE → REQ → WAIT_READY → [loop per element] → DONE → WAIT → IDLE.

### Testbench Pattern

All `tb_*.cpp` testbenches follow: init DUT → reset → drive signals → poll outputs → check with `check(name, got, expected)` → print pass/fail summary. Debug ports (`dbg_reg_sel`, `dbg_elem_sel`, etc.) on OE2/OE3 allow direct register bank inspection without PCPI transactions.

### Instruction Encoding

Testbenches encode instructions manually using RISC-V field packing (opcode=1010111 for RVV, funct3, funct6, rs1/rs2/rd register fields). See existing encoding functions in the testbenches as reference.
