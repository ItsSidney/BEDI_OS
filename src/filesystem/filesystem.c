#include "filesystem.h"

static file_t files[MAX_FILES];
static int current_dir = -1;  // -1 means root
static int user_dir_index = -1;  // Index of USER directory

// strcmp is defined in commands.c, declare it here
extern int strcmp(const char* s1, const char* s2);

int get_user_dir() {
    return user_dir_index;
}

void init_filesystem() {
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].in_use = 0;
        files[i].size = 0;
        files[i].name[0] = 0;
        files[i].parent_dir = -1;
        files[i].type = FS_FILE;
    }
    
    // Create ROOT directory (virtual, always exists at index -1)
    // Create USER directory in root
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].in_use) {
            files[i].in_use = 1;
            files[i].size = 0;
            files[i].type = FS_DIRECTORY;
            files[i].parent_dir = -1;  // Parent is root
            files[i].name[0] = 'U';
            files[i].name[1] = 'S';
            files[i].name[2] = 'E';
            files[i].name[3] = 'R';
            files[i].name[4] = 0;
            user_dir_index = i;
            current_dir = i;  // Start in USER directory
            break;
        }
    }
}

int fs_exists(const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].in_use && files[i].parent_dir == current_dir) {
            int match = 1;
            int j = 0;
            while (filename[j] != 0 && files[i].name[j] != 0) {
                if (filename[j] != files[i].name[j]) {
                    match = 0;
                    break;
                }
                j++;
            }
            if (match && filename[j] == 0 && files[i].name[j] == 0) {
                return 1;
            }
        }
    }
    return 0;
}

int fs_create(const char* filename) {
    if (fs_exists(filename)) {
        return -1; // File already exists
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].in_use) {
            files[i].in_use = 1;
            files[i].size = 0;
            files[i].type = FS_FILE;
            files[i].parent_dir = current_dir;
            int j = 0;
            while (filename[j] != 0 && j < MAX_FILENAME - 1) {
                files[i].name[j] = filename[j];
                j++;
            }
            files[i].name[j] = 0;
            return i; // Return file descriptor
        }
    }
    return -1; // No free file slots
}

int fs_touch(const char* filename) {
    return fs_create(filename);
}

int fs_delete(const char* filename) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].in_use && files[i].parent_dir == current_dir) {
            int match = 1;
            int j = 0;
            while (filename[j] != 0 && files[i].name[j] != 0) {
                if (filename[j] != files[i].name[j]) {
                    match = 0;
                    break;
                }
                j++;
            }
            if (match && filename[j] == 0 && files[i].name[j] == 0) {
                files[i].in_use = 0;
                files[i].size = 0;
                files[i].name[0] = 0;
                return 0; // Success
            }
        }
    }
    return -1; // File not found
}

int fs_open(const char* filename, int flags) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].in_use && files[i].parent_dir == current_dir) {
            int match = 1;
            int j = 0;
            while (filename[j] != 0 && files[i].name[j] != 0) {
                if (filename[j] != files[i].name[j]) {
                    match = 0;
                    break;
                }
                j++;
            }
            if (match && filename[j] == 0 && files[i].name[j] == 0) {
                return i; // Return file descriptor
            }
        }
    }
    return -1; // File not found
}

int fs_truncate(const char* filename) {
    int fd = fs_open(filename, 0);
    if (fd >= 0) {
        files[fd].size = 0;
        return 0;
    }
    return -1;
}

int fs_read(int fd, char* buf, int count) {
    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) {
        return -1;
    }
    
    int bytes_read = 0;
    for (int i = 0; i < count && i < files[fd].size; i++) {
        buf[i] = files[fd].data[i];
        bytes_read++;
    }
    return bytes_read;
}

int fs_cat(const char* filename, char* output, int max_len) {
    int fd = fs_open(filename, 0);
    if (fd < 0) return -1;
    
    int bytes = fs_read(fd, output, max_len - 1);
    if (bytes > 0) {
        output[bytes] = 0;
    }
    fs_close(fd);
    return bytes;
}

int fs_write(int fd, const char* buf, int count) {
    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) {
        return -1;
    }
    
    int bytes_written = 0;
    for (int i = 0; i < count && files[fd].size < MAX_FILE_SIZE; i++) {
        files[fd].data[files[fd].size] = buf[i];
        files[fd].size++;
        bytes_written++;
    }
    return bytes_written;
}

int fs_close(int fd) {
    if (fd < 0 || fd >= MAX_FILES || !files[fd].in_use) {
        return -1;
    }
    return 0;
}

int fs_list(char* output, int max_len) {
    int pos = 0;
    
    // Show ".." to go to parent directory (unless at root)
    if (current_dir != -1) {
        int len = 2;
        if (pos + len < max_len - 1) {
            output[pos++] = '.';
            output[pos++] = '.';
            output[pos++] = '\n';
        }
    }
    
    // Show ROOT if we're in USER directory
    if (current_dir == user_dir_index) {
        int len = 4;
        if (pos + len < max_len - 1) {
            output[pos++] = 'R';
            output[pos++] = 'O';
            output[pos++] = 'O';
            output[pos++] = 'T';
            output[pos++] = '\n';
        }
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].in_use && files[i].parent_dir == current_dir) {
            int j = 0;
            while (files[i].name[j] != 0 && pos < max_len - 1) {
                output[pos++] = files[i].name[j++];
            }
            if (pos < max_len - 1) {
                if (files[i].type == FS_DIRECTORY) {
                    output[pos++] = '/';
                }
                output[pos++] = '\n';
            }
        }
    }
    if (pos < max_len) {
        output[pos] = 0;
    }
    return pos;
}

