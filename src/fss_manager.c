#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/LinkedList.h"
#include "../include/Queue.h"

#define MAX_PATH 1024
#define BUF_SIZE (1024 * (sizeof(struct inotify_event) + MAX_PATH + 1))
#define PIPE_IN "fss_in"
#define PIPE_OUT "fss_out"

typedef struct {
    char src[MAX_PATH];
    char tgt[MAX_PATH];
} Pair;

typedef struct {
    char src[MAX_PATH];
    char tgt[MAX_PATH];
    int active;
    int err_count;
    char last_sync[32];
    int wd;
} DirInfo;

// Global variables
static List sync_info_mem_store;
static Queue workers_queue;
static volatile sig_atomic_t shutdown = 0;
static volatile sig_atomic_t workers_active = 0;
static int max_workers = 5;
static int inotify_fd = -1;
static FILE* log_file = NULL;

void timestamp(char* buf) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buf, 32, "[%Y-%m-%d %H:%M:%S]", tm_info);
}

int compare_sync_dir(Pointer a, Pointer b) {
    DirInfo* dir = (DirInfo*)a;
     char* src = ( char*)b;
    return strcmp(dir->src, src);
}

DirInfo* find_sync_dir( char* src) {
    return (DirInfo*)list_find(sync_info_mem_store, (Pointer)src, compare_sync_dir);
}

void add_sync_dir( char* src,  char* tgt) {
    DirInfo* new_dir = malloc(sizeof(DirInfo));
    strcpy(new_dir->src, src);
    strcpy(new_dir->tgt, tgt);
    new_dir->active = 1;
    new_dir->err_count = 0;

    timestamp(new_dir->last_sync);

    new_dir->wd = inotify_add_watch(inotify_fd, src, IN_CREATE | IN_MODIFY | IN_DELETE);
    if (new_dir->wd == -1) {
        perror("inotify_add_watch failed");
    }

    list_insert_next(sync_info_mem_store, LIST_BOF, new_dir);
}

void remove_sync_dir( char* src) {
    ListNode node = list_find_node(sync_info_mem_store, (Pointer)src, compare_sync_dir);
    if (node != LIST_EOF) {
        DirInfo* dir = list_node_value(sync_info_mem_store, node);
        inotify_rm_watch(inotify_fd, dir->wd);
        list_remove_next(sync_info_mem_store, list_previous(sync_info_mem_store, node));
    }
}




void send_msg( char* msg) {
    int fd = open(PIPE_OUT, O_WRONLY);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    }
}

void log_file_and_console(int fd, char* msg) {
    
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
    }
    fprintf(log_file, "%s", msg);
    fflush(log_file);
}




void handle_int(int sig) {
    (void)sig;
    shutdown = 1;
}

void handle_child(int sig) {
    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
        workers_active--;

        //If there are workers waiting and the number of active workers is less than the limit then start the next in queue worker
        while (workers_active < max_workers && queue_size(workers_queue) > 0) {
            Pair* pair = queue_front(workers_queue);
            pid_t pid = fork();
            if (pid == 0) {
                execl("./bin/fss_worker", "./bin/fss_worker", pair->src, pair->tgt, "ALL", "FULL", (char*)NULL);
                perror("execl error");
                exit(1);
            } else if (pid > 0) {
                workers_active++;
                queue_remove_front(workers_queue);
            }else{
                perror("fork error");
            }
        }
    }
}


// Function to start a worker process for full synchronization of a directory
void start_worker( char* src,  char* tgt) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        execl("./bin/fss_worker", "./bin/fss_worker", src, tgt, "ALL", "FULL", (char*)NULL);
        perror("execl error");
        exit(1);
    } else {
        workers_active++;
        char time_buf[32];
        timestamp(time_buf);
        fprintf(log_file, "%s Added directory: %s -> %s\n", time_buf, src, tgt);
        fprintf(log_file, "%s Monitoring started for %s\n", time_buf, src);
        fflush(log_file);
    }
}


