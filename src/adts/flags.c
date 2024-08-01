#include "con4m.h"

// Note: This is not yet multi-thread safe. My intent is to
// do this with a pthread_mutex object at some point once we
// actually start adding threading support.

static void
flags_init(c4m_flags_t *self, va_list args)
{
    int64_t length = -1;

    c4m_karg_va_init(args);
    c4m_kw_int64("length", length);

    self->num_flags = (uint32_t)length;

    // Always allocate at least a word, even if there's no length provided.
    if (length < 0) {
        length = 64;
    }

    self->bit_modulus   = length % 64;
    self->alloc_wordlen = (length + 63) / 64;

    self->contents = c4m_gc_raw_alloc(sizeof(uint64_t) * self->alloc_wordlen,
                                      NULL);
}

c4m_flags_t *
c4m_flags_copy(const c4m_flags_t *self)
{
    c4m_flags_t *result = c4m_new(c4m_type_flags(),
                                  c4m_kw("length", c4m_ka(self->num_flags)));

    for (int i = 0; i < result->alloc_wordlen; i++) {
        result->contents[i] = self->contents[i];
    }

    return result;
}

c4m_flags_t *
c4m_flags_invert(c4m_flags_t *self)
{
    c4m_flags_t *result = c4m_flags_copy(self);

    for (int i = 0; i < result->alloc_wordlen; i++) {
        result->contents[i] = ~self->contents[i];
    }

    if (!result->bit_modulus) {
        return result;
    }

    int num_to_clear = 64 - result->bit_modulus;
    uint64_t mask    = (1ULL << num_to_clear) - 1;


    result->contents[result->alloc_wordlen - 1] &= mask;

    return result;
}

static c4m_utf8_t *
flags_repr(const c4m_flags_t *self)
{
    C4M_STATIC_ASCII_STR(prefix, "0x{");
    C4M_STATIC_ASCII_STR(fmt_cons, ":{}x}");
    c4m_utf8_t *result;
    int         n = self->alloc_wordlen;

    if (self->bit_modulus) {
        c4m_utf8_t *fmt = c4m_str_format(fmt_cons, (self->bit_modulus + 3) / 4);
        fmt             = c4m_to_utf8(c4m_str_concat(prefix, fmt));
        result          = c4m_str_format(fmt, c4m_box_u64(self->contents[--n]));
    }
    else {
        result = c4m_new_utf8("0x");
    }

    while (n--) {
        result = c4m_cstr_format("{}{:x}",
                                 result,
                                 c4m_box_u64(self->contents[n]));
    }

    return result;
}

static void
flags_marshal(const c4m_flags_t *self,
              c4m_stream_t      *out,
              c4m_dict_t        *memos,
              int64_t           *mid)
{
    c4m_marshal_i32(self->bit_modulus, out);
    c4m_marshal_i32(self->alloc_wordlen, out);
    for (int i = 0; i < self->alloc_wordlen; i++) {
        c4m_marshal_u64(self->contents[i], out);
    }
}

static void
flags_unmarshal(c4m_flags_t *self, c4m_stream_t *in, c4m_dict_t *memos)
{
    self->bit_modulus   = c4m_unmarshal_i32(in);
    self->alloc_wordlen = c4m_unmarshal_i32(in);

    self->contents = c4m_gc_raw_alloc(sizeof(uint64_t) * self->alloc_wordlen,
                                      NULL);

    for (int i = 0; i < self->alloc_wordlen; i++) {
        self->contents[i] = c4m_unmarshal_u64(in);
    }
}

static c4m_flags_t *
flags_lit(const c4m_utf8_t    *s,
          c4m_lit_syntax_t     st,
          c4m_utf8_t          *m,
          c4m_compile_error_t *code)
{
    int64_t      len    = c4m_str_codepoint_len(s);
    c4m_flags_t *result = c4m_new(c4m_type_flags(),
                                  c4m_kw("length", c4m_ka(len * 4)));

    uint64_t cur_word = 0;
    int      ct       = 0;
    int      alloc_ix = 0;

    while (--len) {
        cur_word <<= 4;
        ct++;

        switch (s->data[--len]) {
        case '0':
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            cur_word |= (s->data[len] - '0');
            break;
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            cur_word |= (s->data[len] - 'a' + 10);
            break;
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            cur_word |= (s->data[len] - 'A' + 10);
            break;
        default:
            *code = c4m_err_parse_lit_bad_flags;
            return NULL;
        }
        if (!(ct % 16)) {
            result->contents[alloc_ix++] = cur_word;
            cur_word                     = 0;
        }
    }

    if (ct % 16) {
        result->contents[alloc_ix] = cur_word;
    }

    return result;
}

