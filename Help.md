# MaxZOS 维护手册

>**本手册写给谁**：一个只掌握 C 语言基础语法、从未接触过汇编和操作系统开发的人。
>
> **本手册要做什么**：告诉你如何安全、正确地维护这个项目——修改系统文字、添加新命令、扩展文件系统、排查问题、使用 git 管理代码。**全程不需要你写一行汇编代码**。
>
> **本手册不做什么**：不教你 C 语言本身，也不深入讲解汇编和链接脚本（这部分内容只要求你"知道它们存在、不要乱动"）。

---

# 第一部分　入门

## 1. 这个项目是什么

MaxZOS 是一个**运行在模拟器里的操作系统内核**，用 C 语言编写（另有少量汇编，但你不必理解它们）。

它和普通 C 程序有本质区别，先用一句话理解：

> 普通程序运行在操作系统之上，调用操作系统提供的函数（`printf`、`malloc`……）；而 MaxZOS **自己就是操作系统**，它运行在模拟的裸机硬件上，不能调用任何现成的函数——所有功能都要自己实现。

目前 MaxZOS 具备的能力：

| 能力 | 说明 |
|---|---|
| 开机引导 | 通过 GRUB 引导程序启动（这部分由汇编完成，你不用管） |
| 屏幕输出 | 直接往显卡内存写字符，显示文本 |
| 键盘输入 | 轮询键盘端口，接收用户按键 |
| 命令行 | 一个简易 shell，支持 `echo`、`clear`、`create`、`cat`、`delete`、`ls`、`exit` |
| 内存文件系统 | 可以在内存里创建、读取、删除、列出文本文件（重启后数据丢失） |
| ACPI 关机 | 执行 `exit` 命令时真正断电关机 |

MaxZOS 运行在 **QEMU 模拟器**里（一个开源的虚拟机软件），不是真实电脑——这对维护者是好消息：**你怎么折腾它都不会弄坏真电脑，最多重启一下模拟器**。

## 2. 你需要会什么、不需要会什么

**需要会（复习一下即可）：**

- C 语言基础语法：变量、函数、`if/else`、`while` 循环、`switch`、结构体、枚举、指针（重点）、字符串（`char*`、`'\0'`）
- 基本的终端操作：`cd`、`ls`、`cat`、`mkdir` 等
- 最基本的 `make` 命令用法（本手册会教）

**不需要会：**

- 汇编语言（x86 指令等）——**完全不需要**
- 链接脚本语法（`linker.ld`）——**不需要**
- 操作系统原理（中断、分页、特权级等）——**不需要**，本手册会在必要时用比喻解释
- 硬件知识（端口、寄存器）——**不需要**，你只需要会调用项目里现成的函数

**一句话原则：**

> 你需要接触和修改的文件只有三个位置：根目录的 `main.c`、`fs/` 目录、`acpi/` 目录。其他文件（`kernel/` 下的汇编和链接脚本、`grub.cfg`、`Makefile` 的结构部分）**看看即可，不要动**。

## 3. 快速上手（第一次运行这个项目）

假设你已经在 Arch Linux 上拿到了项目代码（一个叫 `MaxZOS` 的文件夹），按以下步骤操作：

### 3.1 安装依赖（只需一次）

打开终端，执行：

```bash
sudo pacman -S --needed gcc nasm binutils make qemu-system-x86 grub xorriso mtools
```

- 已经装过的会自动跳过（`--needed` 参数的作用）
- 如果提示找不到某个包，执行 `sudo pacman -Syu` 先更新系统再装

### 3.2 进入项目目录

```bash
cd MaxZOS
```

以后所有操作都在这个目录下进行。

### 3.3 编译

```bash
make
```

第一次编译会输出很多行（编译器在编译各个源文件），最后应该以类似下面的内容结束，**没有任何红色错误**：

```
grub-mkrescue -o bin/myos.iso bin/iso/
xorriso : UPDATE : ...  Writing to 'stdio:bin/myos.iso' completed successfully.
```

编译成功后，产物在 `bin/` 目录里：

| 产物 | 是什么 |
|---|---|
| `bin/kernel.elf` | 编译出的内核程序（ELF 格式，Linux 的 `file` 命令能认出来） |
| `bin/myos.iso` | 打包成光盘镜像的内核，可以像光盘一样启动 |
| `bin/*.o` | 中间目标文件（每个源文件编译一次得到的"半成品"），可以无视 |

### 3.4 运行

```bash
make run
```

会弹出 QEMU 窗口，几秒后屏幕显示：

```
Welcome to MaxZOS v0.9
made by ZhangMaixuan
os/>
```

光标在 `os/> ` 后面闪烁，说明系统运行正常。随便输入几个命令试试：

```
os/> echo hello
hello
os/> create note "buy milk"
os/> ls
note  8
os/> cat note
buy milk
os/> exit
```

输入 `exit` 后系统会真关机，QEMU 窗口自动关闭，终端回到正常状态。

### 3.5 关闭 QEMU 的备用方法

如果系统卡死（比如你改了代码引入 bug），QEMU 窗口可以直接关掉：

- 点击窗口标题栏的 × 关闭
- 或者在运行 `make run` 的终端里按 `Ctrl + C`

这不会损坏任何东西——系统是内存里的，一关就全没了。

### 3.6 清理构建产物

```bash
make clean
```

删除 `bin/` 目录，下次 `make` 会全部重新编译。平时不用经常清理，`make` 自己会判断哪些文件变了、只重新编译变了的文件（这叫**增量编译**，后面第 7 节会讲）。

## 4. 工作流程（你以后维护时的工作循环）

维护这个项目，你的工作流程永远是这四步，循环往复：

```
第 1 步：修改代码（用编辑器改 main.c / fs/ 里的文件）
   ↓
第 2 步：编译（make）
   ↓
第 3 步：运行测试（make run，在 QEMU 里操作验证）
   ↓
第 4 步：满意？→ 用 git 提交存档；不满意 → 回到第 1 步
```

**重要习惯**：

1. **一次只改一件事**。改完编译、运行、确认没问题，再改下一件事。如果一次改了很多地方然后出错了，你很难判断是哪里出的问题。
2. **每次动手改代码之前**，先把当前状态提交到 git（第 7 部分会讲）。这样改坏了可以随时回到"能跑的状态"。
3. **编译报错不等于系统坏了**。编译错误就是你的代码写错了，照着错误提示修即可（第 23 节有详细的错误对照表）。

## 5. 环境与工具链详解

这一节把编译运行过程中涉及的工具逐一介绍。你不必背下来，先通读一遍，**遇到问题时回来查**。

### 5.1 工具总览

| 工具 | 角色 | 用 C 程序员的语言说 |
|---|---|---|
| `gcc` | C 编译器 | 把 `.c` 文件编译成 `.o` 目标文件。**和你在 Linux 上用 gcc 编译普通 C 程序是一样的**，只是加了几个特殊参数（见 5.2） |
| `nasm` | 汇编编译器 | 把 `.asm` 汇编文件编译成 `.o`。**你不需要会写汇编，只需要知道它负责编译 `kernel/` 下那三个汇编文件** |
| `ld` | 链接器 | 把多个 `.o` 拼成一个完整的内核程序（`kernel.elf`）。类比：gcc 编译出"零件"，ld 把它们"组装"成整机 |
| `grub-mkrescue` | 打包工具 | 把内核和引导配置打包成一个可启动的光盘镜像（`.iso`）。类比：把"整机"装进"包装箱" |
| `qemu-system-x86_64` | 模拟器 | 模拟一台 x86 电脑来运行我们的内核。你以后测试就是靠它 |
| `xorriso` / `mtools` | 辅助工具 | `grub-mkrescue` 的底层依赖，负责往镜像里写数据。你永远不会直接用到它们 |
| `make` | 构建调度器 | 读 `Makefile` 里的规则，自动决定"先编译谁、后编译谁、谁需要重新编译"。你执行 `make` 就是在用它 |

### 5.2 gcc 的特殊编译参数（为什么要加这些）

在普通 Linux 程序里，`gcc hello.c` 就能编译。但内核的编译多了一串参数，每个都有明确原因：

| 参数 | 含义 | 为什么需要 |
|---|---|---|
| `-m32` | 生成 32 位代码 | 我们的内核是 32 位 x86 架构（目标机器是 32 位 CPU） |
| `-ffreestanding` | 告诉编译器"这是独立程序" | 告诉编译器我们不会用操作系统的库函数，编译器不得偷偷插入对库函数的调用 |
| `-nostdlib` | 不链接标准库 | 内核没有 `printf`、`malloc`、`memcpy` 这些现成函数——它们来自 libc，而 libc 依赖操作系统，内核不能用。**这也是为什么项目里要自己写 `str.c`（字符串工具）** |
| `-fno-builtin` | 禁止编译器把函数调用优化成内置函数 | 防止编译器把你写的 `strlen` 之类的函数替换成对 libc 版本的调用 |
| `-fno-stack-protector` | 关闭栈保护 | 栈保护（canary）依赖 libc 的 `__stack_chk_fail`，内核里没有它，必须关掉 |
| `-Wall -Wextra` | 开启所有常见警告 | 帮你在编译阶段发现潜在问题（比如未使用的变量、类型不匹配） |
| `-Iacpi -Ifs` | 添加头文件搜索路径 | 让 `main.c` 里 `#include "acpi.h"` 能找到 `acpi/acpi.h`，`#include "fs.h"` 能找到 `fs/fs.h`。**这是目录整理后的关键机制：源码里的 include 不用写路径，编译器自动去这两个目录找** |

**这些参数在 `Makefile` 里定义，你不需要也不应该修改它们。**

### 5.3 make 的最小知识

`Makefile` 是 make 的"说明书"，描述了三类信息：**产物**（target）、**原材料**（prerequisites）、**制作方法**（recipe）。

用大白话翻译我们项目 Makefile 里的核心规则：

```
bin/myos.iso 需要 bin/kernel.elf 和 grub.cfg 才能做出来
  做它的方法是：把 kernel.elf 和 grub.cfg 复制到临时目录，用 grub-mkrescue 打包成 iso

bin/kernel.elf 需要 6 个 .o 文件 和 kernel/linker.ld 才能做出来
  做它的方法是：用 ld 把 6 个 .o 链接到一起

每个 .o 文件由对应的 .c/.asm 编译而来
  multiboot_header.o   ← kernel/multiboot_header.asm（nasm 编译）
  boot.o               ← kernel/boot.asm（nasm 编译）
  main.o               ← main.c（gcc 编译）
  acpi.o               ← acpi/acpi.c（gcc 编译）
  fs.o                 ← fs/fs.c（gcc 编译）
  str.o                ← fs/str.c（gcc 编译）
```

**增量编译的原理**：make 比较"原材料"和"产物"的时间戳——如果某个 `.c` 文件比它对应的 `.o` 文件新（说明你刚改过它），就重新编译这个文件；没变的文件不编译。所以**你只改了 `main.c` 时，`make` 只重新编译 `main.c`，几秒钟就完事**。

**你真正需要记住的 make 命令只有这几个：**

| 命令 | 作用 | 何时用 |
|---|---|---|
| `make` | 编译全部（含打包 ISO） | 每次改完代码后 |
| `make run` | 编译内核（不打包 ISO，更快）并启动 QEMU | 日常快速测试（推荐） |
| `make run-iso` | 编译 ISO 并模拟从光盘启动 | 想验证完整的"光盘引导"流程时 |
| `make clean` | 删除 `bin/` 全部产物 | 想彻底重来一遍时（比如怀疑产物损坏） |

**什么时候用 `make run`、什么时候用 `make`？**

- 日常改代码测试：`make run`（快，不打包 ISO）
- 想确认"交付物"完整：`make`（打包出 `bin/myos.iso`）
- 怀疑编译产物有问题：先 `make clean` 再 `make run`

---

# 第二部分　项目结构

## 6. 目录结构速览

```
MaxZOS/
├── main.c              ← ★ 你最常改的文件：内核入口 + 终端输出 + 键盘 + 命令解析
├── acpi/               ← ACPI 电源管理模块（通常不用改）
│   ├── acpi.c
│   └── acpi.h
├── fs/                 ← ★ 文件系统模块（扩展文件系统时改这里）
│   ├── fs.c            ← 文件系统的具体实现
│   ├── fs.h            ← 文件系统的数据结构与接口声明
│   ├── str.c           ← 字符串工具函数实现（strlen/strcmp/strcpy/itoa_dec）
│   └── str.h           ← 字符串工具声明
├── kernel/             ← 引导与链接（汇编 + 链接脚本，不要动）
│   ├── boot.asm        ← 启动代码：设置栈、调用 kmain
│   ├── multiboot_header.asm ← 引导头：告诉 GRUB 怎么加载内核
│   └── linker.ld       ← 链接脚本：规定内核在内存里的摆放布局
├── image/              ← README 用的截图
├── grub.cfg            ← GRUB 引导配置（打包进 ISO，一般不用动）
├── Makefile            ← 构建脚本（已自动扫描源文件，日常几乎不用动，详见第 12 节）
├── README.md           ← 项目简介
├── Help.md             ← 本手册
└── bin/                ← 构建产物（自动生成，make clean 可删除，不需要手动管）
```

**日常维护中你 90% 的改动发生在两个地方**：

1. `main.c` —— 改命令、改文字、改交互逻辑
2. `fs/` 目录 —— 改文件系统行为（限制、接口、存储方式）

### 6.1 各文件"可动性"一览

