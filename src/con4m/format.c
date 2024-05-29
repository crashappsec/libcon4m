#include "con4m.h"

// This function is a little overly defensive on bounds, since the caller
// does validate the presence of { and }.
static void
parse_one_format_spec(c4m_utf32_t *s, c4m_fmt_info_t *cur)
{
    int             pos  = cur->start;
    c4m_fmt_spec_t *info = &cur->spec;

    // The actual Python-style format specifier is much easier to
    // parse backward. So once we get to the colon, that is what we
    // will do.

    memset(info, 0, sizeof(c4m_fmt_spec_t));
    int              saved_position;
    int64_t          l = c4m_str_codepoint_len(s);
    c4m_codepoint_t *p = (c4m_codepoint_t *)s->data;
    c4m_codepoint_t  c;

    c = p[pos++];

    // Assumes we've advanced past a leading '{'
    if (c == '}') {
        info->empty = 1;
        return;
    }

    if (c == ':') {
        goto at_colon;
    }

    if (c >= '0' && c <= '9') {
        __uint128_t last = 0;
        __uint128_t n    = 0;
        n                = c - '0';

        while (pos < l) {
            last = n;
            c    = p[pos];

            if (c < '0' || c > '9') {
                break;
            }
            n *= 10;
            n += c - '0';

            if (((int64_t)n) < (int64_t)last) {
                C4M_CRAISE("Integer overflow for argument number.");
            }
        }
        cur->reference.position = (int64_t)n;
        info->kind              = C4M_FMT_NUMBERED;
    }
    else {
        if (!c4m_codepoint_is_c4m_id_start(c)) {
            C4M_CRAISE("Invalid start for format specifier.");
        }
        while (pos < l && c4m_codepoint_is_c4m_id_continue(p[pos])) {
            pos++;
        }
        info->type = C4M_FMT_NAMED;
    }

    if (pos >= l) {
        C4M_CRAISE("Missing } to end format specifier.");
    }
    if (p[pos] == '}') {
        info->empty = 1;
        return;
    }

    if (p[pos++] != ':') {
        C4M_CRAISE("Expected ':' for format info or '}' here");
    }

at_colon:
    saved_position = pos - 1;

    while (true) {
        if (pos == l) {
            C4M_CRAISE("Missing } to end format specifier.");
        }

        c = p[pos];

        if (c == '}') {
            break;
        }

        pos++;
    }

    // Meaning, we got {whatever:}
    if (saved_position + 1 == pos) {
        return;
    }

    c = p[--pos];

    switch (c) {
        //clang-format off
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case ' ':
    case ',':
    case '_':
    case '^':
        break;
    default:
        info->type = p[pos];
        c          = p[--pos];
        // clang-format on
    }

    if (c >= '0' && c <= '9') {
        int         multiplier = 10;
        __uint128_t last       = 0;
        __uint128_t n          = c - '0';

        while (pos != saved_position) {
            c = p[--pos];
            if (c < '0' || c > '9') {
                break;
            }

            last = n;

            n          = n + multiplier * (c - '0');
            multiplier = multiplier * 10;

            if (((int64_t)n) < (int64_t)last) {
                C4M_CRAISE("Integer overflow for argument number.");
            }
        }

        if (c == '.') {
            info->precision = (int64_t)n;
            pos--;
            if (pos == saved_position) {
                return;
            }
        }
        else {
            info->width = (int64_t)n;
            if (pos == saved_position) {
                return;
            }
            goto look_for_sign;
        }
    }

    switch (p[pos]) {
    case '_':
        info->sep = C4M_FMT_SEP_USCORE;
        --pos;
        break;
    case ',':
        info->sep = C4M_FMT_SEP_COMMA;
        --pos;
        break;
    default:
        break;
    }

    if (pos == saved_position) {
        return;
    }

    c = p[pos];

    if (c >= '0' && c <= '9') {
        int         multiplier = 10;
        __uint128_t last       = 0;
        __uint128_t n          = c - '0';

        while (pos != saved_position) {
            c = p[--pos];
            if (c < '0' || c > '9') {
                break;
            }

            last = n;

            n          = n + multiplier * (c - '0');
            multiplier = multiplier * 10;

            if (((int64_t)n) < (int64_t)last) {
                C4M_CRAISE("Integer overflow for argument number.");
            }
        }
        info->width = n;
    }

look_for_sign:
    if (pos == saved_position) {
        return;
    }

    switch (p[pos]) {
    case '+':
        info->sign = C4M_FMT_SIGN_DEFAULT;
        pos--;
        break;
    case '-':
        info->sign = C4M_FMT_SIGN_ALWAYS;
        pos--;
        break;
    case ' ':
        info->sign = C4M_FMT_SIGN_POS_SPACE;
        pos--;
        break;
    default:
        break;
    }

    if (pos == saved_position) {
        return;
    }

    switch (p[pos]) {
    case '<':
        info->align = C4M_FMT_ALIGN_LEFT;
        break;
    case '>':
        info->align = C4M_FMT_ALIGN_RIGHT;
        break;
    case '^':
        info->align = C4M_FMT_ALIGN_CENTER;
        break;
    default:
        C4M_CRAISE("Invalid format specifier.");
    }

    pos--;

    if (pos == saved_position) {
        return;
    }

    info->fill = p[pos--];

    if (pos != saved_position) {
        C4M_CRAISE("Invalid format specifier.");
    }
}

