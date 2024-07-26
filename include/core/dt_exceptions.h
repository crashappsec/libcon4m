#pragma once
#include "con4m.h"

typedef struct c4m_exception_st       c4m_exception_t;
typedef struct c4m_exception_frame_st c4m_exception_frame_t;

struct c4m_exception_st {
    c4m_utf8_t      *msg;
    c4m_obj_t       *context;
    c4m_grid_t      *c_trace;
    c4m_exception_t *previous;
    int64_t          code;
    const char      *file;
    uint64_t         line;
};

struct c4m_exception_frame_st {
    jmp_buf               *buf;
    c4m_exception_t       *exception;
    c4m_exception_frame_t *next;
};

typedef struct {
    c4m_grid_t            *c_trace;
    c4m_exception_frame_t *top;
    c4m_exception_frame_t *free_frames;
} c4m_exception_stack_t;
