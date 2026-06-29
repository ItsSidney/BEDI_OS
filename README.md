<picture>
  <source media="(prefers-color-scheme: dark)" srcset="misc/logo.png">
  <img src="misc/logo.png" alt="BEDI OS logo" align="right" width="220">
</picture>

# BEDI OS 🚀

[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Architecture](https://img.shields.io/badge/arch-x86__64-blue)]()

**BEDI OS** is a modern, low-level operating system created from scratch in C and Rust. It features a complete desktop GUI, BSD-style networking, a VFS with FAT32 support, an embedded mathematics engine, and a custom scripting language — all running on bare metal.

---

## 📑 Table of Contents

- [Features](#features)
- [Supported Architectures & Environments](#supported-architectures--environments)
- [Installation](#installation)
  - [Prerequisites](#prerequisites)
  - [Build from Source](#build-from-source)
  - [Run in QEMU](#run-in-qemu)
  - [Boot on Real Hardware](#boot-on-real-hardware)
- [Quickstart](#quickstart)
- [Configuration](#configuration)
- [Filesystem Layout](#filesystem-layout)
- [Operations](#operations)
- [Kernel Subsystems](#kernel-subsystems)
- [Application Index](#application-index)
- [Scripting (BEDIC)](#scripting-bedic)
- [Contributing](#contributing)
- [FAQ](#faq)
- [Legal](#legal)

---

## ✨ Features

| Layer | Capabilities |
|-------|---------------|
| **Graphics** | Hardware-accelerated framebuffer, software 3D rasterizer, compositing WM, desktop wallpapers, flat theme system |
| **GUI** | 40+ theme roles, virtual desktops, taskbar, drag/resize windows, Adwaita icons |
| **Input** | Keyboard, mouse (IntelliMouse wheel support), on-screen keyboard |
| **Filesystem** | VFS, RAMFS, FAT32, trash/restore, home directories |
| **Networking** | e1000 & virtio-net drivers, BSD IP/Ethernet stack, ICMP ping, DNS, UDP sockets |
| **Audio** | PC speaker + PCM pipeline, piano synth |
| **Math** | Vectors (2D/3D/4D), matrices, quaternions, expression parser, Newton/Simpson solvers |
| **Security** | Multi-user roles (ADMIN/USER/GUEST), per-session CR3, password hashing, ring-3 isolation |
| **Scripting** | BEDIC language — compile with `bcc`, run with `brun` |
| **Rust** | Linked as a static kernel library; safe allocator, process registry, Tetris game |

---

## 🏗️ Supported Architectures & Environments

### CPU Architectures
- **x86_64** — primary and only supported architecture.
  - **UEFI**: 64-bit boot (BOOTX64.EFI)
  - **BIOS**: Legacy boot via Limine BIOS CD

### Emulators / Hypervisors
| Platform | Status |
|----------|--------|
| QEMU (`q35`, KVM or TCG, `qemu-system-x86_64`) | ✅ Stable |
| GNOME Boxes | ✅ Compatible |
| VMware / VirtualBox | ⚠️ Untested (Limine UEFI required) |

### Real Hardware
| Device | Status |
|--------|--------|
| HP EliteBook (Intel iGPU) | ✅ Tested |
| Generic x86_64 UEFI PC | ✅ Expected compatible |
| BIOS-only legacy PC | ⚠️ Limine BIOS path required |

---

## 🛠️ Installation

### Prerequisites

```bash
# Debian / Ubuntu / Arch
sudo apt install gcc nasm xorriso qemu-system-x86  # or pacman -S base-devel nasm xorriso qemu-system-x86
rustup default stable
rustup target add x86_64-unknown-none
cargo install --force  # if needed
```

- `gcc` — C compiler (produces 64-bit freestanding object files)
- `nasm` — x86_64 assembler
- `ld` — GNU linker
- `xorriso` — ISO image creation
- `cargo` / `rustc` — Rust toolchain with `x86_64-unknown-none` target
- `qemu-system-x86_64` — for testing (optional)

### Build from Source

```bash
git clone https://github.com/ItsSidney/BEDI_OS.git
cd BEDI_OS

# Clean any stale object files
./build.sh clean

# Build kernel + ISO
./build.sh build
```

Output:
- `bin/bedi_os.bin` — raw 64-bit kernel ELF (freestanding)
- `bedi-x86_64.iso` — bootable ISO image (not committed to repo)

### Run in QEMU

```bash
./build.sh run
```

Flags used:
- `-m 4G` RAM
- `-smp 4` CPUs
- `-machine q35`
- PC speaker audio routed to PulseAudio (`-audiodev pa,id=snd0`)
- networking via user-mode + `e1000`
- `-rtc base=localtime`

If KVM is available, it is auto-detected and used (`-enable-kvm`).

### Boot on Real Hardware

1. Write the ISO to a USB drive:
   ```bash
   sudo dd if=bedi-x86_64.iso of=/dev/sdX bs=4M status=progress && sync
   ```
2. Boot from USB and select the Limine entry.
3. The OS will boot to the desktop GUI.

---

## ⚡ Quickstart

```bash
# Clone
git clone https://github.com/ItsSidney/BEDI_OS.git
cd BEDI_OS

# Build
./build.sh clean && ./build.sh build

# Run
./build.sh run

# View boot log
./build.sh log
```

Inside the OS:
- Use the **Terminal** app for shell commands.
- Write BEDIC scripts and run them via the editor or terminal.
- Open **File Explorer** to browse RAMFS and mounted FAT32 volumes.

---

## ⚙️ Configuration

| File | Purpose |
|------|---------|
| `config/limine.conf` | Bootloader configuration, timeout, entries |
| `config/linker.ld` | Kernel link script (physical layout, sections) |
| `build.sh` | Build orchestration (C + ASM + Rust + ISO) |
| `.gitignore` | Excludes `bin/`, `*.iso`, `*.bin`, Rust target, bootloader binaries |

### Build Config (`build.sh`)
- `CFLAGS`: `-ffreestanding`, `-mcmodel=kernel`, `-m64`, `-march=x86-64`
- `LDFLAGS`: static, nostdlib, no-PIE, 4 KB page size
- Network files are compiled with `-O0 -mno-sse` for deterministic behavior.

---

## 📁 Filesystem Layout

| Path | Description |
|------|-------------|
| `src/apps/` | GUI applications (terminal, editor, games, tools) |
| `src/boot/` | 16/32-bit boot assembly + GDT |
| `src/commands/` | BEDIC compiler (`bcc`) and runtime (`brun`) |
| `src/drivers/` | Video, input, audio, storage, network, PCI, ACPI |
| `src/engine/` | Math engine (standalone library) |
| `src/filesystem/` | VFS, RAMFS, FAT32 |
| `src/gfx/` | Splash screen, BMP utilities |
| `src/gui/` | Theme, WM, UI widgets, icons |
| `src/kernel/` | Core kernel, memory, security, net stack, tasking, syscalls |
| `src/rust/` | Rust kernel module (allocator, Tetris, process registry) |
| `src/libs/` | BMP helper library |
| `include/` | Mirror of headers for apps and kernel |
| `config/` | Linker script + boot config |
| `tools/` | Limine binaries (UEFI + BIOS) and Python conversion scripts |
| `docs/` | Documentation, tutorials |
| `misc/` | Logos and banners |

---

## 🧩 Operations

- `./build.sh build` — Full clean build (C + ASM + Rust + ISO)
- `./build.sh clean` — Remove `bin/`, ISO, and Rust artifacts
- `./build.sh run` — Build if needed, launch in QEMU
- `./build.sh log` — Run headless QEMU and dump serial log to `bedi_qemu.log`
- `git push origin master` — Submit source (ISO and binaries are excluded)

---

## 🧠 Kernel Subsystems

### Memory
- **Physical**: Page frame allocator.
- **Virtual**: 4-level x86_64 paging with HHDM.
- **Heap**: Kernel-space bump/block allocator.

### Scheduling
- Preemptive, fixed-priority tasks.
- Cooperative blocking via `sleep_task`.
- Background worker threads (networking, housekeeping).

### Security
- Multi-user support (`ADMIN`, `USER`, `GUEST`).
- Per-user `page_directory_physical` (CR3) for session isolation.
- Ring-3 transition support.

### I/O & Drivers
- **Video**: Limine FB, Intel GMA, VirtIO GPU.
- **Input**: PS/2 keyboard, IntelliMouse wheel-aware mouse driver.
- **Audio**: PC speaker + PCM pipeline.
- **Storage**: IDE + block device abstraction.
- **Bus**: PCI scan, ACPI RSDP parsing.

---

## 📦 Application Index

| App | Category | Description |
|-----|----------|-------------|
| Terminal | Productivity | Command-line interface, BEDIC REPL |
| Text Editor | Productivity | Large buffer, viewport scrolling, toolbar actions |
| File Explorer | Productivity | Navigate RAMFS / FAT32 |
| Hex Dump | Productivity | Hex viewer for binary files |
| Bitmap Maker | Productivity | Simple pixel art / BMP editor |
| Save / Load Dialogs | Productivity | File pickers |
| Calculator | Science | Arithmetic + expression evaluation |
| Graphing Calculator | Science | Plot functions, multi-expression |
| Mandelbrot | Science | Fractal renderer |
| Process Viewer | System | Live task list |
| Performance Monitor | System | FPS, CPU, memory stats |
| Kernel Log | System | Boot and runtime logs |
| PCI Scanner | System | List PCI devices |
| Personalization | System | Theme, wallpaper, accent color |
| HTTP Viewer | Network | Web page text renderer |
| Net Debug | Network | TCP/UDP/ICMP probes |
| Weather | Network | Weather via HTTP API |
| Piano | Media | Playable keyboard |
| Colour Wheel | Media | Color picker |
| Image Viewer | Media | BMP / image viewer |
| Calendar | Utilities | Date picker |
| Clock | Utilities | Analog + digital clock |
| On-Screen Keyboard | Utilities | Touch/mouse typing |
| Snake | Games | Classic snake |
| Tetris | Games | Falling-block puzzle |
| 2048 | Games | Sliding-tile puzzle |
| Minesweeper | Games | Mines grid |
| Pairs | Games | Memory matching |
| Sudoku | Games | Number puzzle |
| Flappy Bird | Games | Side-scroller |

---

## 🧑‍💻 Scripting (BEDIC)

**BEDIC** (`docs/bedic_tutorial.md`) is a compact, C-like scripting language that targets BEDI OS.

**Workflow**:
```bash
# Write script
echo 'print("Hello", endl)' > hello.bc

# Compile
bcc hello.bc
# -> hello.bin

# Run
brun hello.bin
```

**Supported types**: `int`, `double`, `string`, `bool`
**Flow control**: `if-else`, `while`, comments (`//`)

---

## 🤝 Contributing

Contributions are welcome. Before submitting:

1. Build locally with `./build.sh clean && ./build.sh build`.
2. Test in QEMU (`./build.sh run`) or real hardware.
3. Avoid modifying backup directories.
4. Do not commit binary artifacts (`bin/`, `*.iso`, `*.bin`, `src/rust/target`).

---

## ❓ FAQ

**Q: Why x86_64 only?**
A: Deep ISA-specific features (GDT/TSS, 4-level paging, Limine) are x86_64-core. Porting to RISC-V or ARM is a future possibility.

**Q: Can I use this as a daily driver?**
A: Not yet. The networking stack is functional but the TCP stack is still maturing. Storage is limited to FAT32.

**Q: Is the ISO required?**
A: No. ISO is generated by `build.sh` for testing. Source-only commits are fully accepted.

**Q: How is Rust used inside the kernel?**
A: `libbedi_rust.a` is linked into the monolithic kernel image. It provides a safe allocator, Tetris, and process bookkeeping.

---

## ⚖️ Legal

- **License**: MIT — see `LICENSE`
- **Bootloader**: Limine — BSD-style license
- **Logos**: Original — all rights reserved by the BEDI OS author
