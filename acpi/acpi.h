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
