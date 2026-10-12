# Local File Synchronization System

A Linux file synchronization system developed in **C** for a university **Systems Programming** assignment. The project monitors source directories for filesystem changes and synchronizes their contents with corresponding target directories. It combines Linux filesystem notifications, process management, inter-process communication, and a command-line management interface.

## Overview

The system is organized around three executables:

- **`fss_manager`** — maintains monitored source/target directory pairs, watches for changes with Linux `inotify`, schedules synchronization work, and logs activity.
- **`fss_worker`** — performs full-directory or individual-file synchronization using filesystem operations.
- **`fss_console`** — provides an interactive interface for sending commands to the manager and receiving responses through named pipes.

The manager starts worker processes using `fork`/`exec`, enforces a configurable limit on concurrent workers, and queues pending jobs when that limit is reached. Worker results are returned to the manager through pipes.

## Features

- Initial synchronization of configured directory pairs
- Monitoring for file creation, modification, and deletion with `inotify`
- On-demand synchronization through the console
- Add, inspect, and stop monitoring directory pairs
- Worker-process scheduling with a pending-job queue
- Command and synchronization logging
- Bash utility for reviewing log-based directory status and removing selected backup directories or log files

## Architecture

```text
                         +----------------+
                         |   fss_console  |
                         +--------+-------+
                                  |
                       named pipes (FIFOs)
                                  |
                         +--------v-------+
       inotify events --> |   fss_manager  | <-- configuration file
                         +--------+-------+
                                  |
                          fork / exec + queue
                                  |
                         +--------v-------+
                         |   fss_worker   |
                         +--------+-------+
                                  |
                       source -> target files
```

## Project Structure

```text
.
├── Makefile
├── README.md
├── config_file.txt
├── fss_script.sh
├── include/
│   ├── LinkedList.h
│   └── Queue.h
└── src/
    ├── LinkedList.c
    ├── Queue.c
    ├── fss_console.c
    ├── fss_manager.c
    └── fss_worker.c
```

`LinkedList` stores directory-related information, while `Queue` supports scheduling synchronization jobs. The build creates the `bin/` and `build/` directories.

## Requirements

- Linux (the program uses Linux `inotify` and POSIX process/IPC facilities)
- GCC
- GNU Make
- Bash for `fss_script.sh`

## Build

From the project root:

```bash
make
```

This builds:

```text
bin/fss_manager
bin/fss_console
bin/fss_worker
```

To remove generated build files:

```bash
make clean
```

## Configuration

Add source/target directory pairs to `config_file.txt`, one pair per line, separated by whitespace. For example:

```text
/home/user/data/source1 /home/user/data/backup1
/home/user/data/source2 /home/user/data/backup2
```

Use paths that exist and are accessible to the program. The provided configuration file is initially empty, so add your own directory pairs before starting the manager.

## Run

Run commands from the **project root**, because the manager invokes the worker through `./bin/fss_worker`.

**Terminal 1 — start the manager:**

```bash
make run_manager
```

Or run it directly and set a worker limit:

```bash
mkdir -p log_files
./bin/fss_manager -l log_files/manager_log.txt -c config_file.txt -n 5
```

**Terminal 2 — open the console:**

```bash
make run_console
```

Or:

```bash
./bin/fss_console -l log_files/console_log.txt
```

### Console Commands

| Command | Purpose |
| --- | --- |
| `add <source> <target>` | Add a source/target directory pair |
| `status <source>` | View the monitoring status of a source directory |
| `sync <source>` | Request synchronization for a source directory |
| `cancel <source>` | Stop monitoring a source directory |
| `shutdown` | Request manager shutdown |

Example:

```text
add /home/user/data/source1 /home/user/data/backup1
status /home/user/data/source1
sync /home/user/data/source1
shutdown
```

## Bash Utility

`fss_script.sh` provides additional log-based reporting and cleanup commands:

```bash
bash fss_script.sh -p log_files/manager_log.txt -c listAll
bash fss_script.sh -p log_files/manager_log.txt -c listMonitored
bash fss_script.sh -p log_files/manager_log.txt -c listStopped
bash fss_script.sh -p /path/to/backup_or_log -c purge
```

**Caution:** `purge` deletes the specified directory recursively or removes the specified file. Check the path before using it.

## Systems Programming Concepts

This assignment demonstrates:

- **Filesystem event monitoring:** Linux `inotify`
- **Process creation and execution:** `fork`, `exec`
- **Inter-process communication:** named pipes and anonymous pipes
- **Signal handling:** managing child-process completion and shutdown
- **Concurrency control:** limiting active workers and queueing work
- **Low-level file operations:** reading, writing, creating, and deleting files
- **Data structures in C:** linked lists and queues

## Academic Context

Developed as **Homework 1** for a university **Systems Programming** course. A separate second assignment explores distributed file synchronization over a network.
