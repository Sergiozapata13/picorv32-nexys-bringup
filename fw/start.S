.section .text
.global _start

_start:
    # Inicializar stack desde el linker
    la   sp, _stack_top
    addi sp, sp, -4

    # Limpiar .bss
    la   a0, _sbss
    la   a1, _ebss
    bge  a0, a1, 2f

1:
    sw   zero, 0(a0)
    addi a0, a0, 4
    blt  a0, a1, 1b

2:
    call main

3:
    j 3b