| 文件 | 能不能动 | 原因 |
|---|---|---|
| `main.c` | ✅ 随便动 | 项目的主体逻辑，就是让你维护的 |
| `fs/fs.c`、`fs/fs.h` | ✅ 随便动 | 文件系统，未来扩展的主战场 |
| `fs/str.c`、`fs/str.h` | ✅ 可以动 | 字符串工具，可以加新函数（但不要改已有函数的行为） |
| `acpi/acpi.c`、`acpi/acpi.h` | ⚠️ 尽量别动 | 它只是给 `exit` 命令提供关机能力。如果只是想用关机功能，**不需要动它** |
| `kernel/boot.asm` | ❌ 不要动 | 汇编代码，改错一行系统直接无法启动。它的职责已经完成，不需要修改 |
| `kernel/multiboot_header.asm` | ❌ 不要动 | 同上 |
| `kernel/linker.ld` | ❌ 不要动 | 链接脚本，规定了内存布局。目前布局完全够用 |
| `grub.cfg` | ❌ 一般不动 | 引导菜单配置。只有当你要往镜像里加"额外的文件"（第 35 节扩展路线）时才需要改 |
| `Makefile` | ⚠️ 有限地动 | 已升级为**自动扫描**（第 12 节）：新加 `.c`/`.asm` 文件不用动它。**只有"新增了存放源文件的目录"或"修改编译参数"时才需要改**（第 12.4 节） |
| `bin/` | ❌ 不要手动改 | 全部自动生成，`make clean` 一键删除 |

## 7. 系统启动的完整流程（概念性理解）

理解"系统是怎么跑起来的"，对你日后调试很有帮助。用讲故事的方式讲一遍：

```
┌─────────────────────────────────────────────────────────┐
│ 第 0 步：你执行 make run                                 │
│   → make 编译出 bin/kernel.elf                           │
│   → 启动 qemu-system-x86_64，把 kernel.elf 交给它        │
└─────────────────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────┐
│ 第 1 步：QEMU 模拟的"电脑"开机                           │
│   CPU 开始执行代码。QEMU 用 -kernel 方式引导时，          │
│   内核文件被加载到内存 1MB 处                            │
└─────────────────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────┐
│ 第 2 步：CPU 执行 multiboot_header.asm 定义的头          │
│   （这段汇编告诉"引导器"：我是多启动协议兼容的内核）       │
│   —— 你不需要看懂它，只需要知道它存在                    │
└─────────────────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────┐
│ 第 3 步：CPU 执行 boot.asm 里的 _start 代码              │
│   做的事情只有两件（概念上）：                           │
│   ① 把栈指针设置好（内核的栈在内存里，16KB）             │
│   ② 调用 C 函数 kmain(magic, addr)                      │
│   —— 从此进入 C 语言的世界！                             │
└─────────────────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────┐
│ 第 4 步：执行 main.c 里的 kmain()                       │
│   ① 调用 fs_init() 初始化文件系统                       │
│   ② terminal_clear() 清屏                               │
│   ③ 打印欢迎语和提示符 "os/> "                          │
│   ④ 进入死循环 while(1) { handle_keyboard(); }          │
│   —— 这个死循环就是"操作系统在运行"的本质：              │
│      不停地看键盘有没有新按键                            │
└─────────────────────────────────────────────────────────┘
                     ↓
┌─────────────────────────────────────────────────────────┐
│ 第 5 步：你按下一个键                                    │
│   handle_keyboard() 轮询键盘端口 → 读到扫描码 →          │
│   查表变成字符 → 回显到屏幕 → 存进 input_buf            │
│   按回车 → process_command() 解析执行命令                │
└─────────────────────────────────────────────────────────┘
```

**理解要点：**

1. **汇编只负责"起跑"**：从开机到进入 C 函数，只有那两小段汇编。一旦 `kmain` 被调用，剩下的所有事情都是 C 代码完成的。
2. **死循环是核心**：`while (1)` 不是 bug，操作系统就是这样"永远活着"的。你的所有交互逻辑都在这个循环里。
3. **没有"多任务"、没有"中断"**：当前内核一次只做一件事，CPU 一直在等键盘。这是它简单的原因，也是它稳定的原因——**没有任何并发问题需要考虑**。

# 第三部分　代码逐模块详解

> 这一部分把每个源文件"掰开揉碎"讲清楚。读完这部分，你应该能做到：看到任意一个函数，知道它是干什么的、被谁调用、能不能改。
>
> **阅读提示**：所有函数名都是真的，你可以在对应文件里搜索到（`Ctrl+F`）。文中提到的行号仅供参考，**改过代码后行号会变，认函数名不要认行号**。

## 8. main.c —— 内核的主体（你最常改的文件）

`main.c` 在项目根目录，是整个系统的心脏。它按功能分成五块：

```
main.c 的结构
├── ① 头文件与常量定义（第 1~30 行附近）
├── ② 终端输出函数（terminal_*）
├── ③ 字符串与命令解析（strncmp / cmd_is / extract_quoted / process_command）
├── ④ 键盘处理（handle_keyboard）
└── ⑤ 入口函数（kmain）
```

下面逐块讲。

### 8.1 头文件与常量

```c
#include "acpi.h"      // 关机函数 acpi_power_off 的声明（来自 acpi/ 目录）
#include "fs.h"        // 文件系统接口（来自 fs/ 目录）
#include "str.h"       // 字符串工具（来自 fs/ 目录）
```

**你不用关心"为什么能 include 到别的目录的头文件"**——是 Makefile 里的 `-Iacpi -Ifs` 参数让编译器去那两个目录找的。你只需要知道：**要用文件系统，就 include "fs.h"；要用字符串工具，就 include "str.h"**。

常量一览：

| 常量 | 值 | 含义 |
|---|---|---|
| `VGA_MEMORY` | `0xB8000` | 显卡文本内存的地址。屏幕上显示的每一个字符，其实都存在于这块内存里（后面 8.2 细讲） |
| `VGA_ATTR` | `0x1F` | 字符的显示属性（前景色 + 背景色）。`0x1F` 是**白字蓝底** |
| `COLS` / `ROWS` | `80` / `25` | 屏幕分辨率：80 列 × 25 行 |
| `SCR_SIZE` | `80*25=2000` | 屏幕能容纳的字符总数 |
| `KEYBOARD_PORT` | `0x60` | 键盘数据端口（读按键扫描码） |
| `KEYBOARD_STATUS_PORT` | `0x64` | 键盘状态端口（判断有没有新按键） |
| `INPUT_BUF_SIZE` | `256` | 输入缓冲区的最大长度 |

全局变量：

```c
static char input_buf[INPUT_BUF_SIZE];   // 你输入的命令存放处（直到按回车）
static int  input_len = 0;               // 当前已输入多少个字符
static unsigned char shift_down = 0;     // Shift 键是否按着（用于上档字符）
static volatile unsigned short* vga = (volatile unsigned short*)VGA_MEMORY;
                                         // 指向屏幕内存的指针，往它写 = 往屏幕写
static unsigned int pos = 0;             // 光标当前在屏幕上的位置（0~1999）
```

**关键概念：屏幕就是一块内存**

VGA 文本模式把屏幕当作一块内存。屏幕上一个"字符格子"占 **2 个字节**：

```
字节 0：字符的 ASCII 码（比如 'A' 就是 65）
字节 1：显示属性（前景色/背景色/是否闪烁）
```

整个屏幕 = 80 列 × 25 行 = 2000 个格子 = 4000 字节的内存，地址从 `0xB8000` 开始。

所以 `vga[pos] = (VGA_ATTR << 8) | 'A';` 的意思就是"在屏幕第 pos 个格子里显示一个白色 'A'"。**这就是为什么内核没有 printf 也能显示文字**。

### 8.2 终端输出函数（terminal_*）

这组函数负责"往屏幕写字"。它们都是 `static`（只在 main.c 内部使用）。

| 函数 | 作用 | 类比 |
|---|---|---|
| `terminal_clear()` | 清空整个屏幕，光标回到左上角 | 相当于终端里的 `clear` 命令 |
| `terminal_putchar(char c)` | 在光标处显示一个字符，光标前进一格；`'\n'` 则跳到下一行开头 | 相当于 `putchar` |
| `terminal_write(const char* s)` | 把一个字符串（以 `'\0'` 结尾）逐个字符输出 | 相当于 `puts` |
| `terminal_backspace()` | 删除光标前一个字符（擦掉并用空格覆盖） | 相当于退格键 |

**几个你需要知道的特性：**

1. **字符串必须以 `'\0'` 结尾**。`terminal_write` 遇到 `'\0'` 才停止。如果你传给它一个没有 `'\0'` 结尾的数组，它会一直读下去直到撞上内存里的某个 `0`——这是经典的 C 错误（缓冲区越界），**写代码时务必保证字符串有 `'\0'` 结尾**。

2. **`terminal_putchar('\n')` 只换行、不回行首**。目前实现是"移到下一行开头"（`pos = ((pos/80)+1)*80`），这个行为已经足够，不用管。

3. **屏幕写满后不会滚动**。超过第 25 行时，光标会被"夹住"在最后一行，继续写会覆盖最后一行（`terminal_putchar` 里有 `if (pos >= SCR_SIZE)` 的处理）。所以调试时如果发现屏幕"不动了"，可能是写满了。

4. **没有 `printf`**。系统里没有格式化输出函数。想输出数字（比如文件大小）时，用 `itoa_dec`（8.6 节）把数字转成字符串再 `terminal_write`。**第 29 节会教你怎么自己加一个 printf**。

**修改欢迎语**（最常见的维护需求之一）在 `kmain` 里：

```c
terminal_write("Welcome to MaxZOS v0.9\n");   // ← 改这里的文字
terminal_write("made by ZhangMaixuan\n");     // ← 改这里的文字
```

### 8.3 字符串比较函数 strncmp

```c
static int strncmp(const char* s1, const char* s2, unsigned int n)
```

和 C 标准库的 `strncmp` 行为一致：比较 `s1` 和 `s2` 的前 `n` 个字符，相等返回 0，不等返回第一个不同字符的差值。**这是项目自己实现的，不要指望有标准库**。

**注意**：项目里还有 `fs/str.c` 里的 `strcmp`（比较整个字符串）和 `strlen`，它们和 main.c 里的 `strncmp` 是三个不同的函数，别搞混。

### 8.4 命令解析辅助函数

这四个小函数是命令系统的"基础设施"，理解它们之后，加新命令就很容易了。

**① `cmd_is(const char* name)` —— 判断用户输入的是不是某个命令**

```c
static int cmd_is(const char* name) {
    unsigned int n = strlen(name);
    return strncmp(input_buf, name, n) == 0 &&
           (input_buf[n] == '\0' || input_buf[n] == ' ');
}
```

逻辑翻译成大白话：

> 用户输入的开头正好是命令名，**并且**命令名后面要么是行尾（`'\0'`）、要么是空格——才算匹配成功。

为什么要有后半句？因为如果没有它，输入 `clearxxx` 也会被当成 `clear`（前缀匹配）。这就是代码注释里说的"修复 `clearxxx` 误命中 `clear`"的 bug。**以后你加命令，判断就用 `cmd_is("你的命令名")`，不要自己写 `strncmp` 判断**。

**② `skip_spaces(char* p)` —— 跳过一串前导空格**

```c
static char* skip_spaces(char* p) {
    while (*p == ' ') p++;
    return p;
}
```

用于"命令名后面可能有多个空格，我要跳到第一个非空格字符"。

**③ `extract_quoted(char** pp)` —— 提取一个参数，支持双引号**

这个函数最微妙（涉及双重指针），它是 `echo` 和 `create` 共用的"参数提取器"。规则：

- 跳过前导空格
- 如果参数以 `"` 开头：把引号里的内容提取出来（引号不算内容），**并在内容末尾补 `'\0'`**；如果引号没有闭合，返回 `NULL`（调用方输出 "unclosed quote"）
- 如果不是引号开头：直接返回原指针（参数就是剩余原文）

用例子说明（假设输入是 `echo "hello world"`）：

```
input_buf 内存内容：  e c h o   " h e l l o   w o r l d "
                                              ↑
extract_quoted 把这里的引号改成 '\0'，返回指向 h 的指针
```

所以 `echo "hello world"` 输出的是 `hello world`（不带引号）。这就是"内容含空格时用双引号"的实现原理。

**双重指针 `char**` 的作用**：函数通过 `*pp` 把"参数结束后的位置"传回给调用方，方便调用方继续处理后面的内容。**你不必完全理解它，照抄这个模式用即可**（第 15 节有完整例子）。

**④ `term_write_cb(const char* s)` —— 输出回调**

```c
static void term_write_cb(const char* s) {
    terminal_write(s);
}
```

这是给 `fs_list` 用的"回调函数"（见 9.4 节）。它只是把 `terminal_write` 包装了一下，让文件系统模块能"把输出交给终端"而不用依赖 main.c。

### 8.5 命令解析主函数 process_command

`process_command()` 是命令系统的心脏：**用户按回车后，它被调用一次，负责把 `input_buf` 里的命令拆解、匹配、执行**。

它的整体骨架：

```c
static void process_command(void) {
    terminal_putchar('\n');              // ① 先换行（因为输入时没有换行）
    if (input_len == 0) {                // ② 空命令？直接打提示符
        terminal_write("os/> ");
        return;
    }
    if (cmd_is("clear") || cmd_is("cls")) {
        terminal_clear();                // ③ 命令匹配与执行（一大串 else if）
    } else if (cmd_is("echo")) {
        ...
    } else if (cmd_is("create")) {
        ...
    } else if (cmd_is("cat")) {
        ...
    } else if (cmd_is("delete")) {
        ...
    } else if (cmd_is("ls")) {
        ...
    } else if (cmd_is("exit")) {
        terminal_write("Shutting down...\n");
        acpi_power_off();                // 真正关机（这个函数在 acpi/ 里）
    } else {
        terminal_write("unknown command\n");   // ④ 都不匹配
    }
    input_len = 0;                       // ⑤ 清空输入缓冲
    terminal_write("os/> ");             // ⑥ 打印下一个提示符
}
```

**执行顺序的细节**：

1. 先换行——因为用户输入命令时，屏幕上的光标在命令文本末尾，回车后先换一行再输出结果。
2. 空命令（直接按回车）不报错，直接给新提示符。
3. 命令匹配用 `cmd_is`，**从上到下逐个试**。一旦匹配就执行并结束（`else if` 链）。
4. 全部不匹配 → `unknown command`。
5. 执行完后 `input_len = 0`：清空缓冲，等待下一条命令。**注意：input_buf 的内容并没有被清空**（只是长度归零），下次输入会从开头覆盖写——这是刻意的，节省时间。

**当前命令清单与行为**：