c4m_fmt_info_t *
c4m_extract_format_specifiers(c4m_str_t *fmt)
{
    c4m_fmt_info_t  *top  = NULL;
    c4m_fmt_info_t  *last = NULL;
    c4m_fmt_info_t  *cur  = NULL;
    c4m_codepoint_t *p;
    int              l;
    int              n;

    fmt = c4m_to_utf32(fmt);
    p   = (c4m_codepoint_t *)fmt->data;
    l   = c4m_str_codepoint_len(fmt);

    for (int i = 0; i < l; i++) {
        switch (p[i]) {
        case '\\':
            i++; // Skip whatever is next;
            continue;
        case '{':
            n = i + 1;
            while (n < l) {
                if (p[n] == '}') {
                    break;
                }
                else {
                    n++;
                }
            }
            if (n == l) {
                C4M_CRAISE("Missing } to end format specifier.");
            }
            cur        = c4m_gc_alloc(c4m_fmt_info_t);
            cur->start = i + 1;
            cur->end   = n;

            if (last == NULL) {
                top = last = cur;
            }
            else {
                last->next = cur;
                last       = cur;
            }
            parse_one_format_spec(fmt, cur);
        }
    }

    return top;
}

static inline c4m_utf8_t *
apply_padding_and_alignment(c4m_utf8_t *instr, c4m_fmt_spec_t *spec)
{
    int64_t tofill = spec->width - c4m_str_codepoint_len(instr);

    if (tofill <= 0) {
        return instr;
    }

    c4m_utf8_t     *pad;
    c4m_utf8_t     *outstr;
    c4m_codepoint_t cp;
    int             len;

    if (spec->fill != 0) {
        cp = spec->fill;
    }
    else {
        cp = ' ';
    }

    switch (spec->align) {
    case C4M_FMT_ALIGN_CENTER:
        len    = tofill >> 1;
        pad    = c4m_utf8_repeat(cp, len);
        outstr = c4m_str_concat(pad, instr);
        outstr = c4m_str_concat(outstr, pad);
        if (tofill & 1) {
            pad    = c4m_utf8_repeat(cp, 1);
            outstr = c4m_str_concat(outstr, pad);
        }

        return outstr;
    case C4M_FMT_ALIGN_RIGHT:
        pad = c4m_utf8_repeat(cp, tofill);
        return c4m_str_concat(pad, instr);
    default:
        pad = c4m_utf8_repeat(cp, tofill);
        return c4m_str_concat(instr, pad);
    }
}

