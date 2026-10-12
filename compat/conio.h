/*
 * compat/conio.h - minimal <conio.h> replacement for Linux and macOS.
 * Provides _kbhit() and _getch() on top of termios so that the original
 * Windows source compiles unchanged. Not used on Windows (the real
 * <conio.h> is picked up there).
 */
#ifndef COMPAT_CONIO_H
#define COMPAT_CONIO_H

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>

static struct termios compat_saved_termios;
static int compat_raw_enabled = 0;

static void compat_restore_terminal(void) {
    if (compat_raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSANOW, &compat_saved_termios);
        compat_raw_enabled = 0;
    }
}

/* Switch the terminal to raw, non-echoing mode on first use. */
static void compat_enable_raw(void) {
    struct termios raw;
    if (compat_raw_enabled) return;
    if (tcgetattr(STDIN_FILENO, &compat_saved_termios) != 0) return;
    raw = compat_saved_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
        compat_raw_enabled = 1;
        atexit(compat_restore_terminal);
    }
}

/* Non-zero if a key press is waiting. */
static int _kbhit(void) {
    fd_set fds;
    struct timeval tv = {0, 0};
    compat_enable_raw();
    fflush(stdout);
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

/* Read one key press without echo. */
static int _getch(void) {
    unsigned char c;
    compat_enable_raw();
    return (read(STDIN_FILENO, &c, 1) == 1) ? (int)c : -1;
}

#endif /* COMPAT_CONIO_H */
