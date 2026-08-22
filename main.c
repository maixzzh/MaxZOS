/*
 * main.c - 内核入口点，使用 C 语言直接操作 VGA 显存输出文本
 * 编译要求：-ffreestanding -nostdlib -m32 -fno-builtin -fno-stack-protector
 */

#define VGA_MEMORY 0xB8000
#define VGA_ATTR   0x0F
#define COLS       80      // 每行字符数
#define ROWS       25      // 总行数

void kmain(unsigned long magic, unsigned long addr) {
    volatile unsigned short* vga = (volatile unsigned short*)VGA_MEMORY;
    const char* message = "Hello, World!\nThis is zmx's OS.";
    unsigned int i = 0;
    unsigned int pos = 0; 

    while (message[i] != '\0') {
        if (message[i] == '\n') {
            pos = ((pos / COLS) + 1) * COLS;
        } else {
            vga[pos] = (VGA_ATTR << 8) | message[i];
            pos++; 
        }
        i++;
    }
    while (1) {
        asm volatile ("hlt");
    }
}