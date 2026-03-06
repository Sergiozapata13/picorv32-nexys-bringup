.section .text
.global _start

_start:
    la sp, 0x0000FFFC
    call main

1:
    j 1b
