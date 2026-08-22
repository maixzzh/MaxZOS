/* ============================================================
 * linker.ld - 链接脚本
 * 功能：定义内核的内存布局，确保 Multiboot 头在文件最前面，
 *       并指定入口点为 _start（在 boot.asm 中定义）
 * 目标格式：ELF 32-bit (i386)
 * ============================================================ */

ENTRY(_start)           /* 指定入口符号为 _start */

SECTIONS
{
    /* 内核加载基址：GRUB 默认会加载到 1MB 物理地址 */
    /* 对于 Multiboot 兼容内核，通常从 1MB 开始（0x100000） */
    . = 1M;

    /* 第一个段：Multiboot 头，必须放在最前面，使 GRUB 能识别 */
    .multiboot_header : ALIGN(4)
    {
        *(.multiboot_header)
    }

    /* 只读代码段 */
    .text : ALIGN(4K)
    {
        *(.text)
    }

    /* 只读数据段 */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    /* 读写数据段 */
    .data : ALIGN(4K)
    {
        *(.data)
    }

    /* BSS 段（未初始化数据，包括栈） */
    .bss : ALIGN(4K)
    {
        *(.bss)
    }

    /* 可选：调试信息等，但本示例不需要 */
}

