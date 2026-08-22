section .text
global _start            ; 导出 _start 作为链接器入口点

extern kmain             ; 声明外部 C 函数 kmain，定义在 main.c 中

_start:
    ; 1. 设置栈指针 (栈向下增长)
    ;    stack_top 是在 .bss 段中定义的栈顶地址
    mov esp, stack_top

    ; 2. 调用 C 内核入口函数 kmain
    ;    GRUB 在启动内核时，将 magic 值放在 eax，multiboot_info 指针放在 ebx
    ;    按照 cdecl 调用约定，参数从右向左压栈
    push ebx             ; 第二个参数：addr (multiboot_info 指针)
    push eax             ; 第一个参数：magic (0x2BADB002 表示成功)
    call kmain

    ; 3. 如果 kmain 意外返回，则进入死循环（安全措施）
    cli                  ; 禁用中断，防止干扰
    hlt                  ; 暂停 CPU
    jmp $                ; 无限跳转到自身
section .bss
align 16                ; 按 16 字节对齐，优化性能
stack_bottom:
    resb 16384          ; 保留 16 KB (16384 字节) 的栈空间
stack_top:              ; 栈顶标记（高地址）