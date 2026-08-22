/*
 * Copyright (C) 2026 ZhangMaixuan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */


#ifndef FS_H
#define FS_H

#define FS_MAX_FILES     64      /* 条目总数上限（文件 + 目录共用一个表） */
#define FS_MAX_NAME_LEN  32      /* 名字缓冲（含 '\0'），单段最长 31 字符 */
#define FS_MAX_CONTENT   256     /* 文件内容最长 256 字节（另有 1 字节 '\0'） */
#define FS_MAX_PATH      128     /* fs_pwd 输出缓冲建议大小 */

#define FS_TYPE_FILE     0
#define FS_TYPE_DIR      1

typedef enum {
    FS_OK = 0,          /* 成功 */
    FS_EXISTS,          /* 同名条目已存在 */
    FS_NOT_FOUND,       /* 条目不存在（路径某段找不到） */
    FS_FULL,            /* 条目表已满（达到 FS_MAX_FILES） */
    FS_EMPTY_NAME,      /* 路径为空 */
    FS_NAME_TOO_LONG,   /* 单段名字超过 31 字符 */
    FS_BAD_CONTENT,     /* 内容超过 FS_MAX_CONTENT 字节 */
    /* ---- 新错误码追加在末尾（Help.md 约定，不重排旧值） ---- */
    FS_IS_DIR,          /* 期望是文件，实际是目录（如 cat 目录） */
    FS_NOT_DIR,         /* 期望是目录，实际是文件（路径中间段或末段） */
    FS_BAD_PATH,        /* 路径语法问题（如 create /、创建名为 . 或 ..） */
    FS_PATH_TOO_LONG,   /* 构造的路径超过 FS_MAX_PATH */
    FS_DIR_NOT_EMPTY,   /* 删除非空目录 */
    FS_IS_ROOT,         /* 试图删除根目录 */
    FS_IS_CWD,          /* 试图删除当前目录或其祖先目录 */
} fs_status_t;

typedef struct {
    char           name[FS_MAX_NAME_LEN];  /* 单段名字（不含 '/'）；根目录存 "/" */
    unsigned char  type;                   /* FS_TYPE_FILE / FS_TYPE_DIR */
    int            parent;                 /* 父目录索引；根目录指向自己（0） */
    int            next_sibling;           /* 父目录子链中的下一项；-1 = 链尾 */
    int            first_child;            /* 首个子项索引（仅目录）；-1 = 空目录 */
    unsigned short len;                    /* 内容长度（仅文件有效） */
    unsigned char  used;                   /* 1 = 槽位占用 */
    char           content[FS_MAX_CONTENT + 1];  /* 内容（仅文件有效） */
} file_t;

/* 输出回调：fs_list 使用，避免 fs 模块直接依赖 VGA 输出 */
typedef void (*fs_out_fn)(const char*);

fs_status_t fs_create(const char* path, const char* content);
fs_status_t fs_mkdir(const char* path);
fs_status_t fs_read(const char* path, char* out, unsigned int maxlen);
fs_status_t fs_delete(const char* path);
fs_status_t fs_list(const char* path, fs_out_fn out);   /* path == NULL 表示当前目录 */
fs_status_t fs_cd(const char* path);
fs_status_t fs_pwd(char* out, unsigned int maxlen);
void fs_init(void);   /* 建立根目录；挂载钩子：将来从 multiboot modules / 磁盘加载 */

#endif
