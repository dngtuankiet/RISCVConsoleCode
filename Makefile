BOOTROM_DIR?=$(abspath .)

#Kiet custom
# WOLFSSL_DIR = /home/tuankiet/Documents/tools/wolfssl-build-riscv32
#WOLFSSL_DIR = /home/tuankiet/tools/wolfssl-build-rv32imac-elf
WOLFSSL_DIR = /home/tuankiet/tools/wolfssl-build-rv64gc-elf

LIB_WOLFSSL = $(WOLFSSL_DIR)/lib/libwolfssl.a
#LIB_HTIF = $(RISCV)/riscv64-unknown-elf/lib/libgloss_htif.a
# LIB_RISCV =  -L$(RISCV)/sysroot/lib -L$(RISCV)/sysroot/usr/lib -L$(RISCV)/lib -L$(RISCV)/riscv32-unknown-elf/lib
LIB_RISCV =  -L$(RISCV)/lib

# INCLUDE = -I$(WOLFSSL_DIR)/include -I$(RISCV)/riscv32-unknown-elf/include -I$(RISCV)/sysroot/usr/include
INCLUDE = -I$(WOLFSSL_DIR)/include -I$(RISCV)/include -I./include

ISACONF?=RV32IMAC
#CROSSCOMPILE?=riscv32-unknown-elf
CROSSCOMPILE?=riscv64-unknown-elf

CC=riscv64-unknown-elf-gcc
#CC_LINUX = riscv32-unknown-linux-gnu-gcc
CCX=$(CROSSCOMPILE)-g++
OBJCOPY=$(CROSSCOMPILE)-objcopy
OBJDUMP=$(CROSSCOMPILE)-objdump
ifeq ($(ISACONF),RV64GC)
CFLAGS_ARCH=-march=rv64imafdc -mabi=lp64d
else ifeq ($(ISACONF),RV64IMAC)
CFLAGS_ARCH=-march=rv64imac -mabi=lp64
else ifeq ($(ISACONF),RV32GC)
CFLAGS_ARCH=-march=rv32imafdc -mabi=ilp32d
else #RV32IMAC
CFLAGS_ARCH=-march=rv32i -mabi=ilp32
endif

# CFLAGS=$(CFLAGS_ARCH) -mcmodel=medany -O2 -std=gnu11 -Wall
# LFLAGS=-static --specs=nosys.specs -L $(BOOTROM_DIR)/linker -T memory.lds -T link.lds
# LFLAGS= -static -nostdlib -nostartfiles --specs=htif.specs -L $(BOOTROM_DIR)/linker -T memory.lds -T link.lds

#CFLAGS=$(CFLAGS_ARCH) -mcmodel=medany -O2 -std=gnu11 -Wall -nostartfiles -ffreestanding --specs=htif.specs
#CFLAGS+= -DWOLFSSL_USER_SETTINGS -DWOLFSSL_NO_MALLOC
#CFLAGS+= -fno-common -g -DENTROPY=0 -DNONSMP_HART=0
#CFLAGS+= -I $(BOOTROM_DIR)/include $(INCLUDE) -I. -I./src -I./kprintf -I./lib -I./clkutils -I./libfdt $(ADD_OPTS)
#LFLAGS= -static -nostdlib -nostartfiles -specs=nosys.specs -L $(BOOTROM_DIR)/linker -T memory.lds -T link.lds

CFLAGS=$(CFLAGS_ARCH) -mcmodel=medany -O2 -std=gnu11 -Wall -nostartfiles -ffreestanding --specs=htif.specs
CFLAGS+= -DWOLFSSL_USER_SETTINGS
CFLAGS+= -fno-common -g -DENTROPY=0 -DNONSMP_HART=0
CFLAGS+= -I $(BOOTROM_DIR)/include $(INCLUDE) -I. -I./src -I./kprintf -I./lib -I./clkutils -I./libfdt $(ADD_OPTS)
LFLAGS= -static -nostartfiles -specs=htif.specs -L $(BOOTROM_DIR)/linker -T memory.lds -T kiet.lds