| 命令 | 匹配条件 | 行为 |
|---|---|---|
| `clear` / `cls` | `cmd_is("clear") \|\| cmd_is("cls")` | 清屏 |
| `echo <内容>` | `cmd_is("echo")` | 输出内容；双引号包裹时剥离引号；未闭合引号报 "unclosed quote" |
| `create <文件名> [内容]` | `cmd_is("create")` | 创建文件；无内容时提示 usage；成功无输出；失败输出错误消息 |
| `cat <文件名>` | `cmd_is("cat")` | 显示文件内容；失败输出错误消息 |
| `delete <文件名>` | `cmd_is("delete")` | 删除文件；失败输出错误消息 |
| `ls` | `cmd_is("ls")` | 列出所有文件：`文件名  大小`，每行一个 |
| `exit` | `cmd_is("exit")` | 打印 "Shutting down..." 并 ACPI 关机 |

**命令的边界行为（有意设计，测试时用得上）**：

- `echo "hello`（未闭合引号）→ `unclosed quote`
- `create foo`（只有文件名没内容）→ `usage: create <name> [content]`（创建空文件要用 `create foo ""`）
- `cat foo bar` → 整个 `foo bar` 被当作文件名查找 → `file not found`（安全，不会误操作）
- `clearxxx` → `unknown command`（因为 `cmd_is` 的边界检查）

### 8.6 键盘处理 handle_keyboard

`handle_keyboard()` 是系统唯一的"事件源"——**所有输入都从它来**。它在 `kmain` 的死循环里被反复调用：

```c
while (1) {
    handle_keyboard();
}
```

它的完整流程：

```
① 读键盘状态端口 0x64，检查 bit0（有没有新按键数据）
   ├─ 没有 → 直接返回（下次循环再来）
   └─ 有 → 继续
② 读数据端口 0x60，拿到扫描码（一个数字，比如 30 = 按键 A）
③ 处理特殊键：
   ├─ 0x2A/0x36（Shift 按下）→ shift_down = 1
   ├─ 0xAA/0xB6（Shift 松开）→ shift_down = 0
   └─ 带 0x80 的码（按键松开）→ 忽略
④ 用扫描码查表得到字符（kbdus / kbdus_shift 两张表）
   ├─ 是回车 → 执行 process_command()（进入命令解析）
   ├─ 是退格 → input_len-- 并调用 terminal_backspace()
   ├─ 是普通字符 → 存进 input_buf 并回显到屏幕
   └─ 查表结果是 0（未映射的键）→ 忽略
```

**两个你需要知道的细节**：

1. **kbdus / kbdus_shift 是两张对照表**：普通映射表（`kbdus`）和按住 Shift 时的映射表（`kbdus_shift`）。想改按键映射（比如把 Caps Lock 映射成别的），改这两张表即可——但**不建议**，键盘映射表很容易改乱。

2. **缓冲区保护**：`if (input_len < INPUT_BUF_SIZE - 1)` 保证输入不会超过 256 字节。输入满了之后按什么键都不再进缓冲（但也不报错）。如果以后你嫌 256 不够，改 `INPUT_BUF_SIZE` 的值即可（**一个常量，全局生效**，第 18 节有示例）。

### 8.7 入口函数 kmain

```c
void kmain(unsigned long magic, unsigned long addr) {
    fs_init();                    // ① 初始化文件系统（目前是空操作）
    terminal_clear();             // ② 清屏
    terminal_write("Welcome to MaxZOS v0.9\n");   // ③ 欢迎语（可改）
    terminal_write("made by ZhangMaixuan\n");     // ④ 署名（可改）
    terminal_write("os/> ");      // ⑤ 初始提示符（可改）
    while (1) {                   // ⑥ 主循环：永远等待键盘
        handle_keyboard();
    }
}
```

**关于两个参数**：`magic` 是引导程序给的内核魔数（校验用的），`addr` 是 multiboot 信息结构指针。**目前代码没有使用它们**，编译器因此给出 "unused parameter" 警告——**这是正常警告，不是错误，不用管它**。

**kmain 的职责**：初始化一切 → 显示界面 → 进入主循环。**你以后想"开机时做什么"（比如开机自动创建文件、显示版本号），就改这里**。

## 9. fs/ 目录 —— 文件系统模块（扩展的主战场）

`fs/` 目录包含两个文件对：`fs.c/h`（文件系统本体）和 `str.c/h`（字符串工具）。

### 9.1 设计理念：一切皆静态数组

这是整个文件系统**最重要的设计决策**，先理解它：

> **文件系统不使用动态内存（malloc），所有数据放在编译时就确定大小的静态数组里。**

具体来说：

```c
static file_t files[FS_MAX_FILES];   // 32 个文件槽位，编译时分配好
```

- 文件系统最多同时存在 32 个文件（`FS_MAX_FILES`）
- 每个文件的"槽位"大小固定：文件名 32 字节 + 内容 257 字节 + 两个数字
- 整个表大约 9KB 内存，在 `.bss` 段（自动清零的全局区）

**为什么这样设计**：内核没有 `malloc`（没有堆管理器），也不能用操作系统的内存分配。静态数组是最简单、最不容易出错的方案。

**含义**：你以后扩展文件系统（比如加权限字段、加修改时间），就是**往 `file_t` 结构体里加字段**，其余逻辑照旧——因为空间是提前分配的，只是每个槽位变大一点。

### 9.2 常量与数据结构（fs.h）

`fs.h` 是文件系统的"合同"，分为三部分：

**① 规模常量**（改这些就能调整系统的容量上限）：

```c
#define FS_MAX_FILES     32    // 最多同时存在的文件数
#define FS_MAX_NAME_LEN  32    // 文件名缓冲区大小（含 '\0'），实际最长 31 字符
#define FS_MAX_CONTENT   256   // 每个文件内容最多 256 字节
```

**② 错误码枚举**：

```c
typedef enum {
    FS_OK = 0,          // 成功
    FS_EXISTS,          // 同名文件已存在
    FS_NOT_FOUND,       // 文件不存在
    FS_FULL,            // 文件表已满
    FS_EMPTY_NAME,      // 文件名为空
    FS_NAME_TOO_LONG,   // 文件名超过 31 字符
    FS_BAD_CONTENT,     // 内容超过 256 字节
} fs_status_t;
```

**这是本项目的"返回码规范"**：所有文件系统函数都返回这个枚举。你调用时用 `if (st != FS_OK)` 判断是否成功。**想加新的错误情况（比如"文件被锁定"）就在枚举末尾加一项**，然后记得在 main.c 的 `fs_err_str` 里加对应的错误消息（第 22 节有示例）。

**③ 文件结构体**：

```c
typedef struct {
    char  name[FS_MAX_NAME_LEN];      // 文件名（字符串，'\0' 结尾）
    char  content[FS_MAX_CONTENT + 1];// 文件内容（256 + 1 个 '\0'）
    unsigned short len;               // 内容实际长度（不含 '\0'）
    unsigned char  used;              // 1 = 这个槽位正在被使用，0 = 空闲
} file_t;
```

**核心概念：文件表是一个"槽位数组"**。`used` 字段标记哪个槽位有文件——删除文件不是"抹掉内容"，而是把 `used` 改成 0（内容留在原地但已不可访问，下次创建文件会覆盖它）。

### 9.3 公开接口（fs.h 中声明的函数）

| 函数 | 作用 | 返回值 |
|---|---|---|
| `fs_status_t fs_create(const char* name, const char* content)` | 创建文件 | `FS_OK` 或错误码 |
| `fs_status_t fs_read(const char* name, char* out, unsigned int maxlen)` | 读出文件内容到 `out` | `FS_OK` / `FS_NOT_FOUND` |
| `fs_status_t fs_delete(const char* name)` | 删除文件 | `FS_OK` / `FS_NOT_FOUND` |
| `void fs_list(fs_out_fn out)` | 列出所有文件（通过回调输出） | 无 |
| `void fs_init(void)` | 初始化文件系统 | 无 |

**这四个函数就是"文件系统的全部对外接口"。main.c 的命令就是调它们实现的。**

调用示例（读文件）：

```c
char buf[257];                 // 内容最多 256 字符 + '\0'
fs_status_t st = fs_read("note", buf, sizeof(buf));
if (st == FS_OK) {
    terminal_write(buf);       // 读出成功，直接输出
} else {
    terminal_write("file not found\n");
}
```

**关于 `fs_read` 的细节**：`maxlen` 是目标缓冲区大小。它最多复制 `maxlen - 1` 字节，然后**一定补一个 `'\0'` 结尾**。所以调用时缓冲区至少留 1 字节余量（如 `char buf[257]` + `sizeof(buf)`）。

**关于 `fs_list` 的回调**：`fs_list` 不直接打印，而是每找到一个文件就调用一次你给它的函数 `out`，把"一行文本"（如 `note  8\n`）传给它。main.c 里传的是 `term_write_cb`（= terminal_write）。**这样设计的好处**：文件系统模块不依赖任何具体的输出设备，将来可以接串口、接文件。

### 9.4 内部实现（fs.c）

`fs.c` 内部有四个"非公开"函数（`static`，外部看不到）：

| 函数 | 作用 |
|---|---|
| `static int fs_find(const char* name)` | 在文件表里找同名文件，返回槽位下标（找不到返回 -1） |
| `static int fs_find_free(void)` | 找一个空闲槽位，返回下标（没有返回 -1） |
| `static fs_status_t fs_add(...)` | 共用的"写入核心"：完成全部校验后把数据写入槽位 |
| `files[FS_MAX_FILES]` | 全局文件表（在 `.bss` 段，开机自动全 0 = 全部空闲） |

**`fs_add` 的校验顺序**（这是文件系统最重要的逻辑，创建文件的所有规则都在这里）：

```
① 文件名为空？        → FS_EMPTY_NAME
② 文件名超长（≥32）？ → FS_NAME_TOO_LONG
③ 已有同名文件？      → FS_EXISTS
④ 内容超长（>256）？  → FS_BAD_CONTENT
⑤ 文件表满了？        → FS_FULL
⑥ 全部通过 → 找到空闲槽，复制名字、复制内容、记长度、置 used=1
```

**顺序很重要**：比如"重名"的检查在"内容超长"之前，所以 `create aaa aaa...`（内容超长且重名）报的是重名错误。以后加新校验（如"文件名不能含特殊字符"）也按这个思路**插在合适的位置**。

**`fs_init` 目前是空壳**：

```c
void fs_init(void) {
    /* 预留：将来解析 multiboot modules / 挂载磁盘后端 */
}
```

因为 `.bss` 段自动清零，文件表天然是"全空"状态，所以初始化无事可做。**将来做持久化（从磁盘加载文件）时，把加载逻辑写在这里**（第 35 节）。

### 9.5 str.c —— 字符串工具

`fs/str.c` 提供了四个函数（声明在 `str.h`）：

| 函数 | 作用 | 类比标准库 |
|---|---|---|
| `unsigned int strlen(const char* s)` | 求字符串长度（不含 '\0'） | `strlen` |
| `int strcmp(const char* a, const char* b)` | 比较两个字符串是否相同（返回 0 表示相同） | `strcmp` |
| `char* strcpy(char* dst, const char* src)` | 把 src 复制到 dst（含 '\0'） | `strcpy` |
| `void itoa_dec(unsigned int value, char* buf)` | 把无符号整数转成十进制字符串 | `sprintf(buf, "%u", value)` |

**⚠️ 重要提醒：`itoa_dec` 的缓冲区必须至少 12 字节**（32 位无符号数最多 10 位数字 + '\0'）。写 `char num[12];` 然后用，别省空间。

**想加新字符串函数**（比如 `strchr` 找字符、`stoupper` 转大写）：在 `str.h` 声明、`str.c` 实现，格式照抄现有函数。加完不需要改 Makefile（`str.o` 已经在编译列表里），main.c / fs.c 里直接调用即可。

## 10. acpi/ 目录 —— 电源管理（了解即可）

`acpi/acpi.h` 只声明了一个函数：

```c
void acpi_power_off(void);   // 真正关闭电脑电源
```

`acpi/acpi.c` 内部做的事情（你不用懂细节）：扫描内存找 ACPI 表 → 解析出关机方法 → 写电源管理寄存器 → 断电。它甚至还有"兜底方案"：如果 ACPI 找不到，直接写 QEMU 和 Bochs 模拟器的专用关机端口。

**你只需要知道三件事**：

1. `exit` 命令调用 `acpi_power_off()` 实现关机
2. **这个模块通常不需要改**
3. 如果哪天它出了问题（比如关机失效），一个临时替代方案是**死循环**：`while(1);`（QEMU 会继续运行，但至少不崩溃）——真正的修复需要懂 ACPI，建议交给懂的人

## 11. kernel/ 目录 —— 三个"黑盒"（不要动）

`kernel/` 下有三个文件，它们的共同特点是：**对维护者来说是不需要理解的黑盒，改了反而容易坏**。

### 11.1 boot.asm

一小段汇编，职责是：设置栈指针 → 调用 C 的 `kmain`。它本质上是"C 世界的大门"：

```asm
_start:
    mov esp, stack_top   ; 设置栈
    push ebx             ; 传第 2 个参数（multiboot 信息）
    push eax             ; 传第 1 个参数（magic）
    call kmain           ; 进入 C 代码！
```

**不要修改的原因**：`kmain` 被调用的约定（参数怎么传、栈怎么设）全在这里。改错一行，系统直接起不来，而且你（不熟悉汇编）很难查。

### 11.2 multiboot_header.asm

定义了 Multiboot 协议头（magic、flags、checksum 三个数字），是 GRUB 识别"这是一个可启动内核"的依据。**永远不要动**——动了 GRUB 可能直接拒绝加载。

### 11.3 linker.ld

链接脚本，规定了内核各段（代码、数据、BSS）在内存中的摆放位置：

```
起始地址 1MB → multiboot 头 → 代码段 → 只读数据 → 数据段 → BSS（含 16KB 栈）
```

**不要修改的原因**：这个布局目前完全够用。改错地址会导致内核加载后崩溃，调试难度极高。**特别注意**：如果你在 `file_t` 里加字段导致 `.bss` 变大，**不需要**改链接脚本——`.bss` 会自己往后长，链接器自动处理。

