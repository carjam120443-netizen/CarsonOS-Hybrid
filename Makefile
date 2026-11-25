CROSS ?= x86_64-linux-gnu-
CC := $(CROSS)gcc
NASM ?= nasm
BUILD := build
KERNEL := $(BUILD)/kernel.elf

C_SOURCES := kernel/main.c
ASM_SOURCES := kernel/arch/x86_64/entry.asm kernel/arch/x86_64/cpu.asm kernel/arch/x86_64/interrupts.asm
C_OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SOURCES))
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror \
          -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
          -m64 -mno-red-zone -mgeneral-regs-only -mcmodel=kernel
LDFLAGS := -nostdlib -static -no-pie -Wl,-T,kernel/linker.ld \
           -Wl,--build-id=none -Wl,-z,max-page-size=0x1000

.PHONY: all clean
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

clean:
	rm -rf $(BUILD)