c4m_flags_t *
c4m_flags_add(c4m_flags_t *self, c4m_flags_t *with)
{
    uint32_t     res_flags   = c4m_max(self->num_flags, with->num_flags);
    c4m_flags_t *result      = c4m_new(c4m_type_flags(),
                                  c4m_kw("length", c4m_ka(res_flags)));
    int32_t      min_wordlen = c4m_min(self->alloc_wordlen, with->alloc_wordlen);

    for (int i = 0; i < min_wordlen; i++) {
        result->contents[i] = self->contents[i] | with->contents[i];
    }

    if (self->alloc_wordlen > with->alloc_wordlen) {
        for (int i = min_wordlen; i < result->alloc_wordlen; i++) {
            result->contents[i] = self->contents[i];
        }
    }
    else {
        // If they're equal, this loop won't run.
        for (int i = min_wordlen; i < result->alloc_wordlen; i++) {
            result->contents[i] = with->contents[i];
        }
    }

    return result;
}

c4m_flags_t *
c4m_flags_sub(c4m_flags_t *self, c4m_flags_t *with)
{
    // If one flag set is longer, we auto-expand the result, even if
    // it's the rhs, as it implies the lhs has unset flags, but
    // they're valid.

    uint32_t     res_flags   = c4m_max(self->num_flags, with->num_flags);
    c4m_flags_t *result      = c4m_new(c4m_type_flags(),
                                  c4m_kw("length", c4m_ka(res_flags)));
    int32_t      min_wordlen = c4m_min(self->alloc_wordlen, with->alloc_wordlen);
    int          i;

    for (i = 0; i < min_wordlen; i++) {
        result->contents[i] = self->contents[i] & ~with->contents[i];
    }

    if (self->alloc_wordlen > with->alloc_wordlen) {
        for (; i < result->alloc_wordlen; i++) {
            result->contents[i] = self->contents[i];
        }
    }

    return result;
}

c4m_flags_t *
c4m_flags_test(c4m_flags_t *self, c4m_flags_t *with)
{
    // If one flag set is longer, we auto-expand the result, even if
    // it's the rhs, as it implies the lhs has unset flags, but
    // they're valid.

    uint32_t     res_flags   = c4m_max(self->num_flags, with->num_flags);
    c4m_flags_t *result      = c4m_new(c4m_type_flags(),
                                  c4m_kw("length", c4m_ka(res_flags)));
    int32_t      min_wordlen = c4m_min(self->alloc_wordlen, with->alloc_wordlen);

    for (int i = 0; i < min_wordlen; i++) {
        result->contents[i] = self->contents[i] & with->contents[i];
    }

    return result;
}

c4m_flags_t *
c4m_flags_xor(c4m_flags_t *self, c4m_flags_t *with)
{
    uint32_t     res_flags   = c4m_max(self->num_flags, with->num_flags);
    c4m_flags_t *result      = c4m_new(c4m_type_flags(),
                                  c4m_kw("length", c4m_ka(res_flags)));
    int32_t      min_wordlen = c4m_min(self->alloc_wordlen, with->alloc_wordlen);

    for (int i = 0; i < min_wordlen; i++) {
        result->contents[i] = self->contents[i] ^ with->contents[i];
    }

    if (self->alloc_wordlen > with->alloc_wordlen) {
        for (int i = min_wordlen; i < result->alloc_wordlen; i++) {
            result->contents[i] = self->contents[i];
        }
    }
    else {
        for (int i = min_wordlen; i < result->alloc_wordlen; i++) {
            result->contents[i] = with->contents[i];
        }
    }

    return result;
}

bool
c4m_flags_eq(c4m_flags_t *self, c4m_flags_t *other)
{
    // We do this in as close to constant time as possible to avoid
    // any side channels.

    uint64_t     sum = 0;
    int          i;
    int32_t      nlow  = c4m_min(self->alloc_wordlen, other->alloc_wordlen);
    int32_t      nhigh = c4m_max(self->alloc_wordlen, other->alloc_wordlen);
    c4m_flags_t *high_ptr;

    if (self->alloc_wordlen == nhigh) {
        high_ptr = self;
    }
    else {
        high_ptr = other;
    }

    for (i = 0; i < nlow; i++) {
        sum += self->contents[i] ^ other->contents[i];
    }

    for (; i < nhigh; i++) {
        sum += high_ptr->contents[i];
    }

    return sum == 0;
}

uint64_t
c4m_flags_len(c4m_flags_t *self)
{
    return self->num_flags;
}

