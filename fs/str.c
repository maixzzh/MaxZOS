/* str.c - 内核自带的轻量字符串工具
 * 注意：编译选项 -ffreestanding -fno-builtin，不链接 libc，
 *       定义 libc 同名函数不会冲突
 */
#include "str.h"

unsigned int strlen(const char* s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/* 仿 libc strncmp：比较前 n 个字符，遇 '\0' 提前结束 */
int strncmp(const char* s1, const char* s2, unsigned int n) {
    while (n > 0 && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *s1 - *s2;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++)) ;
    return dst;
}

/* 十进制整数转字符串（反转法），buf 至少 12 字节 */
void itoa_dec(unsigned int value, char* buf) {
    char tmp[10];          /* 32 位无符号整数最多 10 位数字 */
    int i = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (value > 0) {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }
    for (int j = 0; j < i; j++) {
        buf[j] = tmp[i - 1 - j];
    }
    buf[i] = '\0';
}