static inline c4m_xlist_t *
lookup_arg_strings(c4m_fmt_info_t *specs, c4m_dict_t *args)
{
    c4m_xlist_t    *result = c4m_new(c4m_tspec_xlist(c4m_tspec_utf8()));
    c4m_fmt_info_t *info   = specs;
    int             i      = 0;
    c4m_utf8_t     *key;
    c4m_obj_t       obj;

    while (info != NULL) {
        switch (info->spec.kind) {
        case C4M_FMT_FMT_ONLY:
            key = c4m_str_from_int(i);
            break;
        case C4M_FMT_NUMBERED:
            key = c4m_str_from_int(info->reference.position);
            break;
        default:
            key = c4m_new(c4m_tspec_utf8(),
                          c4m_kw("cstring", c4m_ka(info->reference.name)));
            break;
        }

        i++;

        bool found = false;
        obj        = hatrack_dict_get(args, key, &found);

        if (!found) {
            c4m_utf8_t *err = c4m_new(
                c4m_tspec_utf8(),
                c4m_kw("cstring", c4m_ka("Format parameter not found: ")));
            C4M_RAISE(c4m_str_concat(err, key));
        }
        c4m_vtable_t *vtable = c4m_vtable(obj);
        c4m_format_fn fn     = (c4m_format_fn)vtable->methods[C4M_BI_FORMAT];
        c4m_utf8_t   *s;

        if (fn != NULL) {
            s = c4m_to_utf8(fn(obj, &info->spec));
        }
        else {
            s = c4m_to_utf8(
                c4m_repr(obj, c4m_object_type(obj), C4M_REPR_VALUE));
        }

        s = apply_padding_and_alignment(s, &info->spec);

        c4m_xlist_append(result, s);

        info = info->next;
    }

    return result;
}

static inline void
style_adjustment(c4m_utf32_t *s, int64_t start, int64_t offset)
{
    c4m_style_info_t *styles = s->styling;

    if (styles == NULL) {
        return;
    }

    for (int64_t i = 0; i < styles->num_entries; i++) {
        if (styles->styles[i].end <= start) {
            continue;
        }

        if (styles->styles[i].start > start) {
            styles->styles[i].start += offset;
        }

        styles->styles[i].end += offset;
    }
}

static inline c4m_utf8_t *
assemble_formatted_result(c4m_str_t *fmt, c4m_xlist_t *arg_strings)
{
    // We need to re-parse the {}'s, and also copy stuff (including
    // style info) into the final string.

    fmt = c4m_to_utf32(fmt);

    int64_t          to_alloc = c4m_str_codepoint_len(fmt);
    int64_t          list_len = c4m_xlist_len(arg_strings);
    int64_t          arg_ix   = 0;
    int64_t          out_ix   = 0;
    int64_t          alen; // length of one argument being substituted in.
    int64_t          fmt_len = c4m_str_codepoint_len(fmt);
    c4m_codepoint_t *fmtp    = (c4m_codepoint_t *)fmt->data;
    c4m_codepoint_t *outp;
    c4m_codepoint_t *argp;
    c4m_codepoint_t  cp;
    c4m_str_t       *result;
    c4m_utf8_t      *arg;

    for (int64_t i = 0; i < list_len; i++) {
        c4m_str_t *s = (c4m_str_t *)c4m_xlist_get(arg_strings, i, NULL);
        to_alloc += c4m_str_codepoint_len(s);
    }

    result = c4m_new(c4m_tspec_utf32(),
                     c4m_kw("length", c4m_ka(to_alloc)));
    outp   = (c4m_codepoint_t *)result->data;

    c4m_copy_style_info(fmt, result);

    for (int64_t i = 0; i < fmt_len; i++) {
        // This is not yet handling hex or unicode etc in format strings.
        switch (fmtp[i]) {
        case '\\':
            style_adjustment(result, out_ix, -1);
            cp = fmtp[++i];
            switch (cp) {
            case 'n':
                outp[out_ix++] = '\n';
                break;
            case 't':
                outp[out_ix++] = '\t';
                break;
            case 'f':
                outp[out_ix++] = '\f';
                break;
            case 'v':
                outp[out_ix++] = '\v';
                break;
            default:
                outp[out_ix++] = cp;
                break;
            }
            continue;
        case '}':
            continue;
        case '{':
            // For now, we will not copy over styles from the format call.
            // Might do that later.
            arg  = c4m_xlist_get(arg_strings, arg_ix++, NULL);
            alen = c4m_str_codepoint_len(arg);
            arg  = c4m_to_utf32(arg);
            argp = (c4m_codepoint_t *)arg->data;

            // If we have a null string and there's a style around it,
            // things are broken without this kludge. But to be quite
            // honest, I don't understand why it's broken or why this
            // kludge works??
            if (alen == 0) {
                alen = 1;
            }

            style_adjustment(result, out_ix, alen - 2);

            for (int64_t j = 0; j < alen; j++) {
                outp[out_ix++] = argp[j];
            }

            i++; // Skip the } too.
            continue;
        default:
            outp[out_ix++] = fmtp[i];
            continue;
        }
    }

    result->codepoints = ~out_ix;
    return c4m_to_utf8(result);
}

