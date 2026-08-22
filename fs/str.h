/* str.h - 内核自带的轻量字符串工具（无 libc 环境） */
#ifndef STR_H
#define STR_H

/* 裸机环境无 stddef.h，自行定义 NULL */
#ifndef NULL
#define NULL ((void*)0)
#endif

unsigned int strlen(const char* s);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dst, const char* src);
/* 十进制整数转字符串（反转法），buf 至少 12 字节 */
void itoa_dec(unsigned int value, char* buf);

#endif
