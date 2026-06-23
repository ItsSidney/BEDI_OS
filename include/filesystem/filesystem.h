#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

#define MAX_FILES 256
#define MAX_FILENAME 64
#define MAX_FILE_SIZE 8192
#define MAX_PATH 256
#define MAX_DIRS 32

typedef enum {
    FS_FILE,
    FS_DIRECTORY
} fs_type_t;

// File flags
#define FS_FLAG_READONLY 0x01
#define FS_FLAG_HIDDEN   0x02
#define FS_FLAG_SYSTEM   0x04

typedef struct {
    char name[MAX_FILENAME];
    char data[MAX_FILE_SIZE];
    int size;
    int in_use;
    fs_type_t type;
    int parent_dir;  // Index of parent directory, -1 for root
    uint8_t flags;
    uint32_t creation_time;
    uint32_t modified_time;
} file_t;

// Filesystem functions
void init_filesystem();
int fs_open(const char* filename, int flags);
int fs_read(int fd, char* buf, int count);
int fs_write(int fd, const char* buf, int count);
int fs_close(int fd);
int fs_create(const char* filename);
int fs_delete(const char* filename);
int fs_exists(const char* filename);
int fs_list(char* output, int max_len);
int fs_mkdir(const char* dirname);
int fs_rmdir(const char* dirname);
int fs_cd(const char* dirname);
int fs_pwd(char* output, int max_len);
int fs_cat(const char* filename, char* output, int max_len);
int fs_touch(const char* filename);
int fs_rename(const char* oldname, const char* newname);
int fs_move(const char* src, const char* dst);
int fs_truncate(const char* filename);
int get_user_dir();  // Get index of USER directory
int fs_get_node(int idx, char* name, int* size, int* type, int* parent, uint8_t* flags, uint32_t* mod_time);
int fs_get_current_dir();
int fs_set_flags(const char* filename, uint8_t flags);
#endif