## 12. Makefile —— 构建说明书（已自动扫描，几乎不用动）

Makefile 已经在前文（第 5 节）介绍了原理。这里补充**维护者需要知道的全部细节**。

**好消息**：这个 Makefile 已经升级为**自动扫描**模式——新加的 `.c`/`.asm` 文件会被自动纳入构建，**通常完全不需要改 Makefile**。

### 12.1 自动搜集源文件（核心机制）

```make
C_SRCS   = $(wildcard *.c acpi/*.c fs/*.c kernel/*.c)   # 扫描这几个位置的所有 .c 文件
ASM_SRCS = $(wildcard kernel/*.asm)                     # 扫描 kernel/ 下的所有 .asm 文件
OBJS     = $(addprefix $(BIN)/, $(notdir $(C_SRCS:.c=.o)) $(notdir $(ASM_SRCS:.asm=.o)))
```

翻译成大白话：

> **每次执行 `make` 时，它会自动查看"根目录、acpi/、fs/、kernel/"这几个位置有哪些 `.c` 和 `.asm` 文件，把它们全部列进编译清单**。

- `$(wildcard 模式)`：make 的通配符——自动列出匹配的文件。你**新加一个 `.c` 文件**，它下次运行时会自动被找到
- `$(notdir ...)`：把 `acpi/acpi.c` 这类路径变成纯文件名 `acpi.c`（目标文件统一平铺在 `bin/` 下）
- `%.c → %.o`：把 `.c` 后缀换成 `.o`

**所以：把新文件放在根目录、`acpi/`、`fs/`、`kernel/` 这四个位置的任意一处，什么都不用改，`make` 就会编译它。**

### 12.2 编译规则（模式规则）

```make
$(BIN)/%.o: %.c | $(BIN)        # 规则 1：根目录的 .c
$(BIN)/%.o: acpi/%.c | $(BIN)   # 规则 2：acpi/ 的 .c
$(BIN)/%.o: fs/%.c | $(BIN)     # 规则 3：fs/ 的 .c
$(BIN)/%.o: kernel/%.asm | $(BIN)  # 规则 4：kernel/ 的 .asm
```

`%` 是通配符。以 `bin/acpi.o` 为例，make 会依次尝试四条规则，找到源文件**真实存在**的那一条（`acpi/acpi.c`）来编译。

**⚠️ 唯一的限制**：**不同目录下的源文件不能同名**。比如根目录和 `fs/` 里不能同时有 `foo.c`——那样 `foo.o` 不知道编译哪个。命名时避开即可。

### 12.3 自动头文件依赖（改头文件自动重编）

```make
CFLAGS += -MMD -MP          # 编译时自动生成 bin/*.d 依赖文件
-include $(DEPS)            # make 启动时加载这些依赖文件
```

**作用**：`gcc` 编译每个 `.c` 文件时，会顺手生成一个 `.d` 文件（记录"这个 .o 依赖哪些头文件"，比如 `main.o` 依赖 `fs.h`）。make 加载它们后，**你改了 `fs.h`，make 会自动重新编译所有 include 了它的 .c 文件**——不需要任何人手动维护依赖列表。

**验证方法**（想确认这个机制在干活）：

```bash
cat bin/main.d
# 输出类似：bin/main.o: main.c acpi/acpi.h fs/fs.h fs/str.h
touch fs/fs.h && make      # 观察：main.o 和 fs.o 被自动重编，acpi.o 不动
```

### 12.4 什么时候才需要真的动 Makefile

只有以下两种情况：

| 情况 | 怎么改 |
|---|---|
| 新增了一个**目录**（比如未来的 `lib/`）来放源文件 | ① 在 `C_SRCS` 的 `$(wildcard ...)` 里加上 `lib/*.c`；② 加一条模式规则 `$(BIN)/%.o: lib/%.c`（照抄 12.2 格式）；③ 在 `CFLAGS` 里加 `-Ilib`（如果新目录有头文件） |
| 修改编译参数（颜色/优化等级等） | 改 `CFLAGS` 行 |

**其余情况（加文件、改代码、加头文件）一律不需要动 Makefile。**

## 13. grub.cfg —— 引导配置

打包进 ISO 的引导菜单配置。当前内容：

```
menuentry "My OS - Hello World" {
    multiboot /boot/kernel.elf
    boot
}
```

意思是：启动菜单里有一项"Hello World"，它加载 `/boot/kernel.elf`（即 ISO 内的内核）并启动。**一般不用动**。菜单显示的文字（"My OS - Hello World"）想改可以改，不影响功能。

# 第四部分　维护操作手册

> 这一部分是**操作手册**：每个小节解决一类具体的维护需求，按步骤操作即可。每一节都遵循"改哪里 → 怎么改 → 怎么验证"的格式。
>
> **通用验证方法**（每一节都适用）：改完代码 → 终端执行 `make run` → 在 QEMU 里输入相关命令验证 → 没问题后 `make` 打包并提交 git。

## 14. 修改欢迎语和系统提示符

**目标**：改开机显示的文字、改命令行的提示符（现在是 `os/> `）。

**修改文件**：`main.c`（根目录）

**步骤**：

1. 打开 `main.c`，找到 `kmain` 函数：

```c
void kmain(unsigned long magic, unsigned long addr) {
    fs_init();
    terminal_clear();
    terminal_write("Welcome to MaxZOS v0.9\n");    // ← 开机第一行
    terminal_write("made by ZhangMaixuan\n");      // ← 开机第二行
    terminal_write("os/> ");                       // ← 初始提示符
    while (1) {
        handle_keyboard();
    }
}
```

2. 直接改引号里的文字即可。注意：
   - `\n` 是换行符（C 语言标准写法）
   - **提示符有三个地方**：`kmain` 里的初始提示符、`process_command` 里的两处 `terminal_write("os/> ")`（空命令分支和函数末尾）。想改提示符文字（比如改成 `MaxZOS$ `），需要改**全部三处**，否则会出现"开机是一个提示符、回车后变另一个"的割裂现象。

**验证**：`make run` → 开机显示新文字 → 随便执行一个命令后提示符保持新样式。

**常见错误**：

- 只改了一处提示符 → 提示符时变时不变（补齐另外两处）
- 字符串里写中文 → **屏幕上显示乱码**。内核的 VGA 文本模式只支持 ASCII 字符（英文、数字、基本标点）。**想显示中文需要完整的字库和绘图代码，不在本手册范围**。所以界面文字请用英文。

## 15. 修改屏幕颜色

**目标**：把默认的白字蓝底改成其他颜色。

**修改文件**：`main.c`（一个常量）

**步骤**：

1. 找到常量定义：

```c
#define VGA_ATTR    0x1F    // 字符显示属性
```

2. 修改数值。这个字节的格式是：

```
高位 4 位 = 背景色，低位 4 位 = 前景色
十六进制写法： 0x背景前景
```

常用组合：

| 值 | 效果 |
|---|---|
| `0x1F`（当前） | 白字蓝底 |
| `0x0F` | 白字黑底（最经典） |
| `0x0A` | 绿字黑底 |
| `0x0C` | 红字黑底 |
| `0x1E` | 黄字蓝底 |
| `0x70` | 黑字白底（反白） |
| `0x2F` | 白字绿底 |

颜色编号：0=黑 1=蓝 2=绿 3=青 4=红 5=紫 6=棕 7=白 8=灰 ……

3. 改完 `make run` 查看效果。

**原理**：所有输出都经过 `VGA_ATTR`（`terminal_putchar` 里的 `vga[pos] = (VGA_ATTR << 8) | c`）。改这一个常量，全系统颜色统一变化。

**进阶**：如果想"某些行用特殊颜色"（比如错误消息红色），可以加一个带颜色参数的输出函数。例：

```c
/* 在 main.c 加一个新函数：带颜色输出一行（其余行不受影响） */
static void terminal_write_attr(const char* s, unsigned char attr) {
    while (*s) {
        vga[pos] = (attr << 8) | *s;   // 注意：用 attr 而不是 VGA_ATTR
        pos++;
        s++;
        if (pos >= SCR_SIZE) pos = (ROWS - 1) * COLS;
    }
}
```

（如需正确处理 `'\n'`，可参照 `terminal_putchar` 的写法。）然后错误输出可以调用 `terminal_write_attr("file not found\n", 0x0C)` 显示红字。

## 16. 新增第一个命令（零基础手把手教程）

> 这一节是**给新手的第一个实战练习**：添加一个叫 `about` 的命令，输入后显示版本信息。
> 我们不急着写代码，先把"命令解析器"这个你即将修改的东西彻底看懂，再动手改。

### 16.0 第一步：找到要修改的代码

1. 用任意文本编辑器打开项目根目录下的 **`main.c`**（VS Code、记事本、vim 都可以）。
2. 在编辑器里按 `Ctrl + F`（查找），输入：

```
process_command
```

3. 找到第一个结果——它就是**命令解析器**。你会看到一个大函数：

```c
static void process_command(void) {
    // 1. 先换行（因为输入是在同一行）
    terminal_putchar('\n');

    // 2. 判断是否为空命令
    if (input_len == 0) {
        terminal_write("os/> ");
        return;
    }

    // 3. 命令匹配
    if (cmd_is("clear") || cmd_is("cls")) {
        // 清屏（两个命令等价）
        terminal_clear();
    } else if (cmd_is("echo")) {
        // echo：无引号输出原文，双引号提取引号内容
        char* p = input_buf + 4;
        char* arg = extract_quoted(&p);
        if (arg == NULL) {
            terminal_write("unclosed quote\n");
        } else {
            terminal_write(arg);
            terminal_putchar('\n');
        }
    } else if (cmd_is("create")) {
        ...
    } else if (cmd_is("cat")) {
        ...
    } else if (cmd_is("delete")) {
        ...
    } else if (cmd_is("ls")) {
        // ls：列出所有文件及大小
        fs_list(term_write_cb);
    } else if (cmd_is("exit")) {
        // exit：ACPI 关机（正常情况不会返回）
        terminal_write("Shutting down...\n");
        acpi_power_off();
    } else {
        terminal_write("unknown command\n");
    }

    input_len = 0;
    terminal_write("os/> ");
}
```

（上面省略号 `...` 是中间的命令分支，实际文件里是完整的。**你现在不需要读中间的部分，只需要看懂结构**。）

### 16.1 理解你要修改的东西：else if 链

**这个函数的核心是一个"检查链"**。它从上到下一句一句地问：

```
用户输入的是 clear 吗？→ 是：清屏，结束
用户输入的是 echo 吗？ → 是：执行 echo，结束
用户输入的是 create 吗？→ 是：执行 create，结束
...（一路问下去）...
用户输入的是 exit 吗？ → 是：关机，结束
都不是？              → 输出 "unknown command"
```

用现实类比：**安检通道**。乘客（你的输入）走过一排检查口，哪个口认你，就在哪个口被处理；所有口都不认，就送去"unknown command"（未知命令）出口。

**每个"检查口"在代码里长这样**：

```c
} else if (cmd_is("exit")) {      // 检查口：是 exit 吗？
    terminal_write("Shutting down...\n");   // 是：干这些事
    acpi_power_off();
}                                 // 这个口结束
```

**你要做的，就是在这条链上"加一个新的检查口"**。就这么简单。

### 16.2 理解 cmd_is：检查口是怎么认人的

```c
static int cmd_is(const char* name) {
    unsigned int n = strlen(name);
    return strncmp(input_buf, name, n) == 0 &&
           (input_buf[n] == '\0' || input_buf[n] == ' ');
}
```

拆成三部分理解：

| 部分 | 干什么 | 类比 |
|---|---|---|
| `strlen(name)` | 算出命令名有几个字符（`"exit"` 就是 4） | 数牌子上的字 |
| `strncmp(input_buf, name, n) == 0` | 比较用户输入的开头 n 个字符和命令名是否相同 | 牌子上的字对上了 |
| `input_buf[n] == '\0' \|\| input_buf[n] == ' '` | 检查第 n 个字符是"结束"或"空格" | 牌子后面没有多余的字 |

**你不需要写 cmd_is，只需要用它**。它的用法永远一样：`cmd_is("你的命令名")`。

### 16.3 实战：添加 about 命令

**目标**：用户输入 `about` 后，屏幕显示：

```
MaxZOS v0.9 - a toy OS
```

**第 1 步：确定插入位置**

在你的编辑器里，滚动到 `process_command` 函数，找到 `exit` 分支这一行：

```c
    } else if (cmd_is("exit")) {
```

**我们插在它前面**（插在它后面也可以，只要在整条链里就行；插在 `exit` 前面只是为了让 `exit` 留在链尾，逻辑清晰）。

**第 2 步：写代码**

把下面这段**完整地**插入到 `exit` 分支的上一行和 `exit` 分支之间（也就是 `ls` 分支的 `}` 之后、`} else if (cmd_is("exit"))` 之前）：

```c
    } else if (cmd_is("about")) {
        // about：显示版本信息
        terminal_write("MaxZOS v0.9 - a toy OS\n");
    }
```

插入后，这一带看起来应该是这样（**注意 `about` 分支的前后衔接**）：

```c
    } else if (cmd_is("ls")) {
        // ls：列出所有文件及大小
        fs_list(term_write_cb);
    } else if (cmd_is("about")) {                     // ← 你插入的部分从这里开始
        // about：显示版本信息
        terminal_write("MaxZOS v0.9 - a toy OS\n");
    } else if (cmd_is("exit")) {                      // ← 到这里结束
        // exit：ACPI 关机（正常情况不会返回）
        terminal_write("Shutting down...\n");
        acpi_power_off();
    } else {
```

**新代码逐行解释**：

| 代码 | 意思 |
|---|---|
| `} else if (cmd_is("about")) {` | 新增的检查口：如果输入是 `about`，进这个分支 |
| `// about：显示版本信息` | 注释：说明这个分支干什么（注释不参与运行，但必须写，是给未来的人看的） |
| `terminal_write("MaxZOS v0.9 - a toy OS\n");` | 在屏幕上输出这行文字。`\n` 是换行符 |
| `}` | 这个分支结束 |

