# =============================================================================
#  Makefile — Simulaciones Verilator
#  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
#
#  Uso (desde ~/sim_pcpi):
#    make all      — corre todos los testbenches en orden
#    make etapa-b  — pcpi_example    (PCPI 1 ciclo)
#    make etapa-c  — pcpi_multicycle (PCPI multiciclo FSM)
#    make oe1      — vpu_decode      (vsetvli/vsetvl)
#    make oe2      — vpu_alu         (VALU vectorial + banco 8x128b)
#    make clean    — elimina obj_dir
#
#  Los archivos .v y los testbenches .cpp deben estar en el mismo directorio.
# =============================================================================

VERILATOR = verilator
VFLAGS    = --cc --exe --build --trace \
            -Wno-UNUSEDSIGNAL -Wno-CASEINCOMPLETE -Wno-WIDTHTRUNC

.PHONY: all etapa-b etapa-c oe1 oe2 clean

# ─── all ─────────────────────────────────────────────────────────────────────
all: etapa-b etapa-c oe1 oe2
	@echo ""
	@echo "========================================"
	@echo "  Todos los testbenches completados"
	@echo "========================================"

# ─── Etapa B — pcpi_example ──────────────────────────────────────────────────
etapa-b:
	@echo ""
	@echo "=== Etapa B: pcpi_example ==="
	$(VERILATOR) $(VFLAGS) \
		pcpi_example.v tb_pcpi_example.cpp \
		-o sim_pcpi_example
	./obj_dir/sim_pcpi_example

# ─── Etapa C — pcpi_multicycle ───────────────────────────────────────────────
etapa-c:
	@echo ""
	@echo "=== Etapa C: pcpi_multicycle ==="
	$(VERILATOR) $(VFLAGS) \
		pcpi_multicycle.v tb_pcpi_multicycle.cpp \
		-o sim_pcpi_multicycle
	./obj_dir/sim_pcpi_multicycle

# ─── OE1 — vpu_decode ────────────────────────────────────────────────────────
oe1:
	@echo ""
	@echo "=== OE1: vpu_decode (vsetvli/vsetvl) ==="
	$(VERILATOR) $(VFLAGS) \
		vpu_decode.v tb_vpu_decode.cpp \
		-o sim_vpu_decode
	./obj_dir/sim_vpu_decode

# ─── OE2 — vpu_alu ───────────────────────────────────────────────────────────
oe2:
	@echo ""
	@echo "=== OE2: vpu_alu (VALU vectorial + banco 8x128b) ==="
	$(VERILATOR) $(VFLAGS) \
		vpu_alu.v tb_vpu_alu.cpp \
		-o sim_vpu_alu
	./obj_dir/sim_vpu_alu

# ─── Clean ───────────────────────────────────────────────────────────────────
clean:
	rm -rf obj_dir
	@echo "obj_dir eliminado."
