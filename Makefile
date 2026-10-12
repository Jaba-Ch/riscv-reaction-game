# Linux / macOS build. On Windows use: gcc risc-v.c -o reaction_game.exe
CC      ?= gcc
CFLAGS  ?= -O2 -Wall
TARGET  = reaction_game

$(TARGET): risc-v.c compat/conio.h compat/windows.h
	$(CC) $(CFLAGS) -Icompat risc-v.c -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
