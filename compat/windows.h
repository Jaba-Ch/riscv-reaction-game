/*
 * compat/windows.h - minimal <windows.h> replacement for Linux and macOS.
 * Provides Sleep(), LARGE_INTEGER, QueryPerformanceFrequency() and
 * QueryPerformanceCounter() on top of POSIX clock_gettime() so that the
 * original Windows source compiles unchanged. Not used on Windows (the real
 * <windows.h> is picked up there).
 */
#ifndef COMPAT_WINDOWS_H
#define COMPAT_WINDOWS_H

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

typedef union {
    struct { uint32_t LowPart; int32_t HighPart; } u;
    long long QuadPart;
} LARGE_INTEGER;

typedef unsigned long DWORD;
typedef int BOOL;

/* Counter ticks are nanoseconds. */
static BOOL QueryPerformanceFrequency(LARGE_INTEGER *f) {
    f->QuadPart = 1000000000LL;
    return 1;
}

static BOOL QueryPerformanceCounter(LARGE_INTEGER *c) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    c->QuadPart = (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    return 1;
}

/* Flush first so prompts without a trailing newline appear before the wait. */
static void Sleep(DWORD ms) {
    fflush(stdout);
    usleep((useconds_t)ms * 1000u);
}

#endif /* COMPAT_WINDOWS_H */
