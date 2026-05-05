# Toolchain Paths
CROSS_COMPILE = /home/mu/Downloads/riscv_cross/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-
CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Flags
# -march=rv64gc_zicsr: Enables General, Compressed, and CSR extensions
# -mabi=lp64d: Standard 64-bit ABI
# -mcmodel=medany: Prevents relocation truncated to fit errors
CFLAGS  = -march=rv64gc_zicsr -mabi=lp64d -mcmodel=medany -ffreestanding -nostdlib -O2
LDFLAGS = -m elf64lriscv -T firmware/linker.ld

# Targets
FIRMWARE_ELF = firmware.elf
HOST_EXE     = uart

# Sources
FIRMWARE_OBJS = firmware/boot.o firmware/firmware.o
HOST_SRC      = host/uart.c

.PHONY: all clean run

all: $(FIRMWARE_ELF) $(HOST_EXE)

# Build RISC-V Firmware
$(FIRMWARE_ELF): $(FIRMWARE_OBJS)
	$(LD) $(LDFLAGS) $(FIRMWARE_OBJS) -o $(FIRMWARE_ELF)

firmware/%.o: firmware/%.S
	$(CC) $(CFLAGS) -c $< -o $@

firmware/%.o: firmware/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build Host Utility
$(HOST_EXE): $(HOST_SRC)
	gcc $(HOST_SRC) -o $(HOST_EXE)

# Cleanup build artifacts
clean:
	rm -f firmware/*.o $(FIRMWARE_ELF) $(HOST_EXE)

# Convenience target to run QEMU
run: all
	@echo "Starting QEMU... Press Ctrl+A then X to exit."
	qemu-system-riscv64 -machine virt -nographic -bios $(FIRMWARE_ELF) -serial pty
