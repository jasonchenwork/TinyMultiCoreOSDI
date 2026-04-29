TOOLCHAIN_PREFIX = aarch64-linux-gnu-
CC = $(TOOLCHAIN_PREFIX)gcc
LD = $(TOOLCHAIN_PREFIX)ld
OBJCPY = $(TOOLCHAIN_PREFIX)objcopy

BUILD_DIR = build
SRC_DIR = src
INC_DIR = include

LINKER_FILE = $(SRC_DIR)/link.lds

# 1. 取得所有原始碼 (小寫 .s)
SRCS_C = $(wildcard $(SRC_DIR)/*.c)
SRCS_ASM = $(wildcard $(SRC_DIR)/*.s)

# 2. 轉換物件檔路徑
OBJS_C = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/c/%.o, $(SRCS_C))
OBJS_ASM = $(patsubst $(SRC_DIR)/%.s, $(BUILD_DIR)/asm/%.o, $(SRCS_ASM))
ALL_OBJS = $(OBJS_C) $(OBJS_ASM)


# 編譯參數
CFLAGS = -std=c99 -ffreestanding -mgeneral-regs-only -I$(INC_DIR) -c

.PHONY: all clean run

all: kernel8.img

# 編譯 C
$(BUILD_DIR)/c/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

# 編譯 ASM
$(BUILD_DIR)/asm/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -o $@

# 連結核心
kernel8.img: $(ALL_OBJS)
	$(LD) $(ALL_OBJS) -nostdlib -T $(LINKER_FILE) -o $(BUILD_DIR)/kernel
	$(OBJCPY) -O binary $(BUILD_DIR)/kernel kernel8.img
	@echo "Build successful: kernel8.img"
	
	# 附加資料
	@if [ -f data/os.img ]; then \
		dd if=data/os.img >> kernel8.img; \
		echo "Add data/os.img successful"; \
	fi

clean:
	rm -rf $(BUILD_DIR) kernel8.img

    
run: $(BUILD_DIR) kernel8.img
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -serial stdio