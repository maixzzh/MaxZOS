/*
 * Copyright (C) 2026 ZhangMaixuan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/* str.h - 内核自带的轻量字符串工具（无 libc 环境） */
#ifndef STR_H
#define STR_H

/* 裸机环境无 stddef.h，自行定义 NULL */
#ifndef NULL
#define NULL ((void*)0)
#endif

unsigned int strlen(const char* s);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, unsigned int n);
char* strcpy(char* dst, const char* src);
/* 十进制整数转字符串（反转法），buf 至少 12 字节 */
void itoa_dec(unsigned int value, char* buf);

#endif