**关于 `terminal_write` 的用法**：它接受一个**字符串字面量**（双引号括起来的文字），像 `terminal_write("hello\n")` 这样。想输出什么就写什么，**记得结尾加 `\n` 换行**（否则下一行提示符会紧贴在你的文字后面）。

**第 3 步：保存文件**

在编辑器里按 `Ctrl + S`。确认保存的是**项目根目录下的 `main.c`**（不是别的文件）。

**第 4 步：编译并运行**

回到终端，执行：

```bash
make run
```

如果一切正常，QEMU 会打开。如果代码有错，终端会显示编译错误（红色的 `error:` 行）——**先别慌，看第 16.5 节**。

**第 5 步：测试**

在 QEMU 里输入：

```
os/> about
MaxZOS v0.9 - a toy OS
os/> aboutx
unknown command
```

**期望结果**：
- 输入 `about` → 显示版本信息 ✓
- 输入 `aboutx`（多了一个 x）→ `unknown command`。**这是故意的**：`cmd_is` 会检查命令名后面必须是结束或空格，所以 `aboutx` 不会被当成 `about`。这证明你的命令边界检查是好的。

**第 6 步：存档**

测试通过后，回到终端（QEMU 可以关掉或留着），执行：

```bash
git add main.c
git commit -m "add about command"
```

（git 的用法详见第 28 节。养成"改完即存档"的习惯。）

### 16.4 加第二条命令？一模一样

想加 `version`、`date`、`info`……任何无参数命令，**重复第 16.3 节的第 2 步**，换个名字和输出文字即可：

```c
    } else if (cmd_is("version")) {
        // version：显示版本号
        terminal_write("MaxZOS 0.9\n");
    }
```

**一句话总结本节**：加无参数命令 = 在 `process_command` 的检查链里抄一个 `else if` 分支，改名字、改输出文字。

### 16.5 新手最容易犯的 5 个错误（加命令时）

| # | 错误 | 现象 | 怎么办 |
|---|---|---|---|
| 1 | 忘写分号 `;` | 编译报 `expected ';'` | 在 `terminal_write(...)` 那行结尾补分号。**每行语句结尾都有分号** |
| 2 | 花括号 `{}` 少一个 | 编译报 `expected '}'` 或 `expected declaration` | 数一下：每个 `else if` 分支一对 `{}`，你的新分支应该也是完整的一对 |
| 3 | 分支插到 `else {` 后面了 | 编译报错，或"unknown command"分支里出现了你的代码 | `else {` 是最后兜底分支，**新命令必须插在它前面**（它是 `else if` 链的终点） |
| 4 | 命令名拼写不一致 | 编译通过，但输入命令永远 unknown command | `cmd_is("about")` 里的字符串和你在键盘上输入的要完全一致（大小写敏感） |
| 5 | 复制粘贴时把 `}` 多复制了一个 | 编译报 `expected '}'` 或逻辑错乱 | 对照第 16.3 节的完整示例，检查你贴进去的部分前后各是什么 |

## 17. 新增一个带参数的命令（零基础手把手教程）

> 这一节实战：添加一个 `add` 命令，把两个数字加起来并显示结果。
> 比如输入 `add 3 5`，屏幕显示 `8`。
> 这是"带参数命令"的最小完整例子——理解了它，所有带参数命令都是同一套路。

### 17.0 先理解：命令的参数存在哪里？

当你输入 `add 3 5` 并按下回车时，这整行文字会按顺序存进一个叫 **`input_buf`** 的字符数组里：

```
input_buf 内存内容（每个格子一个字符）：

下标:  0   1   2   3   4   5   6   7
      ┌───┬───┬───┬───┬───┬───┬───┬───┐
      │ a │ d │ d │   │ 3 │   │ 5 │ \0│
      └───┴───┴───┴───┴───┴───┴───┴───┘
       命令名  空格  第1个数 空格  第2个数  结束符
```

**两个关键概念**：

1. **`'\0'`（结束符）**：C 语言里字符串以 `'\0'` 结尾。输入命令回车时，系统会在末尾自动补上它（`input_buf[input_len] = '\0'`）。**`'\0'` 不是字符 '0'，而是一个值全 0 的特殊符号**，用来表示"字符串到这里结束"。

2. **空格是参数的分隔符**：`add 3 5` 里有两个空格，把字符串切成三段：`add`、`3`、`5`。

**"切参数"这个操作的实现技巧**：把分隔符（空格）**改成 `'\0'`**。改完之后，内存变成：

```
      ┌───┬───┬───┬───┬───┬───┬───┬───┐
      │ a │ d │ d │\0 │ 3 │\0 │ 5 │ \0│
      └───┴───┴───┴───┴───┴───┴───┴───┘
       "add"（结束）  "3"（结束）  "5"
```

这时 `input_buf`（指向开头）是字符串 `"add"`，`input_buf+4` 是字符串 `"3"`，`input_buf+6` 是字符串 `"5"`。**一个数组被切成了三段独立的字符串**——这就是"解析参数"的本质。

### 17.1 先准备一个工具：字符串 → 数字

`add 3 5` 里的 `3` 是**字符** `'3'`，要算加法得先转成**数字** `3`。项目里还没有这个函数，我们自己加（这也是一次"给 str 模块加新函数"的完整练习）。

**第 1 步：打开 `fs/str.h`**，在最后一行（`#endif` 前面）加声明：

```c
int atoi(const char* s);   /* 把 "123" 转成 123；不是数字时返回 0 */
```

**第 2 步：打开 `fs/str.c`**，在文件末尾加实现：

```c
/* 字符串转整数：逐个字符累加；遇到非数字字符就停止 */
int atoi(const char* s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {   /* 当前字符是数字 0~9 吗？ */
        n = n * 10 + (*s - '0');       /* 累加：之前的数 ×10，再加上这一位 */
        s++;                           /* 指向下一个字符 */
    }
    return n;
}
```

**逐行解释**（这段代码值得看懂，它是"解析数字"的标准写法）：

| 代码 | 意思 |
|---|---|
| `while (*s >= '0' && *s <= '9')` | 循环条件：当前字符的编码在数字 0~9 的范围内。`'0'`~`'9'` 在字符编码里是连续的一段，所以可以这样比较。遇到空格、字母或 `'\0'` 就停止 |
| `n = n * 10 + (*s - '0')` | 核心算式。`*s - '0'` 把字符 `'3'` 变成数字 3（字符编码相减）。`n * 10` 把前面的数往左挪一位。例：`"35"` → 先 `0*10+3=3`，再 `3*10+5=35` |
| `s++` | 指针移到下一个字符 |
| `return n` | 返回结果。**注意：如果第一个字符就不是数字，循环一次都不执行，返回 0** |

**第 3 步：保存这两个文件。不需要改 Makefile**（`str.o` 已经在编译列表里，新函数自动生效）。

### 17.2 写 add 命令的分支

**第 1 步**：在 `main.c` 里找到 `process_command` 的 `exit` 分支（方法同第 16.0 节）。

**第 2 步**：在 `exit` 分支前面插入：

```c
    } else if (cmd_is("add")) {
        // add <数字1> <数字2>：两数相加并显示结果
        char* p = skip_spaces(input_buf + 4);   // ① 跳到第一个参数
        char* a_s = p;                          // ② 参数1 = 从这里开始
        while (*p && *p != ' ') p++;            // ③ 走到第一个空格
        if (*p == '\0') {                       // ④ 没有空格 = 只有1个参数
            terminal_write("usage: add <num1> <num2>\n");
        } else {
            *p = '\0';                          // ⑤ 空格改成结束符
            char* b_s = skip_spaces(p + 1);     // ⑥ 跳到参数2
            if (*b_s == '\0') {                 // ⑦ 参数2是空的
                terminal_write("usage: add <num1> <num2>\n");
            } else {
                int sum = atoi(a_s) + atoi(b_s);  // ⑧ 两个数相加
                char num[12];
                itoa_dec(sum, num);               // ⑨ 结果转成字符串
                terminal_write(num);
                terminal_putchar('\n');           // ⑩ 换行
            }
        }
    }
```

**这一段是"带参数命令"的万能模板**，逐行理解它，以后所有带参数命令都是它的变体：

| 行 | 代码 | 它在干什么 |
|---|---|---|
| ① | `char* p = skip_spaces(input_buf + 4);` | 让 `p` 指向第一个参数的开头。`input_buf + 4` 直接指到 `'3'`（`+3` 会指到 `'3'` 前面的空格，`skip_spaces` 也会跳过去，效果相同）。**推荐写法见下面的⚠️说明**：`input_buf + strlen("add")` 更保险 |
| ② | `char* a_s = p;` | 记住参数 1 的开始位置。此时 `a_s` 指向 `'3'` |
| ③ | `while (*p && *p != ' ') p++;` | 让 `p` 往前走，直到遇到**结束符 `'\0'`**（说明后面没参数了）或**空格**（说明参数 1 结束）。循环结束时有两种情况：`*p` 是 `'\0'` 或 `' '` |
| ④ | `if (*p == '\0')` | 如果是结束符：说明 `add 3` 后面没有第二个参数。提示用法并结束 |
| ⑤ | `*p = '\0';` | 把空格**改成结束符**（第 17.0 节讲的"切分"技巧）。现在 `a_s` 指向的字符串是 `"3"` |
| ⑥ | `char* b_s = skip_spaces(p + 1);` | `p + 1` 是空格后面，跳过可能多余的空格，指向参数 2 |
| ⑦ | `if (*b_s == '\0')` | 参数 2 是空的（比如输入 `add 3 `，末尾只有空格）→ 提示用法 |
| ⑧ | `int sum = atoi(a_s) + atoi(b_s);` | 把两个参数字符串转成数字并相加 |
| ⑨ | `char num[12]; itoa_dec(sum, num);` | 数字不能直接输出，`itoa_dec` 把数字转成字符串存进 `num`。**`num[12]` 的 12 是固定要求**（最多 10 位数字 + 结束符，留余量） |
| ⑩ | `terminal_write(num); terminal_putchar('\n');` | 输出结果，换行 |

**⚠️ 关于 `input_buf + 4` 的解释**：`add` 是 3 个字符，但偏移写的是 4——因为 `skip_spaces(input_buf + 3)` 会从 `'d'` 后面的空格开始，`skip_spaces` 内部正好把它跳过去，效果一样。**但更保险、更不易错的写法是**：

```c
char* p = skip_spaces(input_buf + strlen("add"));
```

`strlen("add")` 自动算出 3，`+ 3` 正好指向空格，再 `skip_spaces` 跳过。**以后写命令一律用 `strlen("命令名")`，别自己数**。

**第 3 步**：保存 `main.c`，执行 `make run`。

**第 4 步**：测试：

```
os/> add 3 5
8
os/> add 100 23
123
os/> add 3
usage: add <num1> <num2>
os/> add 3 x
3          ← 注意：atoi("x") 返回 0，所以 3+0=3。这是 atoi 的"宽容"行为
os/> add
usage: add <num1> <num2>
```

**第 5 步**：通过后存档：

```bash
git add main.c fs/str.c fs/str.h
git commit -m "add 'add' command with two number args"
```

### 17.3 参数带空格怎么办：extract_quoted（引号提取）

`add` 的参数是数字，不需要引号。但有些命令的参数是**文字**（比如 `create note "hello world"`），文字里有空格——空格会被当作分隔符切掉，怎么办？

答案是**双引号**：`"hello world"` 整体算一个参数。负责这件事的是 `extract_quoted` 函数（main.c 里现成的，直接用）：

```c
static char* extract_quoted(char** pp) {
    char* p = skip_spaces(*pp);
    if (*p == '"') {
        p++;
        char* start = p;
        while (*p && *p != '"') p++;
        if (*p != '"') return NULL;   // 未闭合引号
        *p = '\0';
        *pp = p + 1;
        return start;
    }
    *pp = p;
    return p;
}
```

**不用完全读懂它，你只需要知道**：

- 用法：`char* arg = extract_quoted(&p);` —— 其中 `p` 是"下一个参数的位置"（指针）
- 返回：参数内容的开头（引号内的文字）
- 如果返回 `NULL`：说明引号没有闭合（输入了 `"abc` 没有后面的 `"`）
- 调用后，`p` 自动指向参数**后面**的位置（方便继续取下一个参数）

**官方示例**（现有代码里 `echo` 分支的用法，可以直接抄）：

```c
} else if (cmd_is("echo")) {
    char* p = input_buf + 4;
    char* arg = extract_quoted(&p);
    if (arg == NULL) {
        terminal_write("unclosed quote\n");
    } else {
        terminal_write(arg);
        terminal_putchar('\n');
    }
}
```

想给 `add` 加个"输出文字"的第二个命令？那就是 `echo` 的翻版。**凡是"要接收一段可能含空格的文字"的参数，就用 `extract_quoted`**。

### 17.4 带参数命令的套路总结（背下来）

以后加任何带参数命令，按这五步走：

```
第 1 步：char* p = skip_spaces(input_buf + strlen("命令名"));
        → 定位到第一个参数

第 2 步：char* 参数变量 = p;
        然后 while (*p && *p != ' ') p++;   // 走到参数分隔处
        （或直接 char* 参数 = extract_quoted(&p);  如果需要引号支持）

第 3 步：if (*p == '\0') { 提示 usage; } else { ... }
        → 参数不足时提示用法（usage 文案格式：usage: 命令名 <参数1> <参数2>）

第 4 步：*p = '\0';  把分隔符改成结束符，参数 1 变成独立字符串
        然后继续用同样的方法取参数 2、参数 3……

第 5 步：用 atoi / strcmp / fs_* 等处理参数，输出结果
```

**两个常犯错误**（加参数命令时最容易踩）：

| 错误 | 现象 | 修复 |
|---|---|---|
| 偏移写错：`input_buf + 5` 而命令名是 6 个字符 | 参数解析全乱，行为诡异 | 改用 `input_buf + strlen("命令名")` |
| 忘了 `if (*p == '\0')` 检查 | 输入参数不足时程序崩溃或行为异常 | 每次切参数后都检查"后面还有内容吗"，没有就提示 usage |

## 18. 修改文件系统限制（容量、名字长度、内容长度）

**目标**：调整文件系统容量（比如允许 64 个文件、文件名最长 40 字符、内容最长 512 字节）。

