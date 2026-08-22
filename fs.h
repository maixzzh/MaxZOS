/* fs.h - 简易内存文件系统（单级目录，全部静态数组，无堆分配） */
#ifndef FS_H
#define FS_H

#define FS_MAX_FILES     32    /* 文件数上限 */
#define FS_MAX_NAME_LEN  32    /* 文件名缓冲（含 '\0'），最长 31 字符 */
#define FS_MAX_CONTENT   256   /* 内容最长 256 字节（另有 1 字节 '\0' 结尾） */

typedef enum {
    FS_OK = 0,          /* 成功 */
    FS_EXISTS,          /* 同名文件已存在 */
    FS_NOT_FOUND,       /* 文件不存在 */
    FS_FULL,            /* 文件表已满（达到 FS_MAX_FILES） */
    FS_EMPTY_NAME,      /* 文件名为空 */
    FS_NAME_TOO_LONG,   /* 文件名超过 31 字符 */
    FS_BAD_CONTENT,     /* 内容超过 FS_MAX_CONTENT 字节 */
} fs_status_t;

typedef struct {
    char  name[FS_MAX_NAME_LEN];
    char  content[FS_MAX_CONTENT + 1];
    unsigned short len;      /* 内容长度（不含 '\0'） */
    unsigned char  used;     /* 1 = 槽位占用 */
} file_t;

/* 输出回调：fs_list 使用，避免 fs 模块直接依赖 VGA 输出 */
typedef void (*fs_out_fn)(const char*);

fs_status_t fs_create(const char* name, const char* content);
fs_status_t fs_read(const char* name, char* out, unsigned int maxlen);
fs_status_t fs_delete(const char* name);
void fs_list(fs_out_fn out);
void fs_init(void);   /* 预留：挂载 / 将来从 multiboot modules 预置文件 */

#endif
