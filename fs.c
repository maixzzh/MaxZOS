/* fs.c - 简易内存文件系统（单级目录，全部静态数组，无堆分配）
 * 存储后端为 .bss 静态表，重启即丢失；fs.h 公开 API 签名冻结，
 * 将来迁移到磁盘后端时只需整体替换本文件内部实现
 */
#include "fs.h"
#include "str.h"

static file_t files[FS_MAX_FILES];   /* .bss 零初始化，即全部空闲 */

/* 查找同名文件，返回索引或 -1 */
static int fs_find(const char* name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* 查找空闲槽位，返回索引或 -1 */
static int fs_find_free(void) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) {
            return i;
        }
    }
    return -1;
}

/* 共用写槽核心：校验链（空名 → 超长 → 查重 → 内容超长 → 满）后写入 */
static fs_status_t fs_add(const char* name, const void* data, unsigned int len) {
    int i;

    if (name[0] == '\0') return FS_EMPTY_NAME;
    if (strlen(name) >= FS_MAX_NAME_LEN) return FS_NAME_TOO_LONG;  /* 需留 '\0' */
    if (fs_find(name) >= 0) return FS_EXISTS;
    if (len > FS_MAX_CONTENT) return FS_BAD_CONTENT;

    i = fs_find_free();
    if (i < 0) return FS_FULL;

    strcpy(files[i].name, name);
    for (unsigned int k = 0; k < len; k++) {
        files[i].content[k] = ((const char*)data)[k];
    }
    files[i].content[len] = '\0';
    files[i].len = (unsigned short)len;
    files[i].used = 1;
    return FS_OK;
}

fs_status_t fs_create(const char* name, const char* content) {
    return fs_add(name, content, content ? strlen(content) : 0);
}

fs_status_t fs_read(const char* name, char* out, unsigned int maxlen) {
    int i = fs_find(name);
    unsigned int n;

    if (i < 0) return FS_NOT_FOUND;
    if (maxlen == 0) return FS_OK;      /* 不写缓冲 */

    n = files[i].len;
    if (n > maxlen - 1) n = maxlen - 1; /* 截断：复制尽量多，保证 '\0' 结尾 */
    for (unsigned int k = 0; k < n; k++) {
        out[k] = files[i].content[k];
    }
    out[n] = '\0';
    return FS_OK;
}

fs_status_t fs_delete(const char* name) {
    int i = fs_find(name);
    if (i < 0) return FS_NOT_FOUND;
    files[i].used = 0;
    return FS_OK;
}

void fs_list(fs_out_fn out) {
    char num[12];
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (!files[i].used) continue;
        out(files[i].name);
        out("  ");
        itoa_dec(files[i].len, num);
        out(num);
        out("\n");
    }
}

void fs_init(void) {
    /* 预留：将来解析 multiboot modules / 挂载磁盘后端
     * 当前为空：.bss 已零初始化，所有槽位即空闲 */
}
