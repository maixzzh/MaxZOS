# Max_Z——一个基于C语言的玩具型操作系统

Max_Z 是一个从零开始构建的简易操作系统内核，运行于 x86 架构。它旨在通过实践探索操作系统底层原理，包括引导启动、内存管理、中断处理和 ACPI 电源管理等核心概念。目前尚处于**预览阶段（Preview）**，支持基本的交互式命令。

> 部分基础框架代码由 DeepSeek V4 Flash 辅助生成。

## 运行截图 This is Preview.📸 

![](./image/截图%202026-08-22%2011-21-28.png)

![](./image/截图%202026-08-22%2011-21-49.png)

## # ✨ 已实现功能

### 1. 命令系统
| 命令 | 语法 | 功能描述 |
| :--- | :--- | :--- |
| **echo** | `echo [字符串]` | 在屏幕上输出指定的文本。支持带引号（`"Hello"`）或不带引号（`Hello`）两种写法。 |
| **clear / cls** | `clear` 或 `cls` | 清空当前屏幕终端的全部内容，将光标重置至左上角。 |
|**exit**|`exit`|可以实现ACPI断电，QEMU进程以退出码0关闭|

### 2. 内核基础特性
- **Multiboot 协议兼容**：可通过 GRUB 引导加载程序启动。
- **ACPI 初步支持**：包含 `acpi.c/h` 模块，为后续电源管理和硬件枚举打下基础。
- **基础终端交互**：提供简易的命令行读取与解析循环。

## 项目结构

```bash
.
├── acpi.c
├── acpi.h
├── boot.asm
├── grub.cfg
├── linker.ld
├── main.c
├── Makefile
├── multiboot_header.asm
└── README.md

1 directory, 9 files
```

## 编译和使用指南

### 适用于Linux

**准备工具**

请确保你的电脑里安装有
- `qemu-system-x86`（一个模拟器软件）
- `gcc`（一个C语言的编译器）
- `nasm`（汇编器）
- `binutils`（二进制工具集）

对于arch系的用户，推荐再安装一个`binutils`。

关于制作可启动镜像的工具
- `grub`引导程序
- `xorriso` ISO景象制作工具
- `mtools` FAT文件系统工具集

**编译方式**

- 构建系统并创建ISO
```bash
make
```
- **运行系统**： `make run`或者`make run-iso`
- 清理构建文件
```
make clean
```