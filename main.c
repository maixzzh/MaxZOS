/*
 * main.c - 带有键盘输入和简单 Shell 的内核
 * 功能：显示提示符 "os/> "，接受用户输入，支持退格、回车，
 *       解析 echo / clear / cls / exit 命令，
 *       Shift 输入上档字符（如 " # $ @ 等），exit 通过 ACPI 关机
 * 编译选项：-m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector
 */

#include "acpi.h"
#include "fs.h"
#include "str.h"

#define VGA_MEMORY  0xB8000
#define VGA_ATTR    0x1F
#define COLS        80
#define ROWS        25
#define SCR_SIZE    (COLS * ROWS)
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

/* ---------- 字符串比较（仿 strncmp） ---------- */
static int strncmp(const char* s1, const char* s2, unsigned int n) {
    while (n > 0 && *s1 && *s1 == *s2) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *s1 - *s2;
}

/* ---------- 命令解析辅助 ---------- */

/* 命令匹配：前缀相等且后面必须是空格或行尾（修复 "clearxxx" 误命中 "clear"） */
static int cmd_is(const char* name) {
    unsigned int n = strlen(name);
    return strncmp(input_buf, name, n) == 0 &&
           (input_buf[n] == '\0' || input_buf[n] == ' ');
}

/* 跳过前导空格 */
static char* skip_spaces(char* p) {
    while (*p == ' ') p++;
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

/* fs_list 输出回调：转发到终端输出 */
static void term_write_cb(const char* s) {
    terminal_write(s);
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
    default:               return "unknown fs error\n";
    }
}

/* ---------- 命令解析 ---------- */
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
        // create <name> [content]：名字为第一个 token，内容支持双引号
        char* p = skip_spaces(input_buf + 6);
        char* name = p;
        while (*p && *p != ' ' && *p != '"') p++;   // 名字 = 第一个 token
        if (*p == '\0') {
            terminal_write("usage: create <name> [content]\n");
        } else {
            char* rest = p + 1;   // 分隔符之后的内容
            *p = '\0';            // 终止名字
            char* content = extract_quoted(&rest);
            if (content == NULL) {
                terminal_write("unclosed quote\n");
            } else {
                fs_status_t st = fs_create(name, content);
                if (st != FS_OK) terminal_write(fs_err_str(st));
            }
        }
    } else if (cmd_is("cat")) {
        // cat <name>：显示文件内容
        char* p = skip_spaces(input_buf + 3);
        if (*p == '\0') {
            terminal_write("usage: cat <name>\n");
        } else {
            char buf[FS_MAX_CONTENT + 1];
            fs_status_t st = fs_read(p, buf, sizeof(buf));
            if (st == FS_OK) {
                terminal_write(buf);
                terminal_putchar('\n');
            } else {
                terminal_write(fs_err_str(st));
            }
        }
    } else if (cmd_is("delete")) {
        // delete <name>：删除文件
        char* p = skip_spaces(input_buf + 6);
        if (*p == '\0') {
            terminal_write("usage: delete <name>\n");
        } else {
            fs_status_t st = fs_delete(p);
            if (st != FS_OK) terminal_write(fs_err_str(st));
        }
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

    // 清屏并显示启动横幅与初始提示符
    terminal_clear();
    terminal_write("Welcome to MaxZOS v0.9\n");
    terminal_write("made by ZhangMaixuan\n");
    terminal_write("os/> ");

    // 主循环：轮询键盘
    while (1) {
        handle_keyboard();
    }
}