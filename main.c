#include <defs.h>
#include <stdint.h>

uint8_t stack[STACK_SIZE] __attribute__ ((aligned (4)));

void
main(void)
{
    for (;;);
}
