/*
 * Max_Z - A toy operating system kernel
 * Copyright (C) 2026 ZhangMaixuan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */


/*
 * acpi.c - 通过 ACPI 实现关机（S5 状态）
 * 流程：扫描 RSDP → 解析 RSDT/XSDT 找到 FADT → 写 PM1a_CNT 的 SLP_TYP+SLP_EN
 * 编译选项：-m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector
 */

#include "acpi.h"

/* ---------- 端口访问（与 main.c 中 inb 的内联汇编风格一致） ---------- */
static inline void outw(unsigned short port, unsigned short val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(unsigned short port, unsigned int val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* ACPI 校验和：区域内所有字节之和 mod 256 应为 0 */
static unsigned char acpi_checksum(const unsigned char* p, unsigned int len) {
    unsigned char sum = 0;
    for (unsigned int i = 0; i < len; i++) {
        sum += p[i];
    }
    return sum;
}

/* 在 [start, end) 区域中按 16 字节对齐搜索 RSDP（签名 "RSD PTR "） */
static const unsigned char* find_rsdp_in(unsigned int start, unsigned int end) {
    for (unsigned int addr = start; addr < end; addr += 16) {
        const unsigned char* p = (const unsigned char*)addr;
        if (p[0] == 'R' && p[1] == 'S' && p[2] == 'D' && p[3] == ' ' &&
            p[4] == 'P' && p[5] == 'T' && p[6] == 'R' && p[7] == ' ' &&
            acpi_checksum(p, 20) == 0) {
            return p;
        }
    }
    return 0;
}

/* 查找 RSDP：先扫 BIOS ROM 区（0xE0000-0xFFFFF），再扫 EBDA */
static const unsigned char* find_rsdp(void) {
    const unsigned char* rsdp = find_rsdp_in(0xE0000, 0x100000);
    if (rsdp) return rsdp;

    // EBDA：0x40E 处存放 EBDA 段地址，扫描其 1KB 区域
    unsigned short ebda_seg = *(volatile unsigned short*)0x40E;
    if (ebda_seg != 0) {
        rsdp = find_rsdp_in((unsigned int)ebda_seg * 16,
                            (unsigned int)ebda_seg * 16 + 1024);
    }
    return rsdp;
}

/*
 * 在 RSDT/XSDT 的表项数组中查找指定签名的表（如 "FACP"），返回表地址；找不到返回 0。
 * 表头 36 字节 SDT 头之后是表项数组：RSDT 为 u32 表项，XSDT 为 u64 表项
 * （32 位内核取低 32 位）。
 */
static unsigned int find_table(const unsigned char* sdt, const char* sig) {
    if (sdt == 0) return 0;

    unsigned int len = sdt[4] | (sdt[5] << 8) | (sdt[6] << 16) | (sdt[7] << 24);
    if (len < 36) return 0;

    if (sdt[0] == 'R' && sdt[1] == 'S' && sdt[2] == 'D' && sdt[3] == 'T') {
        unsigned int count = (len - 36) / 4;
        const unsigned int* entries = (const unsigned int*)(sdt + 36);
        for (unsigned int i = 0; i < count; i++) {
            const unsigned char* p = (const unsigned char*)entries[i];
            if (p[0] == sig[0] && p[1] == sig[1] && p[2] == sig[2] && p[3] == sig[3]) {
                return (unsigned int)entries[i];
            }
        }
    } else if (sdt[0] == 'X' && sdt[1] == 'S' && sdt[2] == 'D' && sdt[3] == 'T') {
        unsigned int count = (len - 36) / 8;
        const unsigned long long* entries = (const unsigned long long*)(sdt + 36);
        for (unsigned int i = 0; i < count; i++) {
            const unsigned char* p = (const unsigned char*)(unsigned int)entries[i];
            if (p[0] == sig[0] && p[1] == sig[1] && p[2] == sig[2] && p[3] == sig[3]) {
                return (unsigned int)entries[i];
            }
        }
    }
    return 0;
}

/* 定位 FADT（"FACP"）：ACPI 2.0+ 优先走 XSDT，否则回退 RSDT */
static const unsigned char* find_fadt(void) {
    const unsigned char* rsdp = find_rsdp();
    if (rsdp == 0) return 0;

    if (rsdp[15] >= 2) {
        // XSDT 地址在 RSDP 偏移 24（u64），扩展校验和覆盖 36 字节
        unsigned long long xsdt = *(const unsigned long long*)(rsdp + 24);
        if (xsdt != 0 && acpi_checksum(rsdp, 36) == 0) {
            unsigned int fadt = find_table((const unsigned char*)(unsigned int)xsdt, "FACP");
            if (fadt) return (const unsigned char*)fadt;
        }
    }

    // ACPI 1.0 或 XSDT 无效时回退 RSDT（地址在 RSDP 偏移 16，u32）
    unsigned int rsdt = *(const unsigned int*)(rsdp + 16);
    if (rsdt != 0) {
        return (const unsigned char*)find_table((const unsigned char*)rsdt, "FACP");
    }
    return 0;
}

/*
 * 向 PM1a/PM1b 控制寄存器写入 S5 关机命令。
 * FADT 标准字段偏移（ACPI 1.0 起均有效）：
 *   pm1a_cnt_blk : +64 (u32)   pm1b_cnt_blk : +68 (u32)
 *   pm1_cnt_len  : +89 (u8，QEMU 为 2 = 16 位寄存器)
 * SLP_TYP 取 5（S5）：QEMU/SeaBIOS 与绝大多数 x86 硬件一致；
 * 严格的 \_S5 值需解析 DSDT 的 AML，超出本内核范围。
 */
static void acpi_write_s5(const unsigned char* fadt) {
    unsigned int fadt_len = fadt[4] | (fadt[5] << 8) | (fadt[6] << 16) | (fadt[7] << 24);
    if (fadt_len < 92) return;   // 至少覆盖到 pm1_cnt_len（偏移 89）之后

    unsigned int pm1a_cnt = *(const unsigned int*)(fadt + 64);
    unsigned int pm1b_cnt = *(const unsigned int*)(fadt + 68);
    unsigned char pm1_cnt_len = fadt[89];

    // SCI_EN(bit0) | SLP_TYP=5(bit10-12) | SLP_EN(bit13)
    unsigned int value = (1 << 0) | (5 << 10) | (1 << 13);

    if (pm1a_cnt != 0) {
        if (pm1_cnt_len >= 4) outl(pm1a_cnt, value);
        else                  outw(pm1a_cnt, (unsigned short)value);
    }
    if (pm1b_cnt != 0) {   // 有 PM1b 时再写一份，提高兼容性
        if (pm1_cnt_len >= 4) outl(pm1b_cnt, value);
        else                  outw(pm1b_cnt, (unsigned short)value);
    }
}

void acpi_power_off(void) {
    const unsigned char* fadt = find_fadt();
    if (fadt != 0) {
        acpi_write_s5(fadt);
    }

    // 兜底：ACPI 未生效时尝试 QEMU / Bochs 的关机端口，最后停机
    outw(0x604, 0x2000);   // QEMU（i440fx 的 PM1a_CNT 端口）
    outw(0xB004, 0x2000);  // Bochs
    asm volatile("cli; hlt" ::: "memory");
    for (;;) { }
}
