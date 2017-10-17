#!/bin/sh

TMPDIR=$(mktemp -d)
CDROOT=$TMPDIR/cdroot

mkdir -p $CDROOT/boot/grub
cp x1 $CDROOT/boot
cat > $CDROOT/boot/grub/grub.cfg << EOF
set timeout=1

menuentry "X1" --class os {
        multiboot (hd96)/boot/x1
}
EOF
grub-mkrescue -o $TMPDIR/grub.iso $CDROOT

qemu-system-i386 \
          -enable-kvm \
          -ctrl-grab \
          -gdb tcp::1234 \
          -m 64 \
          -monitor stdio \
          -cdrom $TMPDIR/grub.iso \
          -boot d

rm -rf $TMPDIR