void process_command(char* buffer) {

    char source[MAX_PATH];
    char target[MAX_PATH];
    char response[4096];
    char time_buffer[32];
 
     if (strncmp(buffer, "add ", 4) == 0) {
         sscanf(buffer + 4, "%s %s", source, target);
         if (find_sync_dir(source)) {
            int fd = open(PIPE_OUT, O_WRONLY);
            log_file_and_console(fd, "Already in queue.\n");
            close(fd);
            return;
         }
         add_sync_dir(source, target);
         if (workers_active < max_workers){
             start_worker(source, target);
         }else {
             Pair* pair = malloc(sizeof(Pair));
             strcpy(pair->src, source);
             strcpy(pair->tgt, target);
             queue_insert_back(workers_queue, pair);
         }
         snprintf(response, sizeof(response), "Added directory: %s -> %s\nMonitoring started for: %s\n", source, target, source);
         int fd = open(PIPE_OUT, O_WRONLY);
         log_file_and_console(fd, response);
         close(fd);
 
     }else if (strncmp(buffer, "cancel ", 7) == 0) {
         sscanf(buffer + 7, "%s", source);
         if (find_sync_dir(source)) {
             DirInfo* dir = find_sync_dir(source);
             inotify_rm_watch(inotify_fd, dir->wd);
             dir->active = 0;
 
            char response[4096];
             snprintf(response, sizeof(response), "Monitoring stopped for %s\n", source);
             int fd = open(PIPE_OUT, O_WRONLY);
             log_file_and_console(fd, response);
             close(fd);
         } else {
            int fd = open(PIPE_OUT, O_WRONLY);
             log_file_and_console(fd, "Directory not monitored.\n");
             close(fd);
         }
     
     }else if (strncmp(buffer, "status ", 7) == 0) {
        char src[MAX_PATH];
         sscanf(buffer + 7, "%s", source);
         DirInfo* dir = find_sync_dir(source);
        char response[4096];
         if (dir) {
            char* act;
             if (dir->active){
                 act = "Active";
             } else {
                 act = "Inactive";   
             }
             snprintf(response, sizeof(response),
                 "Directory: %.512s\nTarget: %.512s\nLast Sync: %.64s\nErrors: %d\nStatus: %s\n", dir->src, dir->tgt, dir->last_sync, dir->err_count, act);
 
 
 
         } else {
             snprintf(response, sizeof(response), "Directory not monitored: %.512s\n", src);
 
         }
         int fd = open(PIPE_OUT, O_WRONLY);
         log_file_and_console(fd, response);
         close(fd);
     
     }else if (strncmp(buffer, "shutdown", 8) == 0) {
         char now[32];
         timestamp(now);
         int fd = open(PIPE_OUT, O_WRONLY);
         printf("%s Shutting down manager...\n", now);
         log_file_and_console(fd, "Shutting down manager...\n");       
 
         printf("%s Waiting for all active workers to finish.\n", now);
         log_file_and_console(fd, "Waiting for all active workers to finish.\n");       
 
         printf("%s Processing remaining queued tasks.\n", now);
         log_file_and_console(fd, "Processing remaining queued tasks.\n");       
         close(fd);
         
         while (queue_size(workers_queue) > 0) {
             // Pair* pair = (Pair*)queue_front(workers_queue);
             queue_remove_front(workers_queue);
         }
 
         fd = open(PIPE_OUT, O_WRONLY);
         printf("%s Manager shutdown complete.\n", now);
         log_file_and_console(fd, "Manager shutdown complete.\n");
         close(fd);
         shutdown = 1;
     
     }else if (strncmp(buffer, "sync ", 5) == 0) {
         sscanf(buffer + 5, "%s", source);
 
         DirInfo* dir = find_sync_dir(source);
         if (!dir) {
             timestamp(time_buffer);
 
 
             snprintf(response, sizeof(response), "Directory not monitored: %.512s\n", source);
             int fd = open(PIPE_OUT, O_WRONLY);
             log_file_and_console(fd, response);
             close(fd);
             
             return;
         }
 
         if (dir->active) {
             timestamp(time_buffer);
             snprintf(response, sizeof(response), "%s Sync already in progress %s\n", time_buffer, source);
             int fd = open(PIPE_OUT, O_WRONLY);
             log_file_and_console(fd, response);
             close(fd);
 
 
             return;
         }
 
         if (workers_active >= max_workers) {
             Pair* pair = malloc(sizeof(Pair));
             strcpy(pair->src, dir->src);
             strcpy(pair->tgt, dir->tgt);
             queue_insert_back(workers_queue, pair);
 
             timestamp(time_buffer);
             snprintf(response, sizeof(response), "%s Sync queued for %s\n", time_buffer, source);
             int fd = open(PIPE_OUT, O_WRONLY);
             log_file_and_console(fd, response);
             close(fd);

             return;
         }
 
 
         pid_t pid = fork();
         if (pid == -1) {
             perror("fork");
             int fd = open(PIPE_OUT, O_WRONLY);
             log_file_and_console(fd, "Fork error.\n");
             close(fd);
             return;
         }
         if (pid == 0) {
             execl("./bin/fss_worker", "./bin/fss_worker", dir->src, dir->tgt, "ALL", "FULL", (char*)NULL);
             perror("execl error");
             exit(1);
         }
 
         dir->wd = inotify_add_watch(inotify_fd, dir->src, IN_CREATE | IN_MODIFY | IN_DELETE);
 
 
         workers_active++;
         dir->active = 1;
 
 
         timestamp(time_buffer);
         fprintf(log_file, "%s Syncing directory: %s -> %s\n", time_buffer, dir->src, dir->tgt);
 
 
         fflush(log_file);
 
         snprintf(response, sizeof(response), "%s Syncing directory: %.512s -> %.512s\n", time_buffer, dir->src, dir->tgt);
         int fd = open(PIPE_OUT, O_WRONLY);
         log_file_and_console(fd, response);
         close(fd);
 
 
     }
 }

