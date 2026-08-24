/*
 * Copyright (C) 2026 ZhangMaixuan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */



#include "fs.h"
#include "str.h"

static file_t files[FS_MAX_FILES];   /* .bss 零初始化，即全部空闲 */
static int fs_cwd = 0;               /* 当前目录索引（0 = 根） */

/* 查找空闲槽位（从 1 起，索引 0 恒为根目录），返回索引或 -1 */
static int fs_find_free(void) {
    for (int i = 1; i < FS_MAX_FILES; i++) {
        if (!files[i].used) {
            return i;
        }
    }
    return -1;
}

/* 在 dir 的子链中按名字（长度 + 内容）查找条目，返回索引或 -1。
 * 注意：调用前必须保证 namelen <= FS_MAX_NAME_LEN - 1，
 * 这样 files[c].name[namelen] 的 '\0' 边界判断不会越界 */
static int fs_lookup_child(int dir, const char* name, unsigned int namelen) {
    int c = files[dir].first_child;
    while (c >= 0) {
        if (strncmp(files[c].name, name, namelen) == 0 &&
            files[c].name[namelen] == '\0') {
            return c;
        }
        c = files[c].next_sibling;
    }
    return -1;
}

/* 路径解析核心：把 path 拆成（父目录索引 parent_out，末段名字 name_out/namelen_out）。
 * 绝对路径（/ 开头）从根开始，相对路径从当前目录开始。
 * 语义：
 *   - ""       → FS_EMPTY_NAME
 *   - "/"      → 位置本身（namelen_out = 0，name_out = NULL）
 *   - 末段 "." → 位置本身；末段 ".." → 父目录（同 namelen = 0 标记）
 *   - 连续斜杠 a//b、尾部斜杠 a/ 容忍
 *   - 中间段必须是已存在目录；"." 跳过、".." 上溯
 * 注意：段不复制（零拷贝），name_out 指向 path 内部；段尾可能不是 '\0'，
 *       调用方拷贝时必须按 namelen 逐字节复制 */
static fs_status_t fs_resolve_path(const char* path, int* parent_out,
                                   const char** name_out, unsigned int* namelen_out) {
    int cur;
    const char* p;

    if (path[0] == '\0') return FS_EMPTY_NAME;

    cur = (path[0] == '/') ? 0 : fs_cwd;
    p = path;

    for (;;) {
        const char* seg = p;
        unsigned int seglen;

        while (*p && *p != '/') p++;
        seglen = (unsigned int)(p - seg);

        if (seglen == 0) {
            /* 空段只可能来自斜杠（前导 / 或 //）：跳过连续斜杠再判断 */
            while (*p == '/') p++;
            if (*p == '\0') {               /* 没有最后一段（路径是 "/" 或 "//"） */
                *parent_out = cur;          /* 返回位置本身，namelen = 0 标记 */
                *name_out = NULL;
                *namelen_out = 0;
                return FS_OK;
            }
            continue;
        }
        if (seglen >= FS_MAX_NAME_LEN) return FS_NAME_TOO_LONG;

        {
            const char* q = p;
            while (*q == '/') q++;          /* 跳过连续斜杠，判断是否还有后续 */

            if (*q == '\0') {               /* seg 是最后一段（尾部斜杠已忽略） */
                if (seglen == 1 && seg[0] == '.') {       /* 末段 "." = 当前目录 */
                    *parent_out = cur;
                    *name_out = NULL;
                    *namelen_out = 0;
                    return FS_OK;
                }
                if (seglen == 2 && seg[0] == '.' && seg[1] == '.') {
                    *parent_out = files[cur].parent;      /* 末段 ".." = 父目录 */
                    *name_out = NULL;
                    *namelen_out = 0;
                    return FS_OK;
                }
                if (files[cur].type != FS_TYPE_DIR) return FS_NOT_DIR;  /* 父必须是目录 */
                *parent_out = cur;
                *name_out = seg;
                *namelen_out = seglen;
                return FS_OK;
            }

            /* 中间段：必须能下潜 */
            if (seglen == 1 && seg[0] == '.') { p = q; continue; }
            if (seglen == 2 && seg[0] == '.' && seg[1] == '.') {
                cur = files[cur].parent;
                p = q;
                continue;
            }
            if (files[cur].type != FS_TYPE_DIR) return FS_NOT_DIR;
            {
                int child = fs_lookup_child(cur, seg, seglen);
                if (child < 0) return FS_NOT_FOUND;
                cur = child;
            }
            p = q;
        }
    }
}

