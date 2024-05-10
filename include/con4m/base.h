#pragma once

// Base includes from the system and any dependencies should go here.

#include <stdint.h>
#include <stddef.h>
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
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <math.h>
#include <setjmp.h>
#include <netdb.h>
#include <dlfcn.h>
#include <pwd.h>

#include <sys/select.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#if defined(__linux__)
#include <sys/random.h>
#include <threads.h>
#include <endian.h>
#endif

#if defined(__MACH__)
#include <machine/endian.h>
#endif

#ifdef HAVE_MUSL
#include <bits/limits.h>
#endif

#ifdef HAVE_PTY_H
#include <pty.h>
#else
extern pid_t
forkpty(int *, char *, struct termios *, struct winsize *);
#endif

#define min(a, b) ({ __typeof__ (a) _a = (a), _b = (b); \
                    _a < _b ? _a : _b; })
#define max(a, b) ({ __typeof__ (a) _a = (a), _b = (b); \
                    _a > _b ? _a : _b; })

#include "vendor.h"
#include "hatrack.h"

typedef struct hatrack_dict_t c4m_dict_t;
typedef struct hatrack_set_st c4m_set_t;

#include "con4m/datatypes.h"

#if BYTE_ORDER == LITTLE_ENDIAN
#define little_64(x)
#define little_32(x)
#define little_16(x)
#elif BYTE_ORDER == BIG_ENDIAN
#if defined(linux)
#define little_64(x) x = htole64(x)
#define little_32(x) x = htole32(x)
#define little_16(x) x = htole16(x)
#else
#define little_64(x) x = htonll(x)
#define little_32(x) x = htonl(x)
#define little_16(x) x = htons(x)
#endif
#else
#error unknown endian
#endif
