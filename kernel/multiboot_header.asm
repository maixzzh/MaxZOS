; ============================================================
; multiboot_header.asm
; 功能：提供 Multiboot 标准头，使 GRUB 能够识别并加载内核
; 标准：Multiboot Specification 0.6.96 (版本 1)
; ============================================================

section .multiboot_header          ; 将头放入 .multiboot_header 节
align 4                            ; 确保接下来的数据按 4 字节对齐

    ; 幻数 (Magic Number) - 由 Multiboot 规范定义
    dd 0x1BADB002                  ; 固定值，用于标识 Multiboot 头

    ; 标志 (Flags) - 控制 GRUB 的行为
    dd 0x00000003                  ; 低两位为 1：
                                   ; bit 0 = 1 : 要求 GRUB 按页（4KB）对齐
                                   ; bit 1 = 1 : 要求 GRUB 提供内存信息（通过 multiboot_info）
                                   ; 其余位为 0

    ; 校验和 (Checksum)
    dd -(0x1BADB002 + 0x00000003)  ; 使得 magic + flags + checksum = 0