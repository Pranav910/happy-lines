# Compiler and flags
CC = gcc
CFLAGS =
TARGET = main

# Default target: compile the executable
all: $(TARGET)

# Compile the executable
$(TARGET): happy-lines.c
	$(CC) $(CFLAGS) -o $(TARGET) happy-lines.c

# Run the executable after compiling
run: $(TARGET)
	./$(TARGET)

# Clean up compiled files
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all run clean