**修改文件**：`fs/fs.h`（三个宏）

**步骤**：

1. 打开 `fs/fs.h`，修改：

```c
#define FS_MAX_FILES     64    /* 32 → 64：文件数上限 */
#define FS_MAX_NAME_LEN  40    /* 32 → 40：文件名缓冲（实际最长 39 字符） */
#define FS_MAX_CONTENT   512   /* 256 → 512：内容上限 */
```

2. 保存，`make run`。

**原理**：所有相关的缓冲区（`file_t` 里的数组）、所有校验逻辑（`fs_add` 里的比较）都引用这三个宏，改一处全局生效。文件表总内存大约变成 64 × (40+513+2+1) ≈ 35KB，对内核无压力。

**注意**：

- `FS_MAX_NAME_LEN` 含 `'\0'`，所以"最长 40"实际是 39 个字符。错误消息里写着 "max 31 chars"，如果你改了长度，记得同步改 `main.c` 里 `fs_err_str` 的文案（第 22 节）。
- 调用 `fs_read` 时的缓冲区要跟着改：`char buf[FS_MAX_CONTENT + 1];`（用宏而不是写死数字，这样改宏时不用改调用处）。
- **把 `FS_MAX_FILES` 改小**（比如 8）会让现有文件表容纳更少文件，已创建的"多余"文件会变不可见——这是可预期的行为，测试时先 `make clean` 从零开始。

## 19. 给文件系统加一个新接口（示例：文件改名 rename）

**目标**：新增 `fs_rename(oldname, newname)`，实现文件改名，并加命令 `rename <旧名> <新名>`。

**涉及文件**：`fs/fs.h`、`fs/fs.c`、`main.c`

**步骤**：

**① 在 fs.h 声明新函数**（放在现有声明后面）：

```c
fs_status_t fs_create(const char* name, const char* content);
fs_status_t fs_read(const char* name, char* out, unsigned int maxlen);
fs_status_t fs_delete(const char* name);
fs_status_t fs_rename(const char* oldname, const char* newname);   // ← 新增
void fs_list(fs_out_fn out);
```

**② 在 fs.c 实现它**（放在 `fs_delete` 后面，用已有的内部函数）：

```c
fs_status_t fs_rename(const char* oldname, const char* newname) {
    int i;

    if (newname[0] == '\0') return FS_EMPTY_NAME;        // 新名为空
    if (strlen(newname) >= FS_MAX_NAME_LEN) return FS_NAME_TOO_LONG;
    if (fs_find(newname) >= 0) return FS_EXISTS;         // 新名已存在
    i = fs_find(oldname);                                 // 旧名必须存在
    if (i < 0) return FS_NOT_FOUND;

    strcpy(files[i].name, newname);                       // 只改名字
    return FS_OK;
}
```

**注意**：这里**复用了 `fs_find` 和 `strcpy`**，没有重复造轮子——这是项目里加新功能的正确姿势：先看看内部已有的函数能不能复用。

**③ 在 main.c 加命令分支**（参考第 17 节的参数解析模板）：

```c
    } else if (cmd_is("rename")) {
        // rename <旧名> <新名>
        char* p = skip_spaces(input_buf + strlen("rename"));
        char* oldname = p;
        while (*p && *p != ' ') p++;          // 切出旧名
        if (*p == '\0') {
            terminal_write("usage: rename <old> <new>\n");
        } else {
            *p = '\0';
            char* newname = skip_spaces(p + 1);
            if (*newname == '\0') {
                terminal_write("usage: rename <old> <new>\n");
            } else {
                fs_status_t st = fs_rename(oldname, newname);
                if (st != FS_OK) terminal_write(fs_err_str(st));
            }
        }
    }
```

**④ 验证**：

```
os/> create a hello
os/> rename a b
os/> ls
b  5
os/> rename b a
file already exists
```

**设计要点**：新函数遵循了与既有接口一致的规范——返回 `fs_status_t` 错误码、错误文案集中在 main.c 的 `fs_err_str` 里。**保持这个风格**，文件系统才会越来越好扩展。

## 20. 新增一个 C 源文件（不需要改 Makefile）

**目标**：新建一个模块 `timer.c/h`（比如放一个延时函数），让 main.c 能调用它。

**好消息**：Makefile 已升级为**自动扫描**（第 12 节），**新加的 `.c` 文件会被自动纳入编译**——本节的"接入构建"这一步，现在什么都不用做。

**涉及文件**：只需新建两个文件 + 在 main.c 里使用它。**Makefile 不用动。**

**① 创建头文件和源文件**。放在哪个目录？**看模块性质**：

| 模块性质 | 放哪 | 例子 |
|---|---|---|
| 文件系统相关 | `fs/` | `fs.c`、`str.c` 所在 |
| 电源/硬件相关 | `acpi/` | `acpi.c` 所在 |
| 内核杂项 | 根目录（main.c 旁边） | 下面这个例子 |

示例（放根目录）：

```c
/* timer.h */
#ifndef TIMER_H
#define TIMER_H

void timer_wait(unsigned int ticks);   /* 空转等待 ticks 次（演示用） */

#endif
```

```c
/* timer.c */
#include "timer.h"

void timer_wait(unsigned int ticks) {
    volatile unsigned long i;
    for (volatile unsigned long n = 0; n < ticks; n++) {
        for (i = 0; i < 1000000; i++) ;   /* 空转消耗时间 */
    }
}
```

**⚠️ 头文件保护宏**（`#ifndef TIMER_H ... #endif`）是**必须的**——防止同一个头文件被多个 .c 文件包含时重复定义报错。每个新头文件都要有，宏名用文件名大写。

**⚠️ 目录放哪的硬性限制**：Makefile 只扫描**根目录、`acpi/`、`fs/`、`kernel/`** 四个位置（第 12.1 节）。放进这四个位置之外的目录（比如自己新建 `lib/`）就**不会**被自动编译——那种情况才需要按第 12.4 节改 Makefile。

**② 在 main.c 使用它**：

```c
#include "timer.h"        // main.c 顶部加

// kmain 里（示例）：
terminal_write("waiting...\n");
timer_wait(5);            // 等一下
terminal_write("done\n");
```

**③ 保存，`make run` 验证**：

- 如果编译通过、QEMU 里显示 `waiting...` 后延迟一下再显示 `done`，说明接入成功——**你什么都没配置，make 自动找到了 timer.c**
- 想确认它真的被编译了：看 `make` 输出里有 `gcc ... -c -o bin/timer.o timer.c` 这一行

**④ 通过后存档**：

```bash
git add timer.c timer.h main.c
git commit -m "add timer module"
```

**常见错误**：

| 现象 | 原因 |
|---|---|
| `fatal error: timer.h: No such file or directory` | `#include "timer.h"` 找不到头文件。头文件要放在与 .c 相同的位置（根目录放根目录、fs/ 放 fs/）；如果放进了**新目录**，需要在 Makefile 的 `CFLAGS` 加 `-I目录名`（第 12.4 节） |
| `undefined reference to 'timer_wait'` | 函数名拼写不一致（声明和实现/调用不一致），或者**文件放在了 Makefile 扫描不到的目录**（第 12.1 节） |
| `multiple definition of 'timer_wait'` | 你把函数实现写进了 `.h` 文件（头文件被多个 .c include 后重复定义）。**实现写 .c，声明写 .h** |

## 21. 修改错误消息文案

**目标**：改文件系统的报错文字（比如把 `file already exists` 改成 `file exists, choose another name`）。

**修改文件**：`main.c`（`fs_err_str` 函数）

**步骤**：

```c
static const char* fs_err_str(fs_status_t s) {
    switch (s) {
    case FS_OK:            return "";
    case FS_EXISTS:        return "file already exists\n";        // ← 改这里
    case FS_NOT_FOUND:     return "file not found\n";
    case FS_FULL:          return "file table full\n";
    case FS_EMPTY_NAME:    return "empty name\n";
    case FS_NAME_TOO_LONG: return "name too long (max 31 chars)\n";// ← 注意同步限制数字
    case FS_BAD_CONTENT:   return "content too long (max 256 bytes)\n";
    default:               return "unknown fs error\n";
    }
}
```

**要点**：

- 文案必须以 `\n` 结尾（否则会紧接着打提示符，看起来像粘在一起）
- 如果第 18 节改了长度限制，这里括号里的数字要同步改
- 加了新错误码（第 9.2 节），这里必须加对应 `case`，否则新错误会落入 `default` 显示 "unknown fs error"

## 22. 修改命令解析的边界行为（进阶）

**目标**：让 `cat` 支持引号文件名（现在 `cat foo bar` 会把整行当文件名）。

**涉及文件**：`main.c`（`process_command` 的 cat 分支）

**当前实现**：

```c
} else if (cmd_is("cat")) {
    char* p = skip_spaces(input_buf + 3);
    if (*p == '\0') {
        terminal_write("usage: cat <name>\n");
    } else {
        char buf[FS_MAX_CONTENT + 1];
        fs_status_t st = fs_read(p, buf, sizeof(buf));   // p 是整行剩余部分
        ...
```

**改成"只取第一个参数"**：

```c
} else if (cmd_is("cat")) {
    char* p = skip_spaces(input_buf + strlen("cat"));
    char* name = p;
    while (*p && *p != ' ') p++;        // 在第一个空格处截断
    *p = '\0';
    if (*name == '\0') {
        terminal_write("usage: cat <name>\n");
    } else {
        char buf[FS_MAX_CONTENT + 1];
        fs_status_t st = fs_read(name, buf, sizeof(buf));
        ...
```

改完后 `cat foo bar` 会读取 `foo`（忽略多余的 `bar`）。**注意**：这个改动改变了既有的"安全行为"（整行作为文件名找不到会报 not found），属于行为变更，改之前想清楚是否真的需要。**"刻意设计的安全边界"和"bug"之间的区别**，见第 30 节。

# 第五部分　调试与故障排查

> 维护工作绕不开调试。这一部分先教你**看懂编译错误**（最常见的场景），再教你**运行时出问题怎么定位**，最后介绍 QEMU 自带的一些调试手段。

## 23. 编译错误对照表（遇到报错先查这里）

编译器报错是**最友好**的报错——它精确告诉你哪个文件哪一行出了什么问题。看不懂英文不要紧，对照下表查含义：

### 23.1 语法类错误（最常见，几乎都是打错字）

| 报错信息（片段） | 含义 | 常见原因与解决 |
|---|---|---|
| `error: expected ';' before ...` | 缺分号 | 上一行结尾忘了 `;`。**英文报错里的行号（`main.c:12:5`）指的就是出错行**，直接去那行附近看 |
| `error: expected '}' ...` | 缺右花括号 | `{}` 没配对。数一数你新加的代码块的括号 |
| `error: 'xxx' undeclared` | 变量/函数未声明 | ① 变量名打错了（`input_buf` 写成 `inputbuf`）；② 忘了 `#include` 对应头文件；③ 函数写错文件名/忘了声明 |
| `warning: implicit declaration of function 'xxx'` | 函数没有声明就使用 | 没 `#include` 头文件，或者函数名拼错。**warning 也要修**，它往往预示着隐藏 bug |
| `error: too few/many arguments to function 'xxx'` | 函数参数个数不对 | 调用的函数签名和你写的不一致。去头文件里查函数原型（第 9.3 节的接口表就是干这个的） |
| `error: conflicting types for 'xxx'` | 类型冲突 | 同一个函数被声明/定义了两次且签名不同。检查是否重复定义 |

### 23.2 编译选项引发的"假错误"

下面这些报错**不是你的代码逻辑问题**，而是裸机环境特有的约束：

| 报错信息（片段） | 含义 | 怎么办 |
|---|---|---|
| `undefined reference to 'printf'` | 调用了不存在的 printf | **内核没有 printf**。改用 `terminal_write`（字符串）或第 33 节自制的 printf |
| `undefined reference to 'memcpy'` / `'strlen'` / `'malloc'` 等 | 调用了 libc 函数 | 内核没有标准库。`strlen/strcmp/strcpy` 用 fs/str.c 里的；其他函数要自己写 |
| `undefined reference to '__stack_chk_fail'` | 栈保护 | 你写的代码让编译器想插栈保护。正常我们加了 `-fno-stack-protector`，如果出现说明数组写越界等可疑操作，检查代码 |
| `warning: 'xxx' defined but not used` | 定义了没使用 | 你加的 static 函数没被调用。要么调用它，要么删掉 |

### 23.3 链接错误

编译阶段通过、链接（`ld`）阶段失败：

| 报错 | 含义 | 解决 |
|---|---|---|
| `undefined reference to 'foo'` | 某个函数调用了但没实现 | ① 函数写错了名字；② 文件放在了 Makefile 扫描不到的目录（自动扫描只覆盖根目录/acpi/fs/kernel，见第 12.1 节）；③ 函数是 `static` 写在别的文件里（static 函数只能本文件用） |
| `multiple definition of 'foo'` | 同一个函数定义了两遍 | 在多个 .c 文件里定义了同名函数。检查是否不小心复制粘贴了函数体 |
| `cannot find linker script: kernel/linker.ld` | 找不到链接脚本 | 目录结构被改动了？确认 `kernel/linker.ld` 存在 |

### 23.4 排查步骤模板

遇到编译错误，按这个顺序做：

```
① 看第一个 error（不是 warning，不是后面的 error——第一个往往是最根本的）
② 去它指出的文件:行号看代码
③ 对照 23.1/23.2/23.3 的表格找同类错误
④ 改完再 make（错误会减少，因为编译器经常因第一个错误"连锁报错"）
⑤ 直到 0 error
```

**常见误区**：报错一大堆时从**最后一个**开始看。错的——要从**第一个**开始看，后面的多半是"被第一个错误连累"产生的连锁报错。

## 24. 运行时问题排查（QEMU 里出问题了怎么办）

编译通过不代表运行没问题。运行时的问题按"严重程度"分为几类：

### 24.1 QEMU 窗口一闪而过 / 系统反复重启

**症状**：QEMU 打开后马上又关了，或者循环重启。

