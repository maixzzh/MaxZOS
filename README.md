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
| **create** | `create <文件名> [内容]` | 创建文件（最多 32 个文件，文件名 ≤31 字符，内容 ≤256 字节）。内容含空格时用双引号括起，如 `create notes "hello world"`；空文件用 `create notes ""`。 |
| **cat** | `cat <文件名>` | 读取并显示文件内容。 |
| **delete** | `delete <文件名>` | 删除文件。 |
| **ls** | `ls` | 列出所有文件及内容大小。 |
|**exit**|`exit`|可以实现ACPI断电，QEMU进程以退出码0关闭|

> 注：文件系统为纯内存实现（静态数组），重启后数据丢失；命令前缀必须完整（`clearxxx` 不再误命中 `clear`）。

### 2. 内核基础特性
- **Multiboot 协议兼容**：可通过 GRUB 引导加载程序启动。
- **ACPI 初步支持**：包含 `acpi.c/h` 模块，为后续电源管理和硬件枚举打下基础。
- **基础终端交互**：提供简易的命令行读取与解析循环。
- **内存文件系统**：包含 `fs.c/h` 模块（单级目录、静态数组存储），提供 create/read/delete/list 操作接口。

## 项目结构

```bash
.
├── acpi/               # ACPI 电源管理模块
│   ├── acpi.c
│   └── acpi.h
├── fs/                 # 文件系统模块（含字符串工具）
│   ├── fs.c            # 内存文件系统实现
│   ├── fs.h
│   ├── str.c           # 字符串工具（strlen/strcmp/strcpy/itoa_dec）
│   └── str.h
├── image/              # 截图资源
├── kernel/             # 内核核心（引导与链接）
│   ├── boot.asm        # 引导入口与栈设置
│   ├── linker.ld       # 链接脚本（基址 1MB）
│   └── multiboot_header.asm
├── main.c              # 内核入口、终端输出、键盘与命令解析
├── grub.cfg            # GRUB 启动配置（打包进 ISO）
├── Makefile            # 构建脚本（产物统一输出到 bin/）
└── README.md

（构建产物统一生成在 bin/ 目录下，含 kernel.elf 与 myos.iso）
```

> 模块划分：文件系统相关改动只需动 `main.c` 与 `fs/` 目录；ACPI 相关改动只需动 `main.c` 与 `acpi/` 目录。

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