# ==============================================================================
# Makefile - 32-bit x86 Hobby OS Kernel (Milestone 1)
# ==============================================================================

# Ensure compiler is in PATH for all shells
export PATH := $(HOME)/opt/i686-elf/bin:$(PATH)

CC      := i686-elf-gcc
AS      := nasm
CFLAGS  := -std=gnu11 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector -fno-pic -m32 -Iinclude
ASFLAGS := -f elf32
LDFLAGS := -T linker.ld -nostdlib -lgcc

BUILD_DIR := build
ISO_DIR   := $(BUILD_DIR)/isodir
KERNEL    := $(BUILD_DIR)/myos.bin
ISO       := $(BUILD_DIR)/myos.iso
DEPLOY_TO := /mnt/c/os/myos.iso

# Discover Assembly and C source files
ASM_SRCS := $(shell find src -type f -name '*.asm')
C_SRCS   := $(shell find src -type f -name '*.c')

# Generate corresponding object file paths in build/
ASM_OBJS := $(patsubst src/%.asm, $(BUILD_DIR)/%.o, $(ASM_SRCS))
C_OBJS   := $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(C_SRCS))
OBJS     := $(ASM_OBJS) $(C_OBJS)

.PHONY: all build iso deploy clean check-multiboot run

all: build

build: $(KERNEL) check-multiboot

# Compile NASM assembly files to 32-bit ELF objects
$(BUILD_DIR)/%.o: src/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Compile C source files to 32-bit ELF objects
$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Link all object files into the kernel ELF binary
$(KERNEL): $(OBJS) linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[LINK] Successfully created $(KERNEL)"

# Verify the kernel contains a valid Multiboot 1 header
check-multiboot: $(KERNEL)
	@if grub-file --is-x86-multiboot $(KERNEL); then \
		echo "[CHECK] Multiboot 1 header verified in $(KERNEL)"; \
	else \
		echo "[ERROR] $(KERNEL) is not Multiboot compliant!"; \
		exit 1; \
	fi

# Build bootable GRUB rescue ISO
iso: $(ISO)

$(ISO): $(KERNEL) check-multiboot iso/boot/grub/grub.cfg
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/myos.bin
	cp iso/boot/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR) 2>/dev/null
	@echo "[ISO] Bootable ISO generated at $(ISO)"

# Deploy the ISO to Windows filesystem for VirtualBox
deploy: $(ISO)
	@mkdir -p /mnt/c/os
	cp $(ISO) $(DEPLOY_TO)
	@echo "[DEPLOY] Successfully deployed ISO to $(DEPLOY_TO)"

# Build ISO and automatically handle poweroff, ISO copy, and VM launch via run.ps1
run: $(ISO)
	@mkdir -p /mnt/c/os
	cp scripts/run.ps1 /mnt/c/os/run.ps1
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\\os\\run.ps1" -SourceIso "$$(wslpath -w $(ISO))"

clean:
	rm -rf $(BUILD_DIR)
	@echo "[CLEAN] Build directory removed"