int fs_mkdir(const char* dirname) {
    if (fs_exists(dirname)) {
        return -1; // Directory already exists
    }
    
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].in_use) {
            files[i].in_use = 1;
            files[i].size = 0;
            files[i].type = FS_DIRECTORY;
            files[i].parent_dir = current_dir;
            int j = 0;
            while (dirname[j] != 0 && j < MAX_FILENAME - 1) {
                files[i].name[j] = dirname[j];
                j++;
            }
            files[i].name[j] = 0;
            return 0; // Success
        }
    }
    return -1; // No free slots
}

int fs_rmdir(const char* dirname) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].in_use && files[i].parent_dir == current_dir && files[i].type == FS_DIRECTORY) {
            int match = 1;
            int j = 0;
            while (dirname[j] != 0 && files[i].name[j] != 0) {
                if (dirname[j] != files[i].name[j]) {
                    match = 0;
                    break;
                }
                j++;
            }
            if (match && dirname[j] == 0 && files[i].name[j] == 0) {
                // Check if directory is empty
                for (int k = 0; k < MAX_FILES; k++) {
                    if (files[k].in_use && files[k].parent_dir == i) {
                        return -2; // Directory not empty
                    }
                }
                files[i].in_use = 0;
                files[i].size = 0;
                files[i].name[0] = 0;
                return 0; // Success
            }
        }
    }
    return -1; // Directory not found
}

int fs_cd(const char* dirname) {
    if (strcmp(dirname, "..") == 0) {
        if (current_dir != -1) {
            int parent = files[current_dir].parent_dir;
            if (parent == -1) {
                // Already at root, stay there
                current_dir = -1;
            } else {
                current_dir = parent;
            }
        }
        return 0;
    }
    
    if (strcmp(dirname, "/") == 0) {
        current_dir = -1;
        return 0;
    }
    
    // Handle absolute paths (starting with /)
    if (dirname[0] == '/') {
        int dir = -1;  // Start at root
        int pos = 1;   // Skip leading /
        
        while (dirname[pos] != 0) {
            // Find next path component
            char component[MAX_FILENAME];
            int comp_pos = 0;
            
            while (dirname[pos] != 0 && dirname[pos] != '/') {
                if (comp_pos < MAX_FILENAME - 1) {
                    component[comp_pos++] = dirname[pos++];
                } else {
                    pos++;
                }
            }
            component[comp_pos] = 0;
            
            // Skip trailing /
            if (dirname[pos] == '/') pos++;
            
            // Skip empty components
            if (comp_pos == 0) continue;
            
            // Find the directory
            int found = 0;
            for (int i = 0; i < MAX_FILES; i++) {
                if (files[i].in_use && files[i].parent_dir == dir && files[i].type == FS_DIRECTORY) {
                    int match = 1;
                    int j = 0;
                    while (component[j] != 0 && files[i].name[j] != 0) {
                        if (component[j] != files[i].name[j]) {
                            match = 0;
                            break;
                        }
                        j++;
                    }
                    if (match && component[j] == 0 && files[i].name[j] == 0) {
                        dir = i;
                        found = 1;
                        break;
                    }
                }
            }
            
            if (!found) return -1;  // Directory not found
        }
        
        current_dir = dir;
        return 0;
    }
    
    // Special case: cd ROOT from USER directory
    if (strcmp(dirname, "ROOT") == 0 && current_dir == user_dir_index) {
        current_dir = -1;
        return 0;
    }
    
    // Relative path from current directory
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].in_use && files[i].parent_dir == current_dir && files[i].type == FS_DIRECTORY) {
            int match = 1;
            int j = 0;
            while (dirname[j] != 0 && files[i].name[j] != 0) {
                if (dirname[j] != files[i].name[j]) {
                    match = 0;
                    break;
                }
                j++;
            }
            if (match && dirname[j] == 0 && files[i].name[j] == 0) {
                current_dir = i;
                return 0; // Success
            }
        }
    }
    return -1; // Directory not found
}

int fs_rename(const char* oldname, const char* newname) {
    if (fs_exists(newname)) return -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].in_use && files[i].parent_dir == current_dir) {
            if (strcmp(files[i].name, oldname) == 0) {
                int j = 0;
                while (newname[j] != 0 && j < MAX_FILENAME - 1) {
                    files[i].name[j] = newname[j];
                    j++;
                }
                files[i].name[j] = 0;
                return 0;
            }
        }
    }
    return -1;
}

int fs_pwd(char* output, int max_len) {
    if (current_dir == -1) {
        if (max_len > 1) {
            output[0] = '/';
            output[1] = 0;
        }
        return 1;
    }
    
    // Build path by walking up the tree
    char path[MAX_PATH];
    int pos = MAX_PATH - 1;
    path[pos--] = 0;
    
    int temp = current_dir;
    while (temp != -1) {
        int len = 0;
        while (files[temp].name[len] != 0) len++;
        
        for (int i = len - 1; i >= 0 && pos > 0; i--) {
            path[pos--] = files[temp].name[i];
        }
        if (pos > 0) path[pos--] = '/';
        
        temp = files[temp].parent_dir;
    }
    
    // Copy to output
    int out_pos = 0;
    for (int i = pos + 1; i < MAX_PATH && out_pos < max_len - 1; i++) {
        output[out_pos++] = path[i];
    }
    output[out_pos] = 0;
    
    return out_pos;
}
