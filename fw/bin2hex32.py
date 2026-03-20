from pathlib import Path

data = Path("firmware.bin").read_bytes()

# rellenar hasta múltiplo de 4 bytes
while len(data) % 4 != 0:
    data += b"\x00"

with open("firmware.hex", "w") as f:
    for i in range(0, len(data), 4):
        word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24)
        f.write(f"{word:08x}\n")
