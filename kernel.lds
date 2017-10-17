MEMORY
{
    /* TODO Describe */
    RAM : ORIGIN = 0x100000, LENGTH = 63M
}

PHDRS
{
    hdr     PT_LOAD FLAGS(4);
    text    PT_LOAD FLAGS(5);
    data    PT_LOAD FLAGS(6);
}

SECTIONS
{
    .hdr : {
        *(.hdr)
    } > RAM : hdr

    .text : {
        /* TODO Add reference to documentation */
        *(.text)
    } > RAM : text

    .data : {
        *(.data)
    } > RAM : data

    .bss : {
        *(.bss)
    } > RAM : data

    /*
     * XXX A global offset section is created because of linking with libgcc.
     * Is it safe to discard it ?
     */
    /DISCARD/ : {
        *(.eh_frame)
    }
}