void create_pipes() {
    // Remove existing pipes
    unlink(PIPE_IN);
    unlink(PIPE_OUT);

    // If mkfifo fails and error does not exist print error and exit
    if (mkfifo(PIPE_IN, 0666) < 0 && errno != EEXIST) {  
        perror("mkfifo in");
        exit(1);
    }
    if (mkfifo(PIPE_OUT, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo out");
        exit(1);
    }
}

void get_args(int argc, char *argv[], char **log_path, char **config_path, int *max_workers){
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
            *log_path = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            *config_path = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            *max_workers = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Usage: %s -l <logfile> -c <configfile> -n workers-limit\n", argv[0]);
            exit(1);
        }
    }

    if (!*log_path || !*config_path) {
        fprintf(stderr, "-l and -c arguments missing\n");
        exit(1);
    }
}


void handle_inotify_event(struct inotify_event* event) {
    if (event->len == 0)
        return;

    for (ListNode node = list_first(sync_info_mem_store); node != LIST_EOF; node = list_next(sync_info_mem_store, node)) {
        DirInfo* dir = list_node_value(sync_info_mem_store, node);
        if (dir->wd == event->wd) {
             char* operation = NULL;
            if (event->mask & IN_CREATE)
                operation = "ADDED";
            else if (event->mask & IN_MODIFY)
                operation = "MODIFIED";
            else if (event->mask & IN_DELETE)
                operation = "DELETED";

            if (operation && workers_active < max_workers) {
                pid_t pid = fork();
                if (pid == 0) {
                    execl("./bin/fss_worker", "./bin/fss_worker", dir->src, dir->tgt, event->name, operation, (char*)NULL);
                    perror("execl failed");
                    exit(1);
                } else if (pid > 0) {
                    workers_active++;
                }
            }
            break;
        }
    }
}

void process_inotify_events() {
    char event_buf[BUF_SIZE];
    int length = read(inotify_fd, event_buf, BUF_SIZE);
    if (length <= 0)
        return;

    int i = 0;
    while (i < length) {
        struct inotify_event* event = (struct inotify_event*)&event_buf[i];
        handle_inotify_event(event);
        i += sizeof(struct inotify_event) + event->len;
    }
}


int main(int argc, char* argv[]) {
    char* log_path = NULL;
    char* config_path = NULL;

    get_args(argc, argv, &log_path, &config_path, &max_workers);

    log_file = fopen(log_path, "a");
    if (!log_file) {
        perror("open logfile");
        exit(1);
    }

    FILE* config_file = fopen(config_path, "r");
    if (!config_file) {
        perror("open configfile");
        exit(1);
    }

    create_pipes();

    signal(SIGINT, handle_int);
    signal(SIGCHLD, handle_child);

    // Create the list that will hold the sync directories and other useful info about them
    sync_info_mem_store = list_create(free);

    // Create the queue that will hold the workers that are waiting to be started
    workers_queue = queue_create(free);

    // Initialize inotify 
    inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd == -1) {
        perror("inotify_init1 failed");
        exit(1);
    }

    char dirs_pair[MAX_PATH * 2];
    while (fgets(dirs_pair, sizeof(dirs_pair), config_file)) {
        char source[MAX_PATH], target[MAX_PATH];
        if (sscanf(dirs_pair, "%s %s", source, target) == 2) {
            add_sync_dir(source, target);
            if (workers_active < max_workers)
                start_worker(source, target);
            else {
                Pair* pair = malloc(sizeof(Pair));
                strcpy(pair->src, source);
                strcpy(pair->tgt, target);
                queue_insert_back(workers_queue, pair);
            }
        }
    }
    fclose(config_file);

    int read_pipe = open(PIPE_IN, O_RDONLY);
    if (read_pipe < 0) {
        perror("open fss_in");
        exit(1);
    }

    struct pollfd fds[2];
    fds[0].fd = read_pipe;
    fds[0].events = POLLIN;
    fds[1].fd = inotify_fd;
    fds[1].events = POLLIN;

    char buffer[1024];

    while (!shutdown) {
        int poll_res = poll(fds, 2, -1);
        if (poll_res < 0) {
            if (errno == EINTR){
                continue;
            }
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN) {
            int len = read(read_pipe, buffer, sizeof(buffer) - 1);
            if (len > 0) {
                buffer[len] = '\0';
                process_command(buffer);
            }
        }

                                   
        if (fds[1].revents & POLLIN) {
            process_inotify_events();
        }        
    }

    close(read_pipe);
    close(inotify_fd);
    fclose(log_file);
    queue_destroy(workers_queue);
    list_destroy(sync_info_mem_store);

    return 0;  
}