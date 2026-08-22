/*
 * main.c - 带有键盘输入和简单 Shell 的内核
 * 功能：显示提示符 "os/> "，接受用户输入，支持退格、回车，
 *       解析 echo / clear / cls / exit 命令，
 *       Shift 输入上档字符（如 " # $ @ 等），exit 通过 ACPI 关机
 * 编译选项：-m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector
 */

#include "acpi.h"

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

/* ---------- 命令解析 ---------- */
static void process_command(void) {
    // 1. 先换行（因为输入是在同一行）
    terminal_putchar('\n');

    // 2. 判断是否为空命令
    if (input_len == 0) {
        terminal_write("os/> ");
        return;
    }

    // 3. clear / cls：清屏（两个命令等价）
    if (strncmp(input_buf, "clear", 5) == 0 || strncmp(input_buf, "cls", 3) == 0) {
        terminal_clear();
    } else if (strncmp(input_buf, "echo", 4) == 0) {
        char* p = input_buf + 4;
        while (*p == ' ') p++;

        if (*p == '"') {
            // 有双引号：提取引号内容
            p++; 
            char* start = p;
            while (*p && *p != '"') p++;
            if (*p == '"') {
                *p = '\0';   // 临时截断
                terminal_write(start);
                terminal_putchar('\n');
            } else {
                terminal_write("unclosed quote\n");
            }
        } else {
            terminal_write(p);
            terminal_putchar('\n');
        }
    } else if (strncmp(input_buf, "exit", 4) == 0) {
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