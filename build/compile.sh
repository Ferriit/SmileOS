#!/bin/bash
set -e

SRC_DIR="../src"
DIST_DIR="../dist"
ISO_DIR="$DIST_DIR/isodir"

echo "[*] Cleaning..."
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
rm -rf "$ISO_DIR"

echo "[*] Compiling..."
nasm -f elf32 "$SRC_DIR/boot.s" -o "$DIST_DIR/boot.o"
nasm -f elf32 "$SRC_DIR/lib.asm" -o "$DIST_DIR/lib_asm.o"
gcc -m32 -Os -s -ffreestanding -fno-stack-protector -O2 -Wall -Wextra -c "$SRC_DIR/kernel.c" -o "$DIST_DIR/kernel.o"

gcc -m32 -Os -s -ffreestanding -fno-stack-protector -O2 -Wall -Wextra -c "$SRC_DIR/lib.c" -o "$DIST_DIR/lib.o"
gcc -m32 -Os -s -ffreestanding -fno-stack-protector -O2 -Wall -Wextra -c "$SRC_DIR/keyb.c" -o "$DIST_DIR/keyb.o"

echo "[*] Linking..."
ld -m elf_i386 -T linker.ld -o "$DIST_DIR/kernel.elf" \
    "$DIST_DIR/kernel.o" "$DIST_DIR/lib.o" "$DIST_DIR/keyb.o" "$DIST_DIR/lib_asm.o" "$DIST_DIR/boot.o"

echo "[*] Building ISO directory..."
mkdir -p "$ISO_DIR/boot/grub"
strip "$DIST_DIR/kernel.elf"
cp "$DIST_DIR/kernel.elf" "$ISO_DIR/boot/"

cat > "$ISO_DIR/boot/grub/grub.cfg" <<EOF
set timeout=0
set default=0

menuentry "SmileOS" {
    multiboot /boot/kernel.elf
    boot
}
EOF

echo "[*] Creating ISO..."
grub-mkrescue -o "$DIST_DIR/SmileOS.iso" "$ISO_DIR"


echo "[✅] Done. Create a disc image with this command:"
echo "qemu-img create -f raw disk.img 16K"
echo "qemu-system-i386 -cdrom $DIST_DIR/SmileOS.iso -drive file=disk.img,format=raw"
