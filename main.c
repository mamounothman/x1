#include <defs.h>
#include <stdint.h>

#define VIDEO_ADDR 0xb8000

uint8_t stack[STACK_SIZE] __attribute__ ((aligned (4)));

static void
print(const char *s)
{
    volatile uint16_t *video;

    video = (volatile uint16_t *)VIDEO_ADDR;

    while (*s != '\0') {
        *video = (0x7 << 8) | *s;
        s++;
        video++;
    }
}

void
main(void)
{
    print("Hello, world!");
    for (;;);
}
