/* SPDX-License-Identifier: GPL-3.0-only */
/* A loaded mystic machine (firmware/src/arcave/mm.h; tools/mm_build.py links with this): the whole of it at its slot
 * of the machine bank, where it runs in place. Slot 0 is flash 0x97000, FM1_XIP(0x97000) = 0x02093000 (MM_BASE); the
 * tool links a second time two slots on to find the addresses in it, which the MYSTIC MACHINES page moves to whichever
 * slot the machine is sent to. A machine has no .data and no .bss: its RAM is lent by the firmware (mm_head_t.state),
 * and the tool refuses one that has either. */
MEMORY
{
    SLOT (rx) : ORIGIN = 0x02093000, LENGTH = 0xA000
}
SECTIONS
{
    .machine : { KEEP(*(.mm_head)) *(.text .text.*) *(.rodata .rodata.* .srodata .srodata.*) . = ALIGN(4); } > SLOT
    .data : { *(.data .data.* .sdata .sdata.*) } > SLOT
    .bss : { *(.bss .bss.* .sbss .sbss.* COMMON) } > SLOT
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
