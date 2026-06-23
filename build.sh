#!/bin/bash
set -e

# Configuration
KERNEL_BIN="bin/bedi_os.bin"
ISO_NAME="bedi-x86_64.iso"
ISO_ROOT="bin/iso_root"
OBJ_DIR="bin/obj"
RUST_DIR="src/rust"

# Tools
CC="gcc"
AS="nasm"
LD="ld"
CARGO="cargo"
RUSTC="rustc"

# Flags
CFLAGS="-ffreestanding -fno-stack-protector -fno-stack-check -fno-lto -fno-PIE -fno-PIC \
        -m64 -march=x86-64 -mno-80387 -mno-mmx -mno-red-zone \
        -mcmodel=kernel -Iinclude -c"
ASFLAGS="-f elf64"
LDFLAGS="-T config/linker.ld -static -nostdlib -no-pie -z max-page-size=0x1000 -m elf_x86_64"

function clean() {
    echo "[CLEAN] Removing build artifacts..."
    rm -rf bin
    rm -f $ISO_NAME
    (cd $RUST_DIR && $CARGO clean 2>/dev/null || true)
}

function check_tools() {
    for tool in $CC $AS $LD xorriso; do
        if ! command -v $tool &> /dev/null; then
            echo "[ERROR] $tool is not installed."
            exit 1
        fi
    done
    if ! command -v $CARGO &> /dev/null; then
        echo "[ERROR] Cargo is not installed."
        exit 1
    fi
}

function build() {
    check_tools
    echo "[BUILD] Starting BEDI OS build process..."

    # Create directories
    mkdir -p $OBJ_DIR
    mkdir -p $ISO_ROOT/boot

    # 1. Build Rust static library
    echo "[BUILD] Building Rust kernel module..."
    (cd $RUST_DIR && $CARGO build --release --target x86_64-unknown-none 2>&1)

    RUST_LIB="$RUST_DIR/target/x86_64-unknown-none/release/libbedi_rust.a"
    if [ ! -f "$RUST_LIB" ]; then
        echo "[ERROR] Rust build failed - library not found at $RUST_LIB"
        ls "$RUST_DIR/target/x86_64-unknown-none/release/" 2>/dev/null || true
        exit 1
    fi
    echo "[BUILD] Rust library ready: $RUST_LIB"

    # 2. Compile C files
    echo "[BUILD] Compiling C source files..."
    C_SOURCES=$(find src -name "*.c" -not -path "*/rust/*")
    for src in $C_SOURCES; do
        rel_path=${src#src/}
        obj="$OBJ_DIR/${rel_path%.c}.o"
        mkdir -p "$(dirname "$obj")"
        
        echo "  CC $src -> $obj"
        # Enable optimizations for crypto and networking to speed up math
        if [[ "$src" == *"crypto"* ]] || [[ "$src" == *"net"* ]]; then
            $CC $CFLAGS -O2 -mno-sse -mno-sse2 $src -o $obj
        else
            $CC $CFLAGS $src -o $obj
        fi
    done

    # 3. Compile ASM files
    echo "[BUILD] Compiling ASM source files..."
    ASM_SOURCES=$(find src/kernel -name "*.asm")
    for src in $ASM_SOURCES; do
        rel_path=${src#src/}
        obj="$OBJ_DIR/${rel_path%.asm}.o"
        mkdir -p "$(dirname "$obj")"

        echo "  AS $src -> $obj"
        $AS $ASFLAGS $src -o $obj
    done

    # 4. Link the kernel with Rust static library
    echo "[BUILD] Linking kernel into $KERNEL_BIN..."
    ALL_OBJ=$(find $OBJ_DIR -name "*.o")
    $LD $LDFLAGS $ALL_OBJ $RUST_LIB -o $KERNEL_BIN

    # 5. Prepare ISO root
    echo "[BUILD] Preparing ISO root..."
    cp $KERNEL_BIN $ISO_ROOT/boot/
    cp config/limine.conf $ISO_ROOT/boot/

    # 6. Fetch Limine binaries
    LIMINE_DIR="tools/bootloader"
    if [ ! -f "$LIMINE_DIR/limine-bios.sys" ]; then
        echo "[ERROR] Limine binaries not found in $LIMINE_DIR!"
        exit 1
    fi

    cp "$LIMINE_DIR/limine-bios.sys" $ISO_ROOT/boot/
    cp "$LIMINE_DIR/limine-bios-cd.bin" $ISO_ROOT/boot/
    cp "$LIMINE_DIR/limine-uefi-cd.bin" $ISO_ROOT/boot/

    mkdir -p $ISO_ROOT/EFI/BOOT
    cp "$LIMINE_DIR/BOOTX64.EFI" $ISO_ROOT/EFI/BOOT/
    cp "$LIMINE_DIR/BOOTIA32.EFI" $ISO_ROOT/EFI/BOOT/

    # 7. Create ISO
    echo "[BUILD] Creating ISO image..."
    xorriso -as mkisofs -b boot/limine-bios-cd.bin \
            -no-emul-boot -boot-load-size 4 -boot-info-table \
            --efi-boot boot/limine-uefi-cd.bin \
            -efi-boot-part --efi-boot-image \
            $ISO_ROOT -o $ISO_NAME 2>/dev/null

    # 8. Install Limine to ISO
    echo "[BUILD] Installing Limine to ISO..."
    ./tools/bootloader/limine bios-install $ISO_NAME &>/dev/null

    echo "[SUCCESS] BEDI OS build complete: $ISO_NAME"
}

function run() {
    if [ ! -f $ISO_NAME ]; then
        build
    fi

    QEMU="qemu-system-x86_64"
    if ! command -v $QEMU &> /dev/null; then
        echo "[ERROR] QEMU is not installed."
        exit 1
    fi

    QEMU_FLAGS="-cdrom $ISO_NAME -m 2G -smp 4 -audiodev pa,id=snd0 -machine pcspk-audiodev=snd0 -serial stdio \
                -net nic,model=e1000 -net user"
    
    if [ -e /dev/kvm ]; then
        QEMU_FLAGS="$QEMU_FLAGS -cpu host -enable-kvm"
    else
        echo "[INFO] KVM not available, using default CPU emulation."
        QEMU_FLAGS="$QEMU_FLAGS -cpu qemu64"
    fi

    echo "[RUN] Launching QEMU..."
    $QEMU $QEMU_FLAGS
}

case "$1" in
    "build")
        build
        ;;
    "clean")
        clean
        ;;
    "run"|"-r"|"--run")
        run
        ;;
    "help"|*)
        echo "Usage: $0 {build|clean|run|help}"
        echo "  build: Compiles Rust+C kernel and creates the ISO"
        echo "  clean: Removes build artifacts"
        echo "  run:   Builds (if necessary) and runs the ISO in QEMU"
        echo "  help:  Shows this help message"
        ;;
esac
