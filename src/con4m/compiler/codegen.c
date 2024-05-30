#define C4M_USE_INTERNAL_API
#include "con4m.h"

typedef struct {
    c4m_compile_ctx      *cctx;
    c4m_file_compile_ctx *fctx;
} gen_ctx;

static void
gen_module_code(gen_ctx *ctx)
{
}

void
c4m_codegen(c4m_compile_ctx *cctx)
{
    gen_ctx ctx = {
        .cctx = cctx,
    };

    int n = c4m_xlist_len(cctx->module_ordering);

    for (int i = 0; i < n; i++) {
        ctx.fctx = c4m_xlist_get(cctx->module_ordering, i, NULL);

        if (ctx.fctx->status >= c4m_compile_status_generated_code) {
            return;
        }

        if (c4m_fatal_error_in_module(ctx.fctx)) {
            C4M_CRAISE("Cannot generate code for files with fatal errors.");
        }

        if (ctx.fctx->status >= c4m_compile_status_generated_code) {
            continue;
        }
        gen_module_code(&ctx);

        ctx.fctx->status = c4m_compile_status_generated_code;
    }
}
