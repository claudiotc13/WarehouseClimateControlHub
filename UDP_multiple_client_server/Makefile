# Makefile for compiling server.c with cJSON

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -pthread

# Source files
SRCS = main.c cJSON.c mqtt_handler.c

# Output executable
TARGET = main

# Default rule
all: $(TARGET)

# Link the target executable
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -lpthread -o $(TARGET) $(SRCS) -lpaho-mqtt3cs

# Clean rule to remove the executable
clean:
	rm -f $(TARGET)
