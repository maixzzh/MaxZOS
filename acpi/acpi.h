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
 * acpi.h - ACPI 电源管理接口
 */
#ifndef ACPI_H
#define ACPI_H

/*
 * 通过 ACPI S5 状态关闭电源（正常情况不返回；
 * 若硬件不支持则尝试 QEMU/Bochs 关机端口后停机）
 */
void acpi_power_off(void);

#endif