**原因**：内核在启动早期崩溃（比如汇编/链接脚本被改过、或者 kmain 里第一行就死掉）。电脑崩溃后会自动重启（QEMU 没有 `-no-reboot` 时），看起来就是"反复重启"。

**排查**：

1. 检查你有没有动过 `kernel/` 下的文件——动了就先恢复（git 回退，见第 28 节）
2. 检查 `kmain` 里开头的代码（`fs_init`、`terminal_clear` 前面几行）
3. 用 QEMU 的调试模式看崩溃现场（第 26 节）

### 24.2 屏幕有输出但系统无响应（按键无效）

**症状**：欢迎语正常显示，但按键盘没反应。

**原因**（由轻到重排查）：

1. **屏幕写满了**——当前内核不滚动，第 25 行写满后光标被夹住，新输出覆盖最后一行，看起来"没动静"。**滚动 QEMU 屏幕或检查最后一行**。这不是 bug。
2. **死循环**——你新加的命令里有个 `while` 循环条件永远为真（比如忘了改循环变量）。`kmain` 的主循环卡在你的死循环里，键盘自然没反应。**检查你新加的循环代码**。
3. **键盘初始化被破坏**——`handle_keyboard` 被你改坏了。

**急救**：`Ctrl+C` 关掉 QEMU → 回退代码 → 重新来。

### 24.3 某个命令行为不对（但系统活着）

**症状**：系统正常，只有某个命令的输出不对。

**排查思路**（按顺序）：

```
① 确认输入确实对了：命令名有没有被 cmd_is 误匹配？（比如"about"匹配了"aboutx"？不会——cmd_is 有边界检查）
② 确认参数解析对不对：用 echo 命令打印输入，确认分隔符位置
③ 确认函数返回值：临时在代码里打印错误码（用 itoa_dec）
④ 确认文件系统状态：用 ls 看文件表里有什么
```

**给排查加上"临时日志"**：在可疑的代码位置临时加：

```c
terminal_write("debug: reaching here, code=");
char num[12];
itoa_dec(st, num);          // 把错误码数字打印出来
terminal_write(num);
terminal_putchar('\n');
```

确认问题后**删除这些调试输出**，不留垃圾代码。

### 24.4 关机不生效（exit 没反应）

**原因**：ACPI 关机依赖 `acpi/acpi.c`。如果它失效（比如 QEMU 参数问题），`exit` 会打印 "Shutting down..." 后卡住。

**排查**：

1. 用 `make run`（`-kernel` 方式）测试。如果 `-kernel` 方式正常、`run-iso` 不正常，是 ACPI 表在不同启动方式下的差异。
2. 实在不行，临时把 `exit` 分支改成死循环 `while(1);`（至少不继续执行）。**这属于临时应急，不是修复**。

## 25. 使用 QEMU 的调试能力（不加代码也能观察系统）

QEMU 自带调试能力，**不需要修改内核代码**就能观察内部状态。这在定位"屏幕显示正确但内部数据不对"的问题时非常有用。

### 25.1 启动带监控台的 QEMU

```bash
# 给 QEMU 加两个参数：
#   -monitor unix:/tmp/mon.sock,server,nowait  —— 开启"监控台"（用 unix socket 连接）
#   -qmp unix:/tmp/qmp.sock,server,nowait      —— 开启更底层的 QMP 接口
#   -display none                              —— 无窗口（纯调试用）
qemu-system-x86_64 -kernel bin/kernel.elf -display none \
  -monitor unix:/tmp/mon.sock,server,nowait \
  -qmp unix:/tmp/qmp.sock,server,nowait
```

然后可以用 `socat` 或 `nc` 连接监控台：

```bash
socat - UNIX-CONNECT:/tmp/mon.sock     # 进入交互式监控台
```

### 25.2 监控台常用命令

| 命令 | 作用 | 示例 |
|---|---|---|
| `pmemsave 地址 大小 文件` | 把客户机物理内存保存到宿主机文件 | `pmemsave 753664 4000 screen.bin` |
| `sendkey 键名` | 模拟按键 | `sendkey a`、`sendkey ret`、`sendkey spc` |
| `info registers` | 查看 CPU 寄存器 | 调试汇编/崩溃用 |
| `quit` | 退出 QEMU | |

**屏幕内容导出**：屏幕显存地址是 `0xB8000`（十进制 `753664`），大小 `4000` 字节。执行：

```
(qemu) pmemsave 753664 4000 screen.bin
```

然后在宿主机解析（每 2 字节一个字符）：

```bash
python3 -c "
data = open('screen.bin','rb').read(4000)
for r in range(25):
    line = ''.join(chr(data[i]) if data[i] else ' ' for i in range(r*160,(r+1)*160,2)).rstrip()
    if line.strip(): print(line)
"
```

**⚠️ 两个实测的坑**（QEMU 11 版本）：

1. `pmemsave` 的文件路径**必须是相对路径**（不能用 `/tmp/xxx.bin` 这种绝对路径，会报 `invalid char` 错误）。直接写 `screen.bin`，文件生成在启动 QEMU 时的目录。
2. `sendkey quot` 是**非法键名**（会报 `invalid parameter: quot`）。引号键的正确键名是 `apostrophe`；要输入双引号 `"`，用 `sendkey shift-apostrophe`。

### 25.3 模拟按键（自动化测试）

`sendkey` 可以模拟键盘输入，配合 `pmemsave` 可以实现"无人值守测试"（本项目的自动化回归测试就是这么做的）。一个最小的自动化测试脚本示例：

```bash
# 用 python 连接监控台并发送命令（先按上面方式启动 QEMU）
python3 - <<'PYEOF'
import socket, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.connect('/tmp/mon.sock')
def cmd(c):
    s.sendall((c + '\n').encode()); time.sleep(0.1)
for k in 'l','s','ret':           # 输入 "ls" 并回车
    cmd('sendkey ' + k)
cmd('pmemsave 753664 4000 screen.bin')
cmd('quit')
s.close()
PYEOF
```

**注意**：`sendkey` 在快速连续注入时**可能丢键**（QEMU 的键盘队列限制）。实测可靠的注入方式是 QMP 接口（见 25.4）。如果你只是手动测试，一次一个键慢慢敲没问题。

### 25.4 QMP 接口（可靠的按键注入）

QMP 是比监控台更底层、更可靠的接口。连接后先握手：

```python
import socket, time, json
q = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
q.connect('/tmp/qmp.sock')
q.sendall((json.dumps({"execute": "qmp_capabilities"}) + '\n').encode())
time.sleep(0.3)

def press(name):    # 按下
    q.sendall((json.dumps({"execute": "input-send-event", "arguments": {"events": [
        {"type": "key", "data": {"down": True,  "key": {"type": "qcode", "data": name}}}]}}) + '\n').encode())
def release(name):  # 松开
    q.sendall((json.dumps({"execute": "input-send-event", "arguments": {"events": [
        {"type": "key", "data": {"down": False, "key": {"type": "qcode", "data": name}}}]}}) + '\n').encode())
def tap(name):      # 完整一次按键
    press(name); time.sleep(0.2); release(name); time.sleep(0.2)
```

按键名（qcode）：字母就是本身（`a`、`b`……），空格 `spc`、回车 `ret`、引号 `apostrophe`、Shift `shift`。双引号 = `shift` + `apostrophe` 组合。**每个按键都要成对 press+release**，这也是它比 sendkey 可靠的原因。

## 26. 用 gdb 调试内核（可选进阶）

如果你会一点 gdb，可以在 QEMU 里打断点、看变量——**比加日志优雅得多**。

```bash
# 终端 1：启动带 gdb 接口的 QEMU（-s 开启 1234 端口调试）
qemu-system-x86_64 -kernel bin/kernel.elf -s -S -display none

# 终端 2：启动 gdb
gdb bin/kernel.elf
(gdb) target remote :1234      # 连上
(gdb) b kmain                  # 在 kmain 打断点
(gdb) c                        # 继续运行
(gdb) p input_len              # 查看变量
(gdb) x/10c input_buf          # 查看输入缓冲
```

**注意**：gdb 调试需要符号信息，当前 `-Wall -Wextra` 编译默认带符号（没有 -g 也行，`-g` 更完整）。如果以后发现 gdb 看不到变量，在 Makefile 的 `CFLAGS` 里加一个 `-g` 即可。

**不会 gdb 也没关系**——本手册的日志调试法（24.3 节）完全够用。

## 27. 常见问题清单（FAQ）

| 问题 | 答案 |
|---|---|
| 编译时 `main.c` 报 unused parameter 警告 | 正常的。`kmain(magic, addr)` 的两个参数目前没用，编译器提醒而已，**不要管** |
| 屏幕上显示乱码/方块 | 你输出中文了。VGA 文本模式只支持 ASCII |
| `make run` 打开了两个 QEMU | 上次的 QEMU 没关掉。关掉旧的再试（或 `pkill -x qemu-system-x86_64`） |
| 屏幕显示到一半不动了 | 写满 25 行了，当前内核不滚动（第 8.2 节）。不是 bug |
| 我改了 fs.h 但 make 没重新编译 | 理论上不会发生：gcc 的 `-MMD` 自动依赖（第 12.3 节）会让 make 自动重编所有 include 了 fs.h 的 .c。如果真没反应，先 `cat bin/main.d` 确认依赖文件里有没有 fs.h，或 `make clean` 全量重编 |
| 修改后 make 报 `nothing to be done` | 你改了文件但时间戳比产物旧？检查是否真的保存了；或者 `make clean` 强制重编 |
| `grub-mkrescue` 报错 | 多半是缺工具（xorriso/mtools）。重装依赖（第 3.1 节） |
| QEMU 报 `Could not open ...` | `-kernel` 路径不对。确认你在 `MaxZOS` 目录下运行，产物是 `bin/kernel.elf` |
| 怎么彻底删除所有产物 | `make clean`（删 bin/）。想连 bin/ 都不存在：`rm -rf bin` |

---

# 第六部分　版本控制（git）

> 这一部分只讲**维护者日常够用的最小 git 操作**。目标：改代码前能存档、改坏了能回退、能看历史。所有命令都在 `MaxZOS` 目录下执行。

## 28. git 基础操作

### 28.1 先理解三个概念

```
工作区（你正在编辑的文件）
   ↓ git add
暂存区（准备好要提交的改动清单）
   ↓ git commit
仓库（永久存档的历史）
```

- **工作区**：磁盘上的真实文件，你改的就是它们
- **暂存区**：你告诉 git"这些改动我要存档"
- **仓库**：git 保存的所有历史版本

### 28.2 日常操作清单

| 场景 | 命令 |
|---|---|
| 看看项目现在什么状态 | `git status` |
| 看看改了哪些文件的内容 | `git diff` |
| 把改动加入暂存区（准备存档） | `git add 文件名`（或 `git add .` 全部） |
| 存档（生成一个版本） | `git commit -m "说明这次改了什么"` |
| 查看历史版本 | `git log --oneline` |
| 丢弃**未提交**的改动（回到上次存档） | `git restore 文件名` |
| 放弃全部未提交改动 | `git restore .`（**危险**：未提交的改动全没） |
| 把改动推到远程仓库 | `git push`（如果有远程） |
| 拉取远程最新代码 | `git pull` |

### 28.3 推荐的工作习惯（强烈建议遵守）

```
① 拿到代码后第一件事：git status 确认状态干净
② 每次开始改代码前：确保最近的改动已 commit（"能跑的状态"已存档）
③ 一次改动一个功能 → make 测试 → 通过后 commit
④ commit 信息写清楚：比如 "add rename command" 而不是 "update"
⑤ 改坏了：git restore 回到存档状态，从头再来（而不是继续瞎改）
```

**为什么要这样**：这个项目将来只有你一个人维护（或加一个维护者），**没有 git 历史的话，你改坏了只能靠记忆恢复**。有了上面的习惯，任何时刻都能回到"上一个能跑的状态"。

### 28.4 版本回退操作

```bash
git log --oneline        # 找到你想回到的那个版本的编号（前面那串字母数字）
git checkout <编号>      # 临时切到那个版本查看（工作区会变成当时的样子）
git checkout main        # 回到最新状态
```

**回退（放弃某次提交之后的改动）**：

```bash
git reset --hard <编号>   # ⚠️ 危险操作！这个编号之后的提交会消失
```

**⚠️ 红线**：`git reset --hard` 会**永久删除**之后的提交。除非你确定，否则只用 `git restore` 处理"未提交"的改动，用 `git checkout` 查看历史。

### 28.5 提交信息写作规范

一句话说明"改了什么、为什么"。参考模板：

```
add rename command for files     ← 加了个功能
fix: cat shows wrong size        ← 修了个 bug（前缀 fix: 表示修复）
change prompt to MaxZOS$         ← 改了个配置
```

**注意**：提交信息不要用中文乱码符号，简洁英文即可（或简短中文也行，保持风格统一）。

# 第七部分　红线、约定与扩展

## 29. 红线清单（绝对不能做的事）

这一节是**最重要的安全须知**。以下行为会导致系统无法启动或无法定位问题，**一律禁止**：

| # | 红线 | 为什么 |
|---|---|---|
| 1 | 不要修改 `kernel/` 下的任何文件 | 汇编与链接脚本是"起跑"逻辑，改错一行系统直接起不来，且你无法调试 |
| 2 | 不要调用 libc 函数（`printf`、`malloc`、`memcpy`、`strlen`……） | 内核没有标准库，链接时直接报 `undefined reference`。字符串用 `fs/str.c`，数字转字符串用 `itoa_dec` |
| 3 | 不要使用 `malloc`/`free` 或任何动态内存 | 没有堆管理器。所有数据用静态数组（`static ... [N]`）或栈上的数组 |
| 4 | 不要在头文件里定义变量或函数体（只能声明） | 头文件会被多个 .c 包含，定义会导致 `multiple definition` 链接错误。头文件里只能有：宏、类型定义（typedef）、函数声明、`extern` |
| 5 | 不要写无限递归或很深的递归 | 内核栈只有 16KB，递归太深直接栈溢出崩溃 |
| 6 | 不要使用浮点数（`float`/`double`） | 内核没有浮点环境，用了行为未定义。用整数 |
| 7 | 不要用 `int` 表示"一定很大"的数 | 这是 32 位环境，`int` 最大约 21 亿。文件大小等用 `unsigned int` 或 `unsigned short` |
| 8 | 不要输出中文到屏幕 | VGA 文本模式只支持 ASCII，中文显示为乱码（第 14 节） |
| 9 | 不要动 `bin/` 目录里的东西 | 全部自动生成 |
| 10 | 不要一次改很多文件后一次测试 | 保持"一次一改一测"，否则出了问题无从定位 |

