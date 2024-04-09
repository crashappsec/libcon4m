#pragma once
#include <con4m.h>

typedef struct exception_st       exception_t;
typedef struct exception_frame_st exception_frame_t;

struct exception_st {
    utf8_t      *msg;
    object_t    *context;
    exception_t *previous;
    int64_t      code;
    const char  *file;
    uint64_t     line;
};

struct exception_frame_st {
    jmp_buf           *buf;
    exception_t       *exception;
    exception_frame_t *next;
};

typedef struct {
    exception_frame_t *top;
    exception_frame_t *free_frames;
} exception_stack_t;
