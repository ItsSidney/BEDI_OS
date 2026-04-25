@echo off
setlocal enabledelayedexpansion

:: Directories
set BIN_DIR=bin
set SRC_DIR=src
set ISO_ROOT=iso_root
set ISO_NAME=BEDI-x86_64.iso
set LIMINE_BIN=tools\limine-bin

if not exist %BIN_DIR% mkdir %BIN_DIR%
if not exist %ISO_ROOT%\boot\limine mkdir %ISO_ROOT%\boot\limine
if not exist %ISO_ROOT%\EFI\BOOT mkdir %ISO_ROOT%\EFI\BOOT

echo [1/4] Assembling (ELF64)...
wsl nasm -f elf64 %SRC_DIR%/kernel/tss_load.asm -o %BIN_DIR%/tss_load.o
wsl nasm -f elf64 %SRC_DIR%/kernel/fpu.asm -o %BIN_DIR%/fpu.o
wsl nasm -f elf64 %SRC_DIR%/kernel/interrupts.asm -o %BIN_DIR%/interrupts.o
if %errorlevel% neq 0 ( echo Error assembling. & pause & exit /b %errorlevel% )

echo [2/4] Compiling (ELF64)...
set GCC_FLAGS=-m64 -mcmodel=kernel -ffreestanding -fno-pie -fno-stack-protector -mno-red-zone -Iinclude -Isrc/filesystem -c
wsl gcc %GCC_FLAGS% %SRC_DIR%/kernel/kernel.c -o %BIN_DIR%/kernel.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/kernel/idt.c -o %BIN_DIR%/idt.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/kernel/tss.c -o %BIN_DIR%/tss.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/filesystem/filesystem.c -o %BIN_DIR%/filesystem.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/commands/commands.c -o %BIN_DIR%/commands.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/drivers/framebuffer.c -o %BIN_DIR%/framebuffer.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/drivers/keyboard.c -o %BIN_DIR%/keyboard.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/drivers/rtc.c -o %BIN_DIR%/rtc.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/drivers/pcspeaker.c -o %BIN_DIR%/pcspeaker.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/games/guessing_game.c -o %BIN_DIR%/guessing_game.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/apps/calculator.c -o %BIN_DIR%/calculator.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/apps/editor.c -o %BIN_DIR%/editor.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/apps/stories.c -o %BIN_DIR%/stories.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/apps/doc_viewer.c -o %BIN_DIR%/doc_viewer.o
wsl gcc %GCC_FLAGS% %SRC_DIR%/gui/gui.c -o %BIN_DIR%/gui.o
if %errorlevel% neq 0 ( echo Error compiling. & pause & exit /b %errorlevel% )

echo [3/4] Linking (ELF64)...
wsl ld -m elf_x86_64 -nostdlib -static -z noexecstack -T linker.ld -o %BIN_DIR%/bedi_os.bin %BIN_DIR%/kernel.o %BIN_DIR%/idt.o %BIN_DIR%/tss.o %BIN_DIR%/tss_load.o %BIN_DIR%/fpu.o %BIN_DIR%/interrupts.o %BIN_DIR%/filesystem.o %BIN_DIR%/commands.o %BIN_DIR%/framebuffer.o %BIN_DIR%/keyboard.o %BIN_DIR%/rtc.o %BIN_DIR%/pcspeaker.o %BIN_DIR%/guessing_game.o %BIN_DIR%/calculator.o %BIN_DIR%/editor.o %BIN_DIR%/stories.o %BIN_DIR%/doc_viewer.o %BIN_DIR%/gui.o
if %errorlevel% neq 0 ( echo Error linking. & pause & exit /b %errorlevel% )

echo [4/4] Preparing Limine ISO Structure...
copy /Y %BIN_DIR%\bedi_os.bin %ISO_ROOT%\boot\ > nul
copy /Y limine.conf %ISO_ROOT%\boot\limine\ > nul

copy /Y %LIMINE_BIN%\limine-bios.sys %ISO_ROOT%\boot\limine\ > nul
copy /Y %LIMINE_BIN%\limine-bios-cd.bin %ISO_ROOT%\boot\limine\ > nul
copy /Y %LIMINE_BIN%\limine-uefi-cd.bin %ISO_ROOT%\boot\limine\ > nul
copy /Y %LIMINE_BIN%\BOOTX64.EFI %ISO_ROOT%\EFI\BOOT\ > nul
copy /Y %LIMINE_BIN%\BOOTIA32.EFI %ISO_ROOT%\EFI\BOOT\ > nul

echo Creating Hybrid ISO using xorriso...
if exist %ISO_NAME% del /f /q %ISO_NAME%
wsl xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label iso_root -o %ISO_NAME%
if %errorlevel% neq 0 ( echo Error creating ISO. & pause & exit /b %errorlevel% )

echo Deploying Limine bootloader to ISO...
%LIMINE_BIN%\limine.exe bios-install %ISO_NAME%

echo.
echo Build Successful! BEDI OS 5.0.0 Pro Hybrid UEFI/BIOS ISO: %ISO_NAME%
echo.
set /p run=Launch in QEMU? (y/n): 
if /i "%run%"=="y" (
    qemu-system-x86_64 -cdrom %ISO_NAME% -m 512M -cpu max -audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0
)
pause
