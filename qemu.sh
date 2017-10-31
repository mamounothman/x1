#!/bin/sh

# Safely create a temporary directory.
TMPDIR=$(mktemp -d)
CDROOT=$TMPDIR/cdroot

# Build a cdrom image with both the GRUB boot loader and the kernel binary.
mkdir -p $CDROOT/boot/grub
cp x1 $CDROOT/boot
cat > $CDROOT/boot/grub/grub.cfg << EOF
set timeout=1

menuentry "X1" --class os {
        multiboot (hd96)/boot/x1
}
EOF
grub-mkrescue -o $TMPDIR/grub.iso $CDROOT

# Start the QEMU emulator with options doing the following :
#  - GDB remote access on the local TCP port 1234
#  - 64MiB of physical memory (RAM)
#  - No video device (automatically sets the first COM port as the console)
#  - Boot from the generated cdrom image.
#
# In order to dump all exceptions and interrupts to a log file, you may add
# the following options :
#       -d int \
#       -D int_debug.log \
#
# Note that these debugging options do not work when KVM is enabled.
qemu-system-i386 \
        -gdb tcp::1234 \
        -m 64 \
        -nographic \
        -cdrom $TMPDIR/grub.iso \
        -boot d

rm -rf $TMPDIR
