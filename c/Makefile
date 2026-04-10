CC = "C:/Program Files/JetBrains/CLion 2025.3.2/bin/mingw/bin/gcc.exe"
CFLAGS = -Wall -Wextra -std=c99

SRCS = main.c chunk.c memory.c debug.c value.c vm.c scanner.c compiler.c object.c table.c
OBJS = $(SRCS:.c=.o)
TARGET = clox.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm -f *.o $(TARGET)