c4m_utf8_t *
c4m_str_vformat(c4m_str_t *fmt, c4m_dict_t *args)
{
    // Positional items are looked up via their ASCII string.
    // Keys are expected to be utf8.
    //
    // Note that the input fmt string is treated as a rich literal,
    // meaning that if there is attached style info it will be 100%
    // stripped.
    c4m_fmt_info_t *info = c4m_extract_format_specifiers(fmt);

    if (info == NULL) {
        return c4m_rich_lit(c4m_to_utf8(fmt)->data);
    }

    // We're going to create a version of the format string where all
    // the parameters are replaced with just {}; we are going to then
    // pass this to c4m_rich_lit to do any formatting we've been asked
    // to do, before we do object substitutions.
    c4m_xlist_t *segments = c4m_new(c4m_tspec_xlist(c4m_tspec_utf32()));

    int             cur_pos = 0;
    c4m_fmt_info_t *cur_arg = info;

    while (cur_arg != NULL) {
        c4m_utf32_t *s = c4m_str_slice(fmt, cur_pos, cur_arg->start);
        c4m_xlist_append(segments, s);
        cur_pos = cur_arg->end;
        cur_arg = cur_arg->next;
    }

    if (cur_pos != 0) {
        int l = c4m_str_codepoint_len(fmt);

        if (cur_pos != l) {
            c4m_utf32_t *s = c4m_str_slice(fmt, cur_pos, l);
            c4m_xlist_append(segments, s);
        }

        fmt = c4m_str_join(segments, c4m_empty_string());
    }

    // After this point, we will potentially have a formatted string,
    // so the locations for the {} may not be where we might compute
    // them to be, so we will just reparse them.
    fmt                      = c4m_rich_lit(c4m_to_utf8(fmt)->data);
    c4m_xlist_t *arg_strings = lookup_arg_strings(info, args);

    return assemble_formatted_result(fmt, arg_strings);
}

c4m_utf8_t *
c4m_base_format(c4m_str_t *fmt, int nargs, va_list args)
{
    c4m_obj_t   one;
    c4m_dict_t *dict = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                              c4m_tspec_ref()));

    for (int i = 0; i < nargs; i++) {
        one             = va_arg(args, c4m_obj_t);
        c4m_utf8_t *key = c4m_str_from_int(i);
        hatrack_dict_add(dict, key, one);
    }

    return c4m_str_vformat(fmt, dict);
}

c4m_utf8_t *
_c4m_str_format(c4m_str_t *fmt, int nargs, ...)
{
    va_list     args;
    c4m_utf8_t *result;

    va_start(args, nargs);
    result = c4m_base_format(fmt, nargs, args);
    va_end(args);

    return result;
}

c4m_utf8_t *
_c4m_cstr_format(char *fmt, int nargs, ...)
{
    va_list     args;
    c4m_utf8_t *result;

    va_start(args, nargs);
    result = c4m_base_format(c4m_new_utf8(fmt), nargs, args);
    va_end(args);

    return result;
}

c4m_utf8_t *
c4m_cstr_array_format(char *fmt, int num_args, c4m_utf8_t **params)
{
    c4m_utf8_t *one;
    c4m_dict_t *dict = c4m_new(c4m_tspec_dict(c4m_tspec_utf8(),
                                              c4m_tspec_ref()));

    for (int i = 0; i < num_args; i++) {
        one             = params[i];
        c4m_utf8_t *key = c4m_str_from_int(i);
        hatrack_dict_add(dict, key, one);
    }

    return c4m_str_vformat(c4m_new_utf8(fmt), dict);
}