BUILD_DIR?=$(abspath ./build)



LIB_FS_O= \
	uart/uart.o \
	trng/trng.o \
	$(BUILD_DIR)/version.o \
	clkutils/clkutils.o \
	kprintf/kprintf.o \
	plic/plic_driver.o \
	i2c/driver.o \
	lib/memcpy.o \
	lib/memset.o \
	lib/strcmp.o \
	lib/strncmp.o \
	lib/strlen.o \
	lib/memchr.o \
	lib/memmove.o \
	lib/memcmp.o \
	lib/strrchr.o \
	lib/strnlen.o \
	libfdt/fdt.o libfdt/fdt_ro.o libfdt/fdt_wip.o libfdt/fdt_sw.o libfdt/fdt_rw.o libfdt/fdt_strerror.o libfdt/fdt_empty_tree.o \
	libfdt/fdt_addresses.o libfdt/fdt_check.o

WOLF_O= \
    utils/wolf_utils.o

# SRC_O= \
# 	src/start.o \
# 	src/main.o

SRC_O= \
    src/start.o \
	src/main.o

$(BUILD_DIR)/version.c:
	mkdir -p $(BUILD_DIR)
	echo "const char *gitid = \"$(shell git describe --always --dirty)\";" > $(BUILD_DIR)/version.c
	echo "const char *gitdate = \"$(shell git log -n 1 --date=short --format=format:"%ad.%h" HEAD)\";" >> $(BUILD_DIR)/version.c
	echo "const char *gitversion = \"$(shell git rev-parse HEAD)\";" >> $(BUILD_DIR)/version.c

%.o: %.S
	$(CC) $(CFLAGS) -DFSBL_TARGET_ADDR=$(FSBL_TARGET_ADDR) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -DFSBL_TARGET_ADDR=$(FSBL_TARGET_ADDR) -c $< -o $@

%.o: %.cpp
	$(CCX) $(CFLAGS) -DFSBL_TARGET_ADDR=$(FSBL_TARGET_ADDR) -c $< -o $@

# elf := $(BUILD_DIR)/out.elf
# $(elf): $(LIB_FS_O) $(SRC_O) $(WOLF_O)
# 	mkdir -p $(BUILD_DIR)
# 	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $(SRC_O) $(LIB_FS_O) $(WOLF_O) -lgcc -lm -lgcc -lc  $(LIB_WOLFSSL)
# #	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $(LIB_FS_O) -lgcc -lm -lgcc -lc $(LIB_WOLFSSL)
#
# .PHONY: elf
# elf: $(elf)

# $(WOLF_O)
elfkiet := $(BUILD_DIR)/out.elf
$(elfkiet): $(LIB_FS_O) $(SRC_O)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS_ARCH) $(LFLAGS) -o $@  $(SRC_O) $(LIB_FS_O)  $(LIB_RISCV)  $(LIB_WOLFSSL) $(LIB_HTIF) -lm -lc -lgcc -lgloss

.PHONY: elfkiet
elfkiet: $(elfkiet)

kiet := $(BUILD_DIR)/out.bin
$(kiet): $(elfkiet)
	$(OBJCOPY) -O binary $< $@
	$(OBJDUMP) -d $^ > $@.dump

.PHONY: kiet
kiet: $(kiet)

# bin := $(BUILD_DIR)/out.bin
# $(bin): $(elf)
# 	$(OBJCOPY) -O binary $< $@
# 	$(OBJDUMP) -d $^ > $@.dump
#
# .PHONY: bin
# bin: $(bin)

hex := $(BUILD_DIR)/out.hex
$(hex): $(bin)
	od -t x4 -An -w4 -v $< > $@

.PHONY: hex
hex: $(hex)

.PHONY: clean
clean::
	rm -rf $(hex) $(elf) $(src_elf) src/main.o $(WOLF_O)

.PHONY: clean
clean_full::
	rm -rf $(hex) $(elf) $(src_elf) $(LIB_FS_O) $(SRC_O) build
