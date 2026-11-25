CROSS ?= x86_64-linux-gnu-
CC := $(CROSS)gcc
NASM ?= nasm
BUILD := build
KERNEL := $(BUILD)/kernel.elf
LIMINE_DIR ?= limine
ISO := CarsonOS-Hybrid-x86_64.iso

C_SOURCES := kernel/main.c kernel/mm/memory.c kernel/drivers/pci/pci.c kernel/drivers/net/e1000.c kernel/drivers/storage/ata.c kernel/fs/fat32.c
ASM_SOURCES := kernel/arch/x86_64/entry.asm kernel/arch/x86_64/cpu.asm kernel/arch/x86_64/interrupts.asm
C_OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SOURCES))
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -mcmodel=kernel
LDFLAGS := -nostdlib -static -no-pie -Wl,-T,kernel/linker.ld -Wl,--build-id=none -Wl,-z,max-page-size=0x1000

.PHONY: all clean iso carpkg-help carpkg-list carpkg-repo carpkg-validate carpkg-update
all: $(KERNEL)

$(KERNEL): $(OBJECTS) kernel/linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "Built $@"

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

iso: $(KERNEL)
	python3 tools/carpkg.py build-iso --kernel "$(KERNEL)" --limine-dir "$(LIMINE_DIR)" --output "$(ISO)"

carpkg-help:
	python3 tools/carpkg.py --help

carpkg-list:
	python3 tools/carpkg.py list

carpkg-repo:
	python3 tools/carpkg.py repo

carpkg-validate:
	python3 tools/carpkg.py validate

carpkg-update:
	python3 tools/carpkg.py update

clean:
	rm -rf $(BUILD)
