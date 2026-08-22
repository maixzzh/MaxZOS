# ============================================================
# Makefile - 编译、链接并生成可启动的 ISO 镜像
# 环境：Arch Linux (x86_64) 目标：x86 (i386) 引导：GRUB + Multiboot
# 所有构建产物统一输出到 bin/ 目录
# ============================================================

# 编译器与工具
CC       = gcc
NASM     = nasm
LD       = ld
GRUB_MK  = grub-mkrescue
RM       = rm -f
MKDIR    = mkdir -p

# 编译标志
#   -m32          : 生成 32 位代码（目标架构 x86）
#   -ffreestanding: 不依赖宿主环境的标准库（裸机）
#   -nostdlib     : 不链接标准库
#   -fno-builtin  : 禁用内置函数（防止编译器替换为库调用）
#   -fno-stack-protector: 关闭栈保护（无 libc 支持）
#   -Wall -Wextra : 开启警告，便于发现问题
CFLAGS   = -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -Wall -Wextra

# 汇编器标志（ELF 32 位目标）
NASMFLAGS = -f elf32

# 链接标志
#   -m elf_i386   : 链接为 32 位 ELF
#   -T linker.ld  : 使用自定义链接脚本
LDFLAGS  = -m elf_i386 -T linker.ld

# 产物目录与目标文件
BIN      = bin
OBJS     = $(addprefix $(BIN)/, multiboot_header.o boot.o main.o acpi.o fs.o str.o)

# 最终产物
KERNEL = $(BIN)/kernel.elf
ISO    = $(BIN)/myos.iso

# 默认目标
all: $(ISO)

# 生成 ISO：在 bin/ 内创建临时目录结构，复制内核和 grub.cfg，用 grub-mkrescue 打包
$(ISO): $(KERNEL) grub.cfg | $(BIN)
	$(MKDIR) $(BIN)/iso/boot/grub
	cp $(KERNEL) $(BIN)/iso/boot/
	cp grub.cfg $(BIN)/iso/boot/grub/
	$(GRUB_MK) -o $@ $(BIN)/iso/
	rm -rf $(BIN)/iso/          # 清理临时目录

# 链接内核
$(KERNEL): $(OBJS) linker.ld | $(BIN)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# 编译汇编文件（multiboot_header.asm / boot.asm）
$(BIN)/multiboot_header.o: multiboot_header.asm | $(BIN)
	$(NASM) $(NASMFLAGS) -o $@ $<

$(BIN)/boot.o: boot.asm | $(BIN)
	$(NASM) $(NASMFLAGS) -o $@ $<

# 编译 C 源文件（main.o 补上头文件依赖，修复现有依赖缺失）
$(BIN)/main.o: main.c acpi.h fs.h str.h | $(BIN)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN)/acpi.o: acpi.c acpi.h | $(BIN)
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译文件系统与字符串工具
$(BIN)/fs.o: fs.c fs.h str.h | $(BIN)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN)/str.o: str.c str.h | $(BIN)
	$(CC) $(CFLAGS) -c -o $@ $<

# 创建产物目录（order-only 依赖：目录存在即可，时间戳变化不触发重编译）
$(BIN):
	$(MKDIR) $(BIN)

# 清理生成的文件（整目录删除）
clean:
	$(RM) -rf $(BIN)

# 运行 QEMU（快速测试，无需 ISO）
run: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL)

# 运行 ISO（模拟从光盘引导）
run-iso: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO)

# 声明伪目标
.PHONY: all clean run run-iso