static void
resize_flags(c4m_flags_t *self, uint64_t new_num)
{
    int32_t new_words   = new_num / 64;
    int32_t new_modulus = new_num % 64;

    if (new_words < self->alloc_wordlen) {
        uint64_t *contents = c4m_gc_raw_alloc(sizeof(uint64_t) * new_words,
                                              NULL);

        for (int i = 0; i < self->alloc_wordlen; i++) {
            contents[i] = self->contents[i];
        }

        self->contents = contents;
    }

    self->bit_modulus   = new_modulus;
    self->alloc_wordlen = new_words;
    self->num_flags     = new_num;
}

bool
c4m_flags_index(c4m_flags_t *self, int64_t n)
{
    if (n < 0) {
        n += self->num_flags;

        if (n < 0) {
            C4M_CRAISE("Negative index is out of bounds.");
        }
    }

    if (n >= self->num_flags) {
        resize_flags(self, n);
        // It will never be set.
        return false;
    }

    uint32_t word_ix = n / 64;
    uint32_t offset  = n % 64;
    uint64_t flag    = 1UL << offset;

    return (self->contents[word_ix] & flag) != 0;
}

void
c4m_flags_set_index(c4m_flags_t *self, int64_t n, bool value)
{
    if (n < 0) {
        n += self->num_flags;

        if (n < 0) {
            C4M_CRAISE("Negative index is out of bounds.");
        }
    }

    if (n >= self->num_flags) {
        resize_flags(self, n);
    }

    uint32_t word_ix = n / 64;
    uint32_t offset  = n % 64;
    uint64_t flag    = 1UL << offset;

    if (value) {
        self->contents[word_ix] |= flag;
    }
    else {
        self->contents[word_ix] &= ~flag;
    }
}

#if 0
// DO THESE LATER; NOT IMPORTANT RIGHT NOW.
c4m_flags_t *
c4m_flags_slice(c4m_flags_t *self, int64_t start, int64_t end)
{
    if (start < 0) {
        start += len;
    }
    else {
        if (start >= len) {
            return c4m_new(c4m_type_flags());
        }
    }
    if (end < 0) {
        end += len + 1;
    }
    else {
        if (end > len) {
            end = len;
        }
    }
    if ((start | end) < 0 || start >= end) {
        return c4m_new(c4m_type_flags());
    }
}
#endif

static inline c4m_type_t *
flags_item_type(c4m_obj_t ignore)
{
    return c4m_type_bit();
}

// For iterating over other container types, we just take a pointer to
// memory and a length, which is what we get from hatrack's views.
//
// Because indexing into a bit field is a bit more complicated
// (advancing a pointer one bit doesn't make sense), we avoid
// duplicating the code in the code generator, and just accept that
// views based on data types with single-bit items are going to use
// the index interface.

static inline void *
flags_view(c4m_flags_t *self, uint64_t *n)
{
    c4m_flags_t *copy = c4m_flags_copy(self);
    *n                = copy->num_flags;

    return copy;
}

const c4m_vtable_t c4m_flags_vtable = {
    .num_entries = C4M_BI_NUM_FUNCS,
    .methods     = {
        [C4M_BI_CONSTRUCTOR]  = (c4m_vtable_entry)flags_init,
        [C4M_BI_TO_STR]       = (c4m_vtable_entry)flags_repr,
        [C4M_BI_MARSHAL]      = (c4m_vtable_entry)flags_marshal,
        [C4M_BI_UNMARSHAL]    = (c4m_vtable_entry)flags_unmarshal,
        [C4M_BI_FROM_LITERAL] = (c4m_vtable_entry)flags_lit,
        [C4M_BI_COPY]         = (c4m_vtable_entry)c4m_flags_copy,
        [C4M_BI_ADD]          = (c4m_vtable_entry)c4m_flags_add,
        [C4M_BI_SUB]          = (c4m_vtable_entry)c4m_flags_sub,
        [C4M_BI_EQ]           = (c4m_vtable_entry)c4m_flags_eq, // EQ
        [C4M_BI_LEN]          = (c4m_vtable_entry)c4m_flags_len,
        [C4M_BI_INDEX_GET]    = (c4m_vtable_entry)c4m_flags_index,
        [C4M_BI_INDEX_SET]    = (c4m_vtable_entry)c4m_flags_set_index,
        [C4M_BI_ITEM_TYPE]    = (c4m_vtable_entry)flags_item_type,
        [C4M_BI_VIEW]         = (c4m_vtable_entry)flags_view,
#if 0
        [C4M_BI_SLICE_GET]    = (c4m_vtable_entry)c4m_flags_slice,
        [C4M_BI_SLICE_SET]    = (c4m_vtable_entry)c4m_flags_set_slice,
#endif
        // Explicit because some compilers don't seem to always properly
        // zero it (Was sometimes crashing on a `c4m_stream_t` on my mac).
        [C4M_BI_FINALIZER] = NULL,
    },
};
