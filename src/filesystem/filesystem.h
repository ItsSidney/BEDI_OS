#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#define MAX_FILES 64
#define MAX_FILENAME 64
#define MAX_FILE_SIZE 4096
#define MAX_PATH 256
#define MAX_DIRS 16

typedef enum {
    FS_FILE,
    FS_DIRECTORY
} fs_type_t;

typedef struct {
    char name[MAX_FILENAME];
    char data[MAX_FILE_SIZE];
    int size;
    int in_use;
    fs_type_t type;
    int parent_dir;  // Index of parent directory, -1 for root
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
int fs_truncate(const char* filename);
int get_user_dir();  // Get index of USER directory

#endif