**如果违反红线的惩罚**：轻则编译报错（好办），重则系统起不来、调试数小时（不好办）。**心里默念：静态数组、无标准库、一次一改**。

## 30. 为什么会有这些约束（理解深层原因）

如果你好奇为什么这个项目"这么别扭"，下面用大白话解释三个核心约束。理解了它们，你写代码时就不容易踩坑。

### 30.1 为什么没有 printf 和标准库

普通程序运行在操作系统里，`printf` 最终会调用操作系统的"写屏幕"服务。**MaxZOS 自己就是操作系统**——没人给它提供服务，所以一切自给自足：

- 屏幕输出：自己写显卡内存（`terminal_*` 函数）
- 字符串函数：自己实现（`fs/str.c`）
- 数字格式化：自己实现（`itoa_dec`）

**这是"从零写操作系统"的本质**：每一步都要自己造轮子。

### 30.2 为什么不用 malloc（动态内存）

`malloc` 需要一段"堆"内存和一套分配算法。给内核加堆不是不行，但**复杂度远超收益**：

- 需要知道"哪些物理内存是空闲的"（要解析 multiboot 信息）
- 需要分配算法（空闲链表等）
- 需要处理碎片、泄漏问题

当前项目规模小，**静态数组完全够用**，而且"上限固定"本身就是一种可预期的安全。等文件系统需要"动态数量的文件"时，再考虑堆（第 34 节路线图）。

### 30.3 为什么没有中断（键盘用轮询）

中断是"硬件主动通知 CPU"的机制，实现它需要：IDT（中断描述符表）、中断处理函数、优先级管理……工程量不小。

当前方案是**轮询**：主循环 `while(1)` 里不停地问键盘"有按键吗？"。简单、可靠、无并发问题。缺点是一个死循环里所有事都是顺序的——但当前系统也只有这一件事，完全够用。

**维护含义**：你的代码都是"单线程、顺序执行"的，**不需要考虑并发、锁、竞态**。这在操作系统开发里是非常奢侈的简单。

## 31. 代码风格约定（让项目保持整洁）

维护者不止一个人时，风格统一很重要。这个项目目前遵守的约定：

| 项目 | 约定 | 例子 |
|---|---|---|
| 缩进 | 4 个空格（不是 Tab） | 看 main.c 的缩进 |
| 函数命名 | 小写 + 下划线，模块前缀 | `fs_create`、`terminal_write`、`cmd_is` |
| 宏命名 | 全大写 + 下划线 | `FS_MAX_FILES`、`VGA_ATTR` |
| 文件内部函数 | 加 `static`（只在本文件用） | main.c 里除 `kmain` 外全是 static |
| 返回值 | 用枚举错误码（fs 模块） | `fs_status_t` |
| 注释 | 中文注释，解释"为什么" | 看现有注释的风格 |
| 字符串常量 | 用 `const char*` 或字符串字面量 | `terminal_write("hello")` |

**新函数必带注释**：说明函数做什么、参数含义、返回值含义。维护者（包括未来的你）靠注释理解代码。

**加函数的位置约定**：模块相关函数加在对应模块文件；main.c 里的辅助函数按功能加在 `process_command` 前后。

## 32. 手动回归测试清单

**每次改动后**，建议在 QEMU 里跑一遍这个清单（约 2 分钟），确认没有破坏已有功能：

### 32.1 基础命令

```
os/> echo hello            → 期望输出：hello
os/> echo "a b"            → 期望输出：a b（引号被剥离）
os/> echo "a b             → 期望输出：unclosed quote
os/> clear                 → 期望：屏幕清空，提示符回到左上角
os/> cls                   → 期望：同上
os/> clearxxx              → 期望：unknown command（边界匹配生效）
os/> exit                  → 期望：打印 Shutting down... 然后 QEMU 关闭
```

### 32.2 文件系统

```
os/> ls                    → 期望：无输出（空文件表）
os/> create a hello        → 期望：无输出（成功）
os/> create b "hello world"→ 期望：无输出（成功，内容含空格）
os/> ls                    → 期望：
a  5
b  11
os/> cat a                 → 期望：hello
os/> cat b                 → 期望：hello world
os/> create a again         → 期望：file already exists
os/> create c               → 期望：usage: create <name> [content]
os/> create "" x            → 期望：empty name
os/> cat missing            → 期望：file not found
os/> delete b               → 期望：无输出（成功）
os/> cat b                  → 期望：file not found（已删除）
os/> ls                     → 期望：只显示 a  5
os/> delete b               → 期望：file not found（重复删除）
```

### 32.3 异常与边界

```
os/> （直接按回车）         → 期望：新提示符（无报错）
os/> （连续输入超过 255 字符）→ 期望：超过后不再接收，但系统不崩
os/> exit                   → 期望：正常关机
```

**测试记录**：建议每次维护后把测试结果截图或记在 README 里（比如"2026-08-22 全部通过"），方便追溯。

## 33. 自己实现一个 printf（进阶选修）

日常维护中，"输出数字"的需求越来越多（文件大小、错误码）。`itoa_dec` 一次只能转一个数，拼字符串很累。可以给项目加一个简易 printf。

**原理**：`printf` 的本质 = 遍历格式字符串，遇到 `%s` 输出字符串、`%d` 输出数字，其余原样输出。实现它只需要 `terminal_write` + `itoa_dec`。

**在 `str.c` 加一个"格式化到缓冲区"的函数**（不直接输出，方便复用）：

```c
/* str.h 中声明：把 fmt 里的 %s（字符串）和 %d（整数）格式化到 buf */
void mz_format(char* buf, const char* fmt, const char* s, int d);
```

```c
/* str.c 中实现： */
void mz_format(char* buf, const char* fmt, const char* s, int d) {
    char num[12];
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {              /* %s：复制字符串参数 */
                while (*s) *buf++ = *s++;
            } else if (*fmt == 'd') {       /* %d：数字转字符串再复制 */
                itoa_dec((unsigned int)d, num);
                char* p = num;
                while (*p) *buf++ = *p++;
            }
            fmt++;
        } else {
            *buf++ = *fmt++;
        }
    }
    *buf = '\0';
}
```

**在 main.c 封装成终端输出版**：

```c
static void printf_simple(const char* fmt, const char* s, int d) {
    char buf[300];
    mz_format(buf, fmt, s, d);
    terminal_write(buf);
}

// 用法：printf_simple("file %s size is %d bytes\n", name, len);
```

**这个版本故意做得简陋**（只支持 `%s`、`%d`、固定两个参数），因为真实现 `va_list` 变参在这个项目里没必要。**够用就好**——这本身就是这个项目的哲学。

## 34. 扩展路线图（将来可能做的事）

给未来的维护者留一份"可以往哪走"的参考，按性价比排序：

| 优先级 | 方向 | 复杂度 | 说明 |
|---|---|---|---|
| ★★★ | `help` 命令 | 低 | 列出所有命令。参考第 16 节，把命令表写进代码 |
| ★★★ | 文件系统 API 扩展（改名、覆盖写、清空） | 低 | 参考第 19 节的方法：加函数 + 加命令 |
| ★★★ | 开机自动创建文件 | 低 | `kmain` 里在 `fs_init()` 后调用 `fs_create` |
| ★★☆ | 简易 printf | 低 | 第 33 节 |
| ★★☆ | 文件权限/只读标志 | 中 | `file_t` 加 `mode` 字段 + `fs_add` 加一条校验（第 9.4 节说明过） |
| ★★☆ | 从宿主机预置文件（multiboot modules） | 中 | 通过 QEMU `-initrd` 把文件带进内存，`fs_init` 里解析。**不需要写设备驱动** |
| ★☆☆ | 多级目录 | 高 | `file_t` 加类型与父目录字段，路径解析。**这是最值得引入"字符串拆分"需求的时机** |
| ★☆☆ | 磁盘持久化（ATA PIO） | 很高 | 写硬盘驱动 + 磁盘格式，工作量数天。**建议等前面的都做完再考虑** |
| ★☆☆ | 中断与多任务 | 很高 | 全新领域，需要先学操作系统理论 |

**决策建议**：每次只选一个方向，按第 4 节的工作循环推进。**每次改动保持"一次一改一测"**，即使功能简单也不要急。

## 35. 术语表（C 程序员视角）

| 术语 | 大白话解释 |
|---|---|
| 内核（kernel） | 操作系统的核心程序。MaxZOS 的 `kernel.elf` 就是它 |
| 裸机（bare metal） | 没有操作系统的硬件环境。内核就运行在裸机上 |
| freestanding | 编译模式：不依赖标准库。对应 `-ffreestanding` 参数 |
| ELF | Linux 下的可执行文件格式。`kernel.elf` 就是这种格式 |
| ISO | 光盘镜像文件格式。`myos.iso` 就是它 |
| QEMU | 模拟器软件，在普通电脑上模拟一台 x86 电脑 |
| 扫描码（scancode） | 键盘按键的硬件编号（比如 30 是 A 键）。内核收到的是扫描码，查表变成字符 |
| VGA 文本模式 | 显卡的一种显示模式：屏幕由固定网格的字符组成（80×25） |
| 显存 | 显卡上用于显示的内存。往 `0xB8000` 写数据 = 改屏幕 |
| 轮询（polling） | 不停地主动检查"有没有新事件"（对比：中断是被动通知） |
| 中断（interrupt） | 硬件主动通知 CPU 的机制。本内核未使用 |
| 堆（heap） | 动态内存区域，`malloc` 从里面分配。本内核没有 |
| 栈（stack） | 函数调用用的内存区。本内核 16KB，在 `.bss` 段 |
| .bss 段 | 程序里"自动清零的全局数据"区域。文件表就放在这里 |
| 链接（linking） | 把多个目标文件合成一个可执行文件的过程 |
| 链接脚本 | 规定链接时各段摆放位置的文件（`kernel/linker.ld`） |
| Multiboot | 引导协议：GRUB 按它加载内核 |
| ACPI | 电源管理标准。本内核用它实现关机 |
| 回调函数 | 作为参数传给另一个函数的函数（`fs_list(term_write_cb)` 里的 `term_write_cb`） |
| static（文件内） | 修饰函数/变量时表示"仅本文件可见"，防止命名冲突 |
| 未定义行为（UB） | C 语言标准不规定的结果，如数组越界。内核里后果通常直接崩溃 |

## 36. 附录

### 36.1 命令速查卡

**构建与运行**

```
make           编译全部 + 打包 ISO
make run       编译内核并运行（日常推荐）
make run-iso   编译并模拟光盘引导
make clean     删除 bin/
```

**系统内命令**

```
echo <文字>              输出文字（支持 "引号"）
clear  /  cls           清屏
create <名> [内容]       创建文件（内容带空格用引号；空文件用 ""）
cat <名>                查看文件内容
delete <名>             删除文件
ls                      列出文件
exit                    关机
```

**git**

```
git status              查看状态
git add .               暂存全部改动
git commit -m "说明"    提交存档
git log --oneline       查看历史
git restore .           放弃未提交改动（危险）
```

### 36.2 函数速查（全部可搜索到）

**main.c**（根目录）

| 函数 | 作用 |
|---|---|
| `kmain` | 入口：初始化 → 欢迎语 → 主循环 |
| `handle_keyboard` | 轮询键盘、组字、触发命令 |
| `process_command` | 命令解析与执行 |
| `cmd_is` | 命令精确匹配（带边界检查） |
| `skip_spaces` | 跳空格 |
| `extract_quoted` | 提取参数（支持双引号） |
| `terminal_write` / `terminal_putchar` / `terminal_clear` / `terminal_backspace` | 终端输出 |
| `fs_err_str` | 错误码 → 文案 |
| `term_write_cb` | fs_list 输出回调 |

**fs/fs.h**（文件系统接口）

| 函数 | 作用 |
|---|---|
| `fs_create(name, content)` | 创建 |
| `fs_read(name, out, maxlen)` | 读取 |
| `fs_delete(name)` | 删除 |
| `fs_list(out)` | 列出 |
| `fs_init()` | 初始化（预留） |

**fs/str.h**（字符串工具）

| 函数 | 作用 |
|---|---|
| `strlen(s)` | 字符串长度 |
| `strcmp(a, b)` | 字符串比较 |
| `strcpy(dst, src)` | 字符串复制 |
| `itoa_dec(value, buf)` | 数字转字符串 |

**acpi/acpi.h**

| 函数 | 作用 |
|---|---|
| `acpi_power_off()` | 关机 |

### 36.3 常量速查

| 常量 | 位置 | 值 | 改它 = 改什么 |
|---|---|---|---|
| `VGA_ATTR` | main.c | `0x1F` | 全局颜色 |
| `INPUT_BUF_SIZE` | main.c | `256` | 命令行输入上限 |
| `FS_MAX_FILES` | fs.h | `32` | 文件数上限 |
| `FS_MAX_NAME_LEN` | fs.h | `32` | 文件名上限（含 '\0'） |
| `FS_MAX_CONTENT` | fs.h | `256` | 文件内容上限 |
| 提示符文字 `"os/> "` | main.c（3 处） | — | 命令行提示符 |

---

# 结语

维护一个操作系统内核听起来很吓人，但**你已经看到了**：这个项目 90% 的维护工作就是"改 main.c 里的字符串和 if 分支"——和你平时写 C 程序没有本质区别。区别只在于**记得几条红线**（无标准库、无动态内存、别碰汇编）和**坚持一个好习惯**（一次一改、改完即测、常存 git）。

祝维护愉快。记住：**系统在 QEMU 里，折腾不坏真电脑；代码在 git 里，随时能回到能跑的状态。**

---

*本手册基于 MaxZOS 当前代码状态编写。文件路径、行号、常量值可能随版本变化，认函数名、认宏名，不要认死数字。*




