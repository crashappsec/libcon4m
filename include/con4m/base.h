#pragma once

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <limits.h>
#include <signal.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#ifdef HAVE_MUSL
#include <bits/limits.h>
#endif

#ifdef HAVE_PTY_H
#include <pty.h>
#else
extern pid_t
forkpty(int *, char *, struct termios *, struct winsize *);
#endif

static inline void *
zalloc(size_t len)
{
    return calloc(len, 1);
}
enum {
    CALLER    = 0x00,
    CALLEE_P1 = 0x01,
    CALLEE_P2 = 0x02,
    CALLEE_P3 = 0x04,
    CALLEE_P4 = 0x08,
    CALLEE_P5 = 0x10,
    CALLEE_P6 = 0x20,
    CALLEE_P7 = 0x40,
    CALLEE_P8 = 0x80
};

typedef char ownership_t;

#define min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); \
                    _a < _b ? _a : _b; })
#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); \
                    _a > _b ? _a : _b; })