/* 全路径查找：把 path 解析到具体条目，返回索引（>= 0）或负错误码（-FS_XXX）。
 * "/"、"."、".." 等 namelen = 0 的解析结果直接落位 */
static int fs_resolve(const char* path) {
    int parent;
    const char* name;
    unsigned int namelen;
    fs_status_t st = fs_resolve_path(path, &parent, &name, &namelen);
    int i;

    if (st != FS_OK) return -st;
    if (namelen == 0) return parent;
    i = fs_lookup_child(parent, name, namelen);
    if (i < 0) return -FS_NOT_FOUND;
    return i;
}

/* 共用写槽核心：校验链（末段名 → 查重 → 内容超长 → 满）后写入并挂到父链尾。
 * name 必须按 namelen 逐字节拷贝（段尾可能是 '/' 而非 '\0'，不能 strcpy） */
static fs_status_t fs_add_entry(int parent, const char* name, unsigned int namelen,
                                unsigned char type, const char* content, unsigned int len) {
    int i;

    if (namelen == 0) return FS_BAD_PATH;       /* "/"、"mkdir ." 等无末段名 */
    if (namelen >= FS_MAX_NAME_LEN) return FS_NAME_TOO_LONG;
    if ((namelen == 1 && name[0] == '.') ||
        (namelen == 2 && name[0] == '.' && name[1] == '.')) {
        return FS_BAD_PATH;                     /* 唯一拒绝 "." 与 ".." 的地方 */
    }
    if (files[parent].type != FS_TYPE_DIR) return FS_NOT_DIR;   /* 父必须是目录 */
    if (fs_lookup_child(parent, name, namelen) >= 0) return FS_EXISTS;
    if (type == FS_TYPE_FILE && len > FS_MAX_CONTENT) return FS_BAD_CONTENT;

    i = fs_find_free();
    if (i < 0) return FS_FULL;

    /* 写字段 */
    for (unsigned int k = 0; k < namelen; k++) {
        files[i].name[k] = name[k];
    }
    files[i].name[namelen] = '\0';
    files[i].type = type;
    files[i].parent = parent;
    files[i].first_child = -1;
    files[i].next_sibling = -1;
    files[i].len = (unsigned short)len;
    for (unsigned int k = 0; k < len; k++) {
        files[i].content[k] = content[k];
    }
    files[i].content[len] = '\0';
    files[i].used = 1;

    /* 挂到父链尾（保持创建顺序） */
    if (files[parent].first_child < 0) {
        files[parent].first_child = i;
    } else {
        int c = files[parent].first_child;
        while (files[c].next_sibling >= 0) c = files[c].next_sibling;
        files[c].next_sibling = i;
    }
    return FS_OK;
}

fs_status_t fs_mkdir(const char* path) {
    int parent;
    const char* name;
    unsigned int namelen;
    fs_status_t st = fs_resolve_path(path, &parent, &name, &namelen);

    if (st != FS_OK) return st;
    return fs_add_entry(parent, name, namelen, FS_TYPE_DIR, NULL, 0);
}

fs_status_t fs_create(const char* path, const char* content) {
    int parent;
    const char* name;
    unsigned int namelen;
    fs_status_t st = fs_resolve_path(path, &parent, &name, &namelen);

    if (st != FS_OK) return st;
    return fs_add_entry(parent, name, namelen, FS_TYPE_FILE,
                        content, content ? strlen(content) : 0);
}

fs_status_t fs_read(const char* path, char* out, unsigned int maxlen) {
    int i = fs_resolve(path);
    unsigned int n;

    if (i < 0) return (fs_status_t)(-i);
    if (files[i].type != FS_TYPE_FILE) return FS_IS_DIR;
    if (maxlen == 0) return FS_OK;      /* 不写缓冲 */

    n = files[i].len;
    if (n > maxlen - 1) n = maxlen - 1; /* 截断：复制尽量多，保证 '\0' 结尾 */
    for (unsigned int k = 0; k < n; k++) {
        out[k] = files[i].content[k];
    }
    out[n] = '\0';
    return FS_OK;
}

