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

#define min(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); \
                    _a < _b ? _a : _b; })
#define max(a,b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); \
                    _a > _b ? _a : _b; })
