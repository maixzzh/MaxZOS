/*
 * Copyright (C) 2026 ZhangMaixuan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "acpi.h"
#include "fs.h"
#include "str.h"

/* ---------- 系统标识信息（集中定义，改这里即可全局生效） ---------- */
#define OS_NAME            "MaxZOS"        /* 系统名称（横幅、about、帮助链接共用） */
#define OS_AUTHOR          "ZhangMaixuan"  /* 作者（横幅与 about 共用） */
#define OS_VERSION_MAJOR   0               /* 版本号：大版本 */
#define OS_VERSION_MINOR   9               /* 版本号：中版本 */
#define OS_BUILD_YEAR      2026            /* 构建年 */
#define OS_BUILD_DATE      "0823"          /* 构建日期（月日，4 位数字） */

/* 字符串化辅助：先展开宏再转成字符串（否则得到 "OS_VERSION_MAJOR" 而非 "0"） */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
/* 显示用版本串（如 "0.9"）与构建日期串（如 "2026-0823"），由上面的常量拼接 */
#define OS_VERSION_STR  STR(OS_VERSION_MAJOR) "." STR(OS_VERSION_MINOR)
#define OS_BUILD_STR    STR(OS_BUILD_YEAR) "-" OS_BUILD_DATE

/* ---------- 提示符（os + 当前路径 + "> "，如 "os/> "、"os/docs> "） ---------- */
#define OS_PROMPT_NAME   "MaxZOS"   /* 提示符前缀 */
#define OS_PROMPT_SUFFIX "$"   /* 提示符后缀 */

#define VGA_MEMORY  0xB8000
#define VGA_ATTR    0x1F
#define COLS        80
#define ROWS        25
#define SCR_SIZE    (COLS * ROWS)
#define TAB_WIDTH   8     /* Tab 制表位宽度（列数），可调为 4 使排版更紧凑 */
#define KEYBOARD_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KBD_STATUS_OUTPUT_FULL 0x01
#define INPUT_BUF_SIZE 256
static char input_buf[INPUT_BUF_SIZE];
static int  input_len = 0;
static unsigned char shift_down = 0;  // Shift 是否处于按下状态
static volatile unsigned short* vga = (volatile unsigned short*)VGA_MEMORY;
static unsigned int pos = 0;    // 当前光标所在的线性索引 (0 ~ SCR_SIZE-1)

// 从端口读一个字节
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 清屏填充空格并将 pos 置 0
static void terminal_clear(void) {
    for (int i = 0; i < SCR_SIZE; i++) {
        vga[i] = (VGA_ATTR << 8) | ' ';
    }
    pos = 0;
}

static void terminal_putchar(char c) {
    if (c == '\n') {
        pos = ((pos / COLS) + 1) * COLS;
        if (pos >= SCR_SIZE) {
            pos = (ROWS - 1) * COLS;
        }
        return;
    }
    if (c == '\t') {
        // 跳到下一个制表位（每 TAB_WIDTH 列一个）
        pos = ((pos / TAB_WIDTH) + 1) * TAB_WIDTH;
        if (pos >= SCR_SIZE) {
            pos = (ROWS - 1) * COLS;
        }
        return;
    }

    vga[pos] = (VGA_ATTR << 8) | c;
    pos++;
    // 仅当超出屏幕底部时限制在最后一行（不滚动）
    if (pos >= SCR_SIZE) {
        pos = (ROWS - 1) * COLS;
    }
}


static void terminal_write(const char* s) {
    while (*s) {
        terminal_putchar(*s++);
    }
}

// 退格：删除前一个字符（在屏幕上擦除并更新 pos）
static void terminal_backspace(void) {
    if (pos > 0 && pos % COLS != 0) { // 不在行首
        pos--;
        vga[pos] = (VGA_ATTR << 8) | ' '; // 用空格覆盖
    } else if (pos >= COLS) {
        pos -= COLS;
        if (pos % COLS == 0 && pos > 0) {
            pos += COLS - 1; // 移到上一行末尾
            vga[pos] = (VGA_ATTR << 8) | ' ';
        }
    }
}

/* ---------- 命令解析辅助 ---------- */

/* 命令匹配：前缀相等且后面必须是空格、Tab 或行尾（修复 "clearxxx" 误命中 "clear"） */
static int cmd_is(const char* name) {
    unsigned int n = strlen(name);
    return strncmp(input_buf, name, n) == 0 &&
           (input_buf[n] == '\0' || input_buf[n] == ' ' || input_buf[n] == '\t');
}

