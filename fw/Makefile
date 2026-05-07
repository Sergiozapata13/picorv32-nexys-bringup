# =============================================================================
#  Makefile — Compila todos los firmwares de benchmarks
#  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
#
#  Uso:
#    make all              compila todos los .hex
#    make dotprod          solo dot product
#    make BENCH=dotprod deploy   compila dotprod y copia a Vivado
#    make clean            limpiar
#
#  Cada benchmark vive en bench_apps/main_<NAME>.c y produce build/<NAME>.hex
# =============================================================================

# ─── Toolchain ───────────────────────────────────────────────────────────────
CC      = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
PYTHON  = python3

CFLAGS  = -march=rv32i -mabi=ilp32 -O2 -nostdlib -Wall
LDFLAGS = -Wl,-T,link.ld
LIBS    = -lgcc

# ─── Directorios ─────────────────────────────────────────────────────────────
SHARED_DIR = shared
APP_DIR    = bench_apps
BUILD_DIR  = build

INCLUDES = -I$(SHARED_DIR)

# Destino para Vivado (ajustar segun ubicacion del proyecto)
VIVADO_SRC ?= /mnt/c/FPGA/Picorv32/Picorv32.srcs/sources_1/new

# ─── Fuentes compartidas ─────────────────────────────────────────────────────
SHARED_SRC = $(SHARED_DIR)/uart.c $(SHARED_DIR)/bench.c
START_SRC  = start.S
LINK_SCR   = link.ld

# ─── Lista de benchmarks (todos los main_*.c en bench_apps) ──────────────────
BENCHES = $(notdir $(basename $(wildcard $(APP_DIR)/main_*.c)))
# main_dotprod -> dotprod
NAMES   = $(BENCHES:main_%=%)

# Archivos finales: build/<name>.hex para cada benchmark
HEX_FILES = $(addprefix $(BUILD_DIR)/, $(addsuffix .hex, $(NAMES)))

# ─── Targets principales ─────────────────────────────────────────────────────
.PHONY: all clean help list deploy
.DEFAULT_GOAL := all

all: $(HEX_FILES)
	@echo ""
	@echo "Compilados $(words $(NAMES)) benchmarks:"
	@for n in $(NAMES); do echo "  build/$$n.hex"; done

# Reglas individuales por benchmark (permite "make dotprod")
$(NAMES): %: $(BUILD_DIR)/%.hex

# Compilar un .hex desde main_<name>.c + fuentes compartidas
$(BUILD_DIR)/%.hex: $(APP_DIR)/main_%.c $(SHARED_SRC) $(START_SRC) $(LINK_SCR) | $(BUILD_DIR)
	@echo "[CC]      $<"
	@$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) \
	    $(START_SRC) $< $(SHARED_SRC) $(LIBS) \
	    -o $(BUILD_DIR)/$*.elf
	@echo "[OBJCOPY] $(BUILD_DIR)/$*.elf -> $(BUILD_DIR)/$*.bin"
	@$(OBJCOPY) -O binary $(BUILD_DIR)/$*.elf $(BUILD_DIR)/$*.bin
	@echo "[HEX]     $(BUILD_DIR)/$*.bin -> $(BUILD_DIR)/$*.hex"
	@$(PYTHON) bin2hex32.py $(BUILD_DIR)/$*.bin $(BUILD_DIR)/$*.hex

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ─── Deploy a Vivado ─────────────────────────────────────────────────────────
# Uso: make BENCH=dotprod deploy
BENCH ?= dotprod
deploy: $(BUILD_DIR)/$(BENCH).hex
	@echo "[DEPLOY]  $(BUILD_DIR)/$(BENCH).hex -> $(VIVADO_SRC)/firmware.hex"
	@cp $(BUILD_DIR)/$(BENCH).hex "$(VIVADO_SRC)/firmware.hex"
	@echo "Listo. Recompilar bitstream en Vivado (reset_run synth_1 -> launch impl_1)."

# ─── Utilidades ──────────────────────────────────────────────────────────────
list:
	@echo "Benchmarks disponibles:"
	@for n in $(NAMES); do echo "  - $$n   (bench_apps/main_$$n.c)"; done

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Limpieza completa."

help:
	@echo "Makefile — TFG RVV-lite firmware build system"
	@echo ""
	@echo "Targets:"
	@echo "  all                 Compilar todos los benchmarks (default)"
	@echo "  <nombre>            Compilar un benchmark especifico (ej: make dotprod)"
	@echo "  deploy BENCH=<n>    Copiar build/<n>.hex como firmware.hex en Vivado"
	@echo "  list                Listar benchmarks disponibles"
	@echo "  clean               Borrar build/"
	@echo "  help                Esta ayuda"
	@echo ""
	@echo "Variables:"
	@echo "  VIVADO_SRC          Ruta del directorio sources_1/new del proyecto Vivado"
	@echo "                      (default: $(VIVADO_SRC))"
