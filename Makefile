# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude

# Directories
SRC_DIR = src
INC_DIR = include
BIN_DIR = bin
OBJ_DIR = build

# Files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Executables
BIN_MANAGER = $(BIN_DIR)/fss_manager
BIN_CONSOLE = $(BIN_DIR)/fss_console
BIN_WORKER = $(BIN_DIR)/fss_worker

# Default rule
all: $(BIN_DIR) $(OBJ_DIR) $(BIN_MANAGER) $(BIN_CONSOLE) $(BIN_WORKER)

# Create bin and build dirs if not existing
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Object file compilation
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Linking binaries
$(BIN_MANAGER): $(OBJ_DIR)/fss_manager.o $(OBJ_DIR)/LinkedList.o $(OBJ_DIR)/Queue.o
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_CONSOLE): $(OBJ_DIR)/fss_console.o
	$(CC) $(CFLAGS) -o $@ $<

# $(BIN_WORKER): $(OBJ_DIR)/worker.o
# 	$(CC) $(CFLAGS) -o $@ $<

$(BIN_WORKER): $(SRC_DIR)/fss_worker.c
	gcc -Wall -Wextra -o $@ $<


# Clean rule
clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)


# Ensure log directory exists
LOG_DIR = log_files

$(LOG_DIR):
	mkdir -p $(LOG_DIR)

# Run the manager
run_manager: $(BIN_MANAGER) $(LOG_DIR)
	./$(BIN_MANAGER) -l $(LOG_DIR)/manager_log.txt -c config_file.txt

# Run the console
run_console: $(BIN_CONSOLE) $(LOG_DIR)
	./$(BIN_CONSOLE) -l $(LOG_DIR)/console_log.txt


.PHONY: all clean
