#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

#define MAX_FILES 256
#define MAX_FILENAME 64
#define MAX_FILE_SIZE 8192
#define MAX_PATH 256
#define MAX_DIRS 32

#define FS_FLAG_READONLY  0x01
#define FS_FLAG_HIDDEN    0x02
#define FS_FLAG_SYSTEM    0x04
#define FS_FLAG_EXECUTABLE 0x08

typedef enum { FS_FILE, FS_DIRECTORY } fs_type_t;

typedef struct {
    char name[MAX_FILENAME];
    char data[MAX_FILE_SIZE];
    int size;
    int in_use;
    fs_type_t type;
    int parent_dir;
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
int get_user_dir();
int fs_get_node(int idx, char* name, int* size, int* type, int* parent, uint8_t* flags, uint32_t* mod_time);
int fs_get_current_dir();
int fs_set_flags(const char* filename, uint8_t flags);

// New expanded API
int fs_get_home_dir();
int fs_get_trash_dir();
int fs_get_desktop_dir();
int fs_trash_file(const char* filename);
int fs_empty_trash();
int fs_restore_from_trash(const char* filename);
int fs_get_free_count();
int fs_copy_file(const char* src_name, const char* dst_name);
int fs_copy_file_to_dir(int src_dir, const char* src_name, int dst_dir, const char* dst_name);
int fs_get_dir_count(int dir_idx);
int fs_find_child(int dir_idx, int child_idx);
void fs_get_permission_string(uint8_t flags, char* out);
void fs_format_time(uint32_t ms, char* out, int max_len);
int fs_is_directory_empty(int dir_idx);
int fs_find_by_index(int dir_idx, int nth, char* name, int* size, int* type, uint8_t* flags, uint32_t* mod_time);

#endif
