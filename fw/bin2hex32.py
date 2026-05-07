#!/usr/bin/env python3
# =============================================================================
#  bin2hex32.py — Convierte un .bin a formato hex little-endian para $readmemh
#  TFG: RVV-lite sobre PicoRV32 — Sergio, TEC
#
#  Uso:
#    python3 bin2hex32.py                          (firmware.bin -> firmware.hex)
#    python3 bin2hex32.py input.bin                (input.bin -> input.hex)
#    python3 bin2hex32.py input.bin output.hex     (rutas explicitas)
# =============================================================================
import sys
from pathlib import Path

if len(sys.argv) == 1:
    in_path  = Path("firmware.bin")
    out_path = Path("firmware.hex")
elif len(sys.argv) == 2:
    in_path  = Path(sys.argv[1])
    out_path = in_path.with_suffix(".hex")
else:
    in_path  = Path(sys.argv[1])
    out_path = Path(sys.argv[2])

data = in_path.read_bytes()

# Rellenar hasta multiplo de 4 bytes
while len(data) % 4 != 0:
    data += b"\x00"

with open(out_path, "w") as f:
    for i in range(0, len(data), 4):
        word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24)
        f.write(f"{word:08x}\n")