/* 跳过前导空格与 Tab（与空格等价的空白分隔符） */
static char* skip_spaces(char* p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* 参数提取：跳过前导空格；若以 '"' 开头则提取引号内内容（原地截断），
 * *pp 推进到参数结束之后；未闭合引号返回 NULL */
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

/* 取下一个参数：跳过空白；引号开头 → 引号内容；否则取到空白处（原地截断）。
 * 成功返回参数指针并推进 *pp；无参数返回 NULL */
static char* next_arg(char** pp) {
    char* p = skip_spaces(*pp);
    if (*p == '\0') return NULL;
    if (*p == '"') return extract_quoted(pp);
    char* start = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    if (*p != '\0') { *p = '\0'; *pp = p + 1; }
    else            { *pp = p; }
    return start;
}

/* 返回当前命令名之后（跳过空白）的参数起点；须在 cmd_is 命中后调用，
 * 替代各分支里手工硬编码的命令名长度偏移 */
static char* cmd_args(void) {
    char* p = input_buf;
    while (*p && *p != ' ' && *p != '\t') p++;
    return skip_spaces(p);
}

/* fs_list 输出回调：转发到终端输出 */
static void term_write_cb(const char* s) {
    terminal_write(s);
}

/* 提示符 = OS_PROMPT_NAME + 当前路径 + OS_PROMPT_SUFFIX；根为 "os/> " */
static void print_prompt(void) {
    char path[FS_MAX_PATH];
    if (fs_pwd(path, sizeof(path)) != FS_OK) {
        terminal_write(OS_PROMPT_NAME "/" OS_PROMPT_SUFFIX);  /* 兜底：回根提示（理论不可达） */
        return;
    }
    terminal_write(OS_PROMPT_NAME);
    terminal_write(path);
    terminal_write(OS_PROMPT_SUFFIX);
}

/* 文件系统错误码 → 提示消息 */
static const char* fs_err_str(fs_status_t s) {
    switch (s) {
    case FS_OK:            return "";
    case FS_EXISTS:        return "file already exists\n";
    case FS_NOT_FOUND:     return "file not found\n";
    case FS_FULL:          return "file table full\n";
    case FS_EMPTY_NAME:    return "empty name\n";
    case FS_NAME_TOO_LONG: return "name too long (max 31 chars)\n";
    case FS_BAD_CONTENT:   return "content too long (max 256 bytes)\n";
    case FS_IS_DIR:        return "is a directory\n";
    case FS_NOT_DIR:       return "not a directory\n";
    case FS_BAD_PATH:      return "bad path\n";
    case FS_PATH_TOO_LONG: return "path too long\n";
    case FS_DIR_NOT_EMPTY: return "directory not empty\n";
    case FS_IS_ROOT:       return "cannot remove root\n";
    case FS_IS_CWD:        return "cannot remove current directory\n";
    default:               return "unknown fs error\n";
    }
}

/* ---------- 命令解析 ---------- */
static void process_command(void) {
    // 1. 先换行（因为输入是在同一行）
    terminal_putchar('\n');

    // 2. 判断是否为空命令
    if (input_len == 0) {
        print_prompt();
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
        // create <path> [content]：路径为第一个 token，内容支持双引号
        char* p = cmd_args();
        char* name = next_arg(&p);
        if (name == NULL || *skip_spaces(p) == '\0') {
            // 无路径或无内容：沿用旧版 "create <name> [content]" 的 usage 行为
            terminal_write("usage: create <path> [content]\n");
        } else {
            char* content = extract_quoted(&p);
            if (content == NULL) {
                terminal_write("unclosed quote\n");
            } else {
                fs_status_t st = fs_create(name, content);
                if (st != FS_OK) terminal_write(fs_err_str(st));
            }
        }
    } else if (cmd_is("cat")) {
        // cat <path>：显示文件内容（只取第一个参数）
        char* p = cmd_args();
        char* arg = next_arg(&p);
        if (arg == NULL) {
            terminal_write("usage: cat <path>\n");
        } else {
            char buf[FS_MAX_CONTENT + 1];
            fs_status_t st = fs_read(arg, buf, sizeof(buf));
            if (st == FS_OK) {
                terminal_write(buf);
                terminal_putchar('\n');
            } else {
                terminal_write(fs_err_str(st));
            }
        }
    } else if (cmd_is("rm") || cmd_is("delete")) {
        // rm / delete <path>：删除文件或空目录（delete 为旧名兼容）
        char* p = cmd_args();
        char* arg = next_arg(&p);
        if (arg == NULL) {
            terminal_write("usage: rm <path>\n");
        } else {
            fs_status_t st = fs_delete(arg);
            if (st != FS_OK) terminal_write(fs_err_str(st));
        }
    } else if (cmd_is("ls")) {
        // ls [path]：列出目录子项；无参数列出当前目录
        char* p = cmd_args();
        char* arg = next_arg(&p);
        terminal_write("Name\tSize(Byte)\n");
        fs_status_t st = fs_list(arg, term_write_cb);
        if (st != FS_OK) terminal_write(fs_err_str(st));
    } else if (cmd_is("mkdir")) {
        // mkdir <path>：创建目录
        char* p = cmd_args();
        char* arg = next_arg(&p);
        if (arg == NULL) {
            terminal_write("usage: mkdir <path>\n");
        } else {
            fs_status_t st = fs_mkdir(arg);
            if (st != FS_OK) terminal_write(fs_err_str(st));
        }
    } else if (cmd_is("cd")) {
        // cd <path>：切换当前目录；成功无输出（提示符自动刷新）
        char* p = cmd_args();
        char* arg = next_arg(&p);
        if (arg == NULL) {
            terminal_write("usage: cd <path>\n");
        } else {
            fs_status_t st = fs_cd(arg);
            if (st != FS_OK) terminal_write(fs_err_str(st));
        }
    } else if (cmd_is("exit")) {
        // exit：ACPI 关机（正常情况不会返回）
        terminal_write("Shutting down...\n");
        acpi_power_off();
    } else if (cmd_is("about")){
        terminal_write(OS_NAME " v" OS_VERSION_STR " (" OS_BUILD_STR ") made by " OS_AUTHOR "\n");
    }else {
        terminal_write("unknown command\n");
    }

    input_len = 0;
    print_prompt();
}

/* ---------- 键盘处理 ---------- */
static void handle_keyboard(void) {
    // 先检查状态寄存器：仅当输出缓冲区有数据时才读取，否则返回，
    // 避免空读 0x60 端口返回陈旧扫描码导致按键被无限重复处理
    unsigned char status = inb(KEYBOARD_STATUS_PORT);
    if (!(status & KBD_STATUS_OUTPUT_FULL)) return;

    unsigned char scancode = inb(KEYBOARD_PORT);

    // Shift 按下/松开：松开码带 0x80 位，必须放在释放判断之前处理
    if (scancode == 0x2A || scancode == 0x36) { shift_down = 1; return; }
    if (scancode == 0xAA || scancode == 0xB6) { shift_down = 0; return; }

    if (scancode & 0x80) return;   // 其余键的松开码，忽略
    if (scancode >= 128) return;   // 越界保护（0xE0 扩展前缀等）

    static const unsigned char kbdus[128] = {
        0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
        0,   '*', 0,  ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        // 数字小键盘等其他键不映射
    };
    // Shift 按下时的映射：数字键变符号、字母大写、标点变上档符号
    static const unsigned char kbdus_shift[128] = {
        0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
        0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',
        0,   '*', 0,  ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    // Shift 按下时用上档映射表，否则用普通映射表
    unsigned char ch = shift_down ? kbdus_shift[scancode] : kbdus[scancode];
    if (ch == 0) return;  // 未定义

    if (ch == '\n') {
        input_buf[input_len] = '\0';   // 结束符
        process_command();
        return;
    }

    if (ch == '\b') {
        if (input_len > 0) {
            input_len--;
            terminal_backspace();
        }
        return;
    }
    if (ch == '\t') {
        // Tab 键：存入输入缓冲并回显（跳到下一个制表位）
        if (input_len < INPUT_BUF_SIZE - 1) {
            input_buf[input_len++] = ch;
            terminal_putchar(ch);
        }
        return;
    }
    if (ch >= 32 && ch < 127) {
        if (input_len < INPUT_BUF_SIZE - 1) {
            input_buf[input_len++] = ch;
            terminal_putchar(ch);
        }
    }
}

void kmain(unsigned long magic, unsigned long addr) {
    // 挂载文件系统（预留：将来从 multiboot modules / 磁盘加载）
    fs_init();

    // 清屏并显示启动横幅与初始提示符（文字全部由开头常量生成）
    terminal_clear();
    terminal_write("Welcome to " OS_NAME " v" OS_VERSION_STR "\n");
    terminal_write("made by " OS_AUTHOR "\n");
    terminal_write("If you want to get help,please to github repo: maixzzh/" OS_NAME "\n\n");
    print_prompt();

    // 主循环：轮询键盘
    while (1) {
        handle_keyboard();
    }
}