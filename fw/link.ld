ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text :
    {
        . = ALIGN(4);
        _stext = .;
        *(.text)
        *(.text*)
        . = ALIGN(4);
        _etext = .;
    } > RAM

    .rodata :
    {
        . = ALIGN(4);
        _srodata = .;
        *(.rodata)
        *(.rodata*)
        *(.srodata)
        *(.srodata*)
        . = ALIGN(4);
        _erodata = .;
    } > RAM

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        *(.sdata)
        *(.sdata*)
        . = ALIGN(4);
        _edata = .;
    } > RAM

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    .heap :
    {
        . = ALIGN(4);
        _heap_start = .;
    } > RAM

    _ram_start  = ORIGIN(RAM);
    _ram_end    = ORIGIN(RAM) + LENGTH(RAM);
    _stack_top  = _ram_end;
}
