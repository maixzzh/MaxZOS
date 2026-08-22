# ============================================================
# Makefile - 编译、链接并生成可启动的 ISO 镜像
# 环境：Arch Linux (x86_64) 目标：x86 (i386) 引导：GRUB + Multiboot
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

# 目标文件
OBJS = multiboot_header.o boot.o main.o acpi.o

# 最终产物
KERNEL = kernel.elf
ISO    = myos.iso

# 默认目标
all: $(ISO)

# 生成 ISO：创建临时目录结构，复制内核和 grub.cfg，用 grub-mkrescue 打包
$(ISO): $(KERNEL) grub.cfg
	$(MKDIR) iso/boot/grub
	cp $(KERNEL) iso/boot/
	cp grub.cfg iso/boot/grub/
	$(GRUB_MK) -o $(ISO) iso/
	rm -rf iso/          # 清理临时目录

# 链接内核
$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# 编译汇编文件（multiboot_header.asm）
multiboot_header.o: multiboot_header.asm
	$(NASM) $(NASMFLAGS) -o $@ $<

# 编译汇编文件（boot.asm）
boot.o: boot.asm
	$(NASM) $(NASMFLAGS) -o $@ $<

# 编译 C 源文件（main.c）
main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译 C 源文件（acpi.c）
acpi.o: acpi.c acpi.h
	$(CC) $(CFLAGS) -c -o $@ $<

# 清理生成的文件
clean:
	$(RM) $(OBJS) $(KERNEL) $(ISO)
	$(RM) -rf iso/

# 运行 QEMU（快速测试，无需 ISO）
run: $(KERNEL)
	qemu-system-x86_64 -kernel $(KERNEL)

# 运行 ISO（模拟从光盘引导）
run-iso: $(ISO)
	qemu-system-x86_64 -cdrom $(ISO)

# 声明伪目标
.PHONY: all clean run run-iso