# Makefile for Concurrent Hash Table Assignment

# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g # -pthread links the pthread library

# Target executable name
TARGET = chash

# Source files
SRCS = main.c hash.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target: builds the executable
all: $(TARGET)

# Rule to link the object files into the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(CFLAGS)

# Rule to compile .c files into .o files
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

# Phony targets
.PHONY: all clean run

# Clean up build files and the log file
# FIX: Using 'cmd /c del /q' to robustly delete files in Windows environments.
clean:
	-cmd /c del /q $(TARGET).exe $(OBJS) output.txt hash.log >nul 2>&1
	# Keep this fallback for Unix-like shells
	-rm -f $(TARGET) $(OBJS) output.txt hash.log
	