/* 从父链摘除条目并释放槽位（调用方已保证 idx 不是根、目录为空、非 cwd/祖先） */
static void fs_unlink_entry(int idx) {
    int parent = files[idx].parent;
    int c = files[parent].first_child;

    if (c == idx) {
        files[parent].first_child = files[idx].next_sibling;
    } else {
        while (c >= 0 && files[c].next_sibling != idx) c = files[c].next_sibling;
        if (c >= 0) files[c].next_sibling = files[idx].next_sibling;
    }
    files[idx].used = 0;
}

fs_status_t fs_delete(const char* path) {
    int i = fs_resolve(path);
    int t;

    if (i < 0) return (fs_status_t)(-i);
    if (i == 0) return FS_IS_ROOT;               /* 根目录不可删 */
    if (files[i].type == FS_TYPE_DIR) {
        if (files[i].first_child >= 0) return FS_DIR_NOT_EMPTY;  /* 非空目录拒绝 */
        /* 是否当前目录或其祖先？沿 cwd 的父链上溯比对 */
        for (t = fs_cwd; t != 0; t = files[t].parent) {
            if (t == i) return FS_IS_CWD;
        }
    }
    fs_unlink_entry(i);
    return FS_OK;
}

fs_status_t fs_list(const char* path, fs_out_fn out) {
    int dir;
    int c;
    char num[12];

    if (path == NULL) {
        dir = fs_cwd;
    } else {
        int i = fs_resolve(path);
        if (i < 0) return (fs_status_t)(-i);
        if (files[i].type != FS_TYPE_DIR) return FS_NOT_DIR;
        dir = i;
    }

    for (c = files[dir].first_child; c >= 0; c = files[c].next_sibling) {
        out(files[c].name);
        if (files[c].type == FS_TYPE_DIR) {
            out("/\t-\n");                      /* 目录：名字 + "/" + 大小列 "-" */
        } else {
            out("\t");
            itoa_dec(files[c].len, num);
            out(num);
            out("\n");
        }
    }
    return FS_OK;
}

fs_status_t fs_cd(const char* path) {
    int i = fs_resolve(path);

    if (i < 0) return (fs_status_t)(-i);
    if (files[i].type != FS_TYPE_DIR) return FS_NOT_DIR;
    fs_cwd = i;                                 /* 原子：解析成功才更新 */
    return FS_OK;
}

fs_status_t fs_isdir(const char* path) {
    int i = fs_resolve(path);

    if (i < 0) return (fs_status_t)(-i);
    return files[i].type == FS_TYPE_DIR ? FS_OK : FS_NOT_DIR;
}

fs_status_t fs_pwd(char* out, unsigned int maxlen) {
    int chain[FS_MAX_FILES];            /* 栈上祖先链（自底向上） */
    int depth = 0;
    int total = 1;                      /* 根 "/" 占 1 */
    int cur;
    unsigned int k;

    for (cur = fs_cwd; cur != 0; cur = files[cur].parent) {
        chain[depth++] = cur;
        total += 1 + (int)strlen(files[cur].name);  /* "/" + 名字 */
    }
    if (total > (int)maxlen - 1) return FS_PATH_TOO_LONG;  /* 不写缓冲，安全失败 */

    k = 0;
    if (depth == 0) {
        out[k++] = '/';                         /* 根目录 */
    } else {
        for (int i = depth - 1; i >= 0; i--) {  /* 从根向下写 */
            out[k++] = '/';
            strcpy(out + k, files[chain[i]].name);
            k += (unsigned int)strlen(files[chain[i]].name);
        }
    }
    out[k] = '\0';
    return FS_OK;
}

void fs_init(void) {
    /* 建立根目录：固定索引 0，parent 自指（根处 ".." = 根），
     * 不挂在任何父链上，因此 ls 永远不会显示它 */
    files[0].name[0] = '/';
    files[0].name[1] = '\0';
    files[0].type = FS_TYPE_DIR;
    files[0].parent = 0;
    files[0].next_sibling = -1;
    files[0].first_child = -1;
    files[0].len = 0;
    files[0].used = 1;
    fs_cwd = 0;

    /* 预留：将来在此解析 multiboot modules / 挂载磁盘后端，
     * 预置条目直接调 fs_add_entry 填充，公开 API 与命令层零改动 */
}
