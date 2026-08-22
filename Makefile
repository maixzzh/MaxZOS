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
# 头文件搜索路径：各模块目录（main.c 中的 #include "acpi.h" 等保持不变）
#   -MMD -MP      : 自动生成头文件依赖（bin/*.d），改头文件后 make 自动重编相关源文件
CFLAGS   = -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector -Wall -Wextra -Iacpi -Ifs -MMD -MP

# 汇编器标志（ELF 32 位目标）
NASMFLAGS = -f elf32

# 链接标志
#   -m elf_i386      : 链接为 32 位 ELF
#   -T kernel/linker.ld : 使用自定义链接脚本
LDFLAGS  = -m elf_i386 -T kernel/linker.ld

# 产物目录与目标文件
# 源文件自动搜集：新加的 .c/.asm 文件无需修改本文件，自动纳入构建
#   $(wildcard 目录/模式) : 列出匹配的源文件
#   $(notdir ...)         : 去掉目录前缀（.o 统一平铺在 bin/ 下）
BIN      = bin
C_SRCS   = $(wildcard *.c acpi/*.c fs/*.c kernel/*.c)
ASM_SRCS = $(wildcard kernel/*.asm)
OBJS     = $(addprefix $(BIN)/, $(notdir $(C_SRCS:.c=.o)) $(notdir $(ASM_SRCS:.asm=.o)))

# 头文件依赖文件（由 gcc -MMD 自动生成，见下方 -include）
DEPS     = $(OBJS:.o=.d)

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
$(KERNEL): $(OBJS) kernel/linker.ld | $(BIN)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# 编译规则（模式规则：% 是通配部分）
# 例如 bin/acpi.o 会尝试匹配以下规则，找到源文件存在的那一条：
#   %.o: %.c        → 根目录的 acpi.c
#   %.o: acpi/%.c   → acpi/ 目录的 acpi.c
#   %.o: fs/%.c     → fs/ 目录的 acpi.c
# 注意：不同目录的源文件不能同名（如根目录和 fs/ 不能都有 foo.c）

# 编译 C 源文件
$(BIN)/%.o: %.c | $(BIN)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN)/%.o: acpi/%.c | $(BIN)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN)/%.o: fs/%.c | $(BIN)
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译汇编源文件（kernel/ 下的 .asm）
$(BIN)/%.o: kernel/%.asm | $(BIN)
	$(NASM) $(NASMFLAGS) -o $@ $<

# 创建产物目录（order-only 依赖：目录存在即可，时间戳变化不触发重编译）
$(BIN):
	$(MKDIR) $(BIN)

# 加载 gcc 自动生成的头文件依赖（.d 文件）；首次构建时不存在，静默跳过
-include $(DEPS)

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
