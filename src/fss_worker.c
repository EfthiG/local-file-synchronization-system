#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>

#define BUFFER_SIZE 4096
#define MAX_ERROR_LOG_SIZE 8192
#define MAX_PATH 4096

int files_copied = 0;
int files_skipped = 0;
char error_log[MAX_ERROR_LOG_SIZE] = {0};



void record_error(const char *path, const char *message) {
    char line[512];
    snprintf(line, sizeof(line), "- File %s: %s\n", path, message);

    if (strlen(error_log) + strlen(line) < MAX_ERROR_LOG_SIZE) {
        strcat(error_log, line);
    }
}



int ensure_directory_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            record_error(path, strerror(errno));
            return -1;
        }
    }
    return 0;
}




int copy_file(const char *source_path, const char *destination_path) {
    int src_fd = open(source_path, O_RDONLY);
    if (src_fd < 0) {
        record_error(source_path, strerror(errno));
        files_skipped++;
        return -1;
    }

    int dst_fd = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        record_error(destination_path, strerror(errno));
        close(src_fd);
        files_skipped++;
        return -1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes;
    while ((bytes = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dst_fd, buffer, bytes) != bytes) {
            record_error(destination_path, "Failed to write data.");
            close(src_fd);
            close(dst_fd);
            files_skipped++;
            return -1;
        }
    }

    if (bytes < 0) {
        record_error(source_path, "Failed to read file.");
        files_skipped++;
    } else {
        files_copied++;
    }

    close(src_fd);
    close(dst_fd);
    return 0;
}




void sync_entire_directory(const char *src_dir, const char *dst_dir) {
    if (ensure_directory_exists(dst_dir) != 0) return;

    DIR *dir = opendir(src_dir);
    if (!dir) {
        record_error(src_dir, strerror(errno));
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char src_path[MAX_PATH], dst_path[MAX_PATH];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        struct stat st;
        if (stat(src_path, &st) == 0 && S_ISREG(st.st_mode)) {
            copy_file(src_path, dst_path);
        }
    }

    closedir(dir);
}



void handle_single_file(const char *src_dir, const char *dst_dir, const char *file_name, const char *action) {
    if (ensure_directory_exists(dst_dir) != 0) return;

    char src_path[MAX_PATH], dst_path[MAX_PATH];
    snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, file_name);
    snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, file_name);

    if (strcmp(action, "ADDED") == 0 || strcmp(action, "MODIFIED") == 0) {
        copy_file(src_path, dst_path);
    } 
    else if (strcmp(action, "DELETED") == 0) {
        struct stat st;
        if (stat(dst_path, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                if (unlink(dst_path) == 0) {
                    files_copied++;
                } else {
                    record_error(dst_path, strerror(errno));
                    files_skipped++;
                }
            }
            else if (S_ISDIR(st.st_mode)) {
                if (rmdir(dst_path) == 0) {
                    files_copied++;
                } else {
                    record_error(dst_path, strerror(errno));
                    files_skipped++;
                }
            } else {
                record_error(dst_path, "Cannot delete this type of file.");
                files_skipped++;
            }
        } else {
            record_error(dst_path, "File does not exist.");
            files_skipped++;
        }
    }
}

void print_execution_report(const char *action, const char *file_name) {
    printf("EXEC_REPORT_START\n");

    if (files_skipped == 0 && strlen(error_log) == 0) {
        printf("STATUS: SUCCESS\n");
    } else if (files_copied > 0) {
        printf("STATUS: PARTIAL\n");
    } else {
        printf("STATUS: ERROR\n");
    }

    if (strcmp(file_name, "ALL") == 0 && strcmp(action, "FULL") == 0) {
        printf("DETAILS: %d files copied, %d skipped\n", files_copied, files_skipped);
    } else if (strcmp(action, "DELETED") == 0) {
        printf("DETAILS: File: %s deleted\n", file_name);
    } else {
        printf("DETAILS: File: %s processed\n", file_name);
    }

    if (strlen(error_log) > 0) {
        printf("ERRORS:\n%s", error_log);
    }

    printf("EXEC_REPORT_END\n");
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <source_dir> <target_dir> <filename> <operation>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *source = argv[1];
    const char *target = argv[2];
    const char *filename = argv[3];
    const char *operation = argv[4];

    if (strcmp(filename, "ALL") == 0 && strcmp(operation, "FULL") == 0) {
        sync_entire_directory(source, target);
    } else {
        handle_single_file(source, target, filename, operation);
    }

    print_execution_report(operation, filename);
    return EXIT_SUCCESS;
}