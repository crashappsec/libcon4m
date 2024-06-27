#include "con4m.h"

#ifdef C4M_DEBUG

static c4m_watch_target c4m_watch_list[C4M_WATCH_SLOTS];
static c4m_watch_log_t  c4m_watch_log[C4M_WATCH_LOG_SZ];
static _Atomic int      next_log_id     = 0;
static _Atomic int      max_target      = 0;
static __thread bool    recursion_guard = false;

static void
watch_init()
{
    static int inited = false;

    if (!inited) {
        inited = true;
        for (int i = 0; i < C4M_WATCH_SLOTS; i++) {
            c4m_gc_register_root(&c4m_watch_list[i].address, 1);
        }
    }
}

void
_c4m_watch_set(void            *ptr,
               int              ix,
               c4m_watch_action ra,
               c4m_watch_action wa,
               bool             print,
               char            *file,
               int              line)
{
    watch_init();

    if (ix < 0 || ix >= C4M_WATCH_SLOTS) {
        abort();
    }

    uint64_t iv = *(uint64_t *)ptr;

    c4m_watch_list[ix] = (c4m_watch_target){
        .address      = ptr,
        .value        = iv,
        .read_action  = ra,
        .write_action = wa,
    };

    int high = atomic_read(&max_target);
    while (ix > high) {
        CAS(&max_target, &high, ix);
    }

    if (print) {
        c4m_printf("[atomic lime]Added watchpoint[/] @{:x} {}:{} (value: {:x})",
                   c4m_box_u64((uint64_t)ptr),
                   c4m_new_utf8(file),
                   c4m_box_u64(line),
                   c4m_box_u64(iv));
    }
}

static void
watch_print_log(int logid, int watchix)
{
    c4m_watch_log_t  entry = c4m_watch_log[logid % C4M_WATCH_LOG_SZ];
    c4m_watch_target t     = c4m_watch_list[watchix];

    if (entry.logid != logid) {
        c4m_printf(
            "[yellow]watch:{}:Warning: [/] log was dropped before it "
            "was reported; watch log size is too small.",
            c4m_box_u64(logid));
        return;
    }

    if (!entry.changed) {
        c4m_printf("[atomic lime]{}:@{:x}: Unchanged[/] {}:{} (value: {:x})",
                   c4m_box_u64(logid),
                   c4m_box_u64((uint64_t)t.address),
                   c4m_new_utf8(entry.file),
                   c4m_box_u64(entry.line),
                   c4m_box_u64(entry.seen_value));
        return;
    }

    // If you get a crash inside the GC, comment out the printf instead.
    // printf("%p vs %p\n", entry.start_value, entry.seen_value);
    c4m_printf("[red]{}:@{:x}: Changed @{}:{} from [em]{:x}[/] to [em]{:x}",
               c4m_box_u64(logid),
               c4m_box_u64((uint64_t)t.address),
               c4m_new_utf8(entry.file),
               c4m_box_u64(entry.line),
               c4m_box_u64(entry.start_value),
               c4m_box_u64(entry.seen_value));
    return;
}

void
_c4m_watch_scan(char *file, int line)
{
    // Because we are using dynamic allocation when reporting, and
    // because we may want to stick this check inside the allocator as
    // a frequent place to look for watches, we want to prevent starting
    // one scan recursively if we already have one in progress.

    if (recursion_guard) {
        return;
    }
    else {
        recursion_guard = true;
    }

    int max = atomic_read(&max_target);
    int logid;
    int log_slot;

    for (int i = 0; i < max; i++) {
        c4m_watch_target t = c4m_watch_list[i];

        // If you want to ignore when it changes, you're turning the
        // whole watchpoint off; doesn't make much sense to
        // report on reads but ignore writes.
        if (!t.address || t.write_action == c4m_wa_ignore) {
            continue;
        }

        uint64_t cur     = *(uint64_t *)t.address;
        bool     changed = cur != t.value;

        logid                   = atomic_fetch_add(&next_log_id, 1);
        log_slot                = logid % C4M_WATCH_LOG_SZ;
        c4m_watch_log[log_slot] = (c4m_watch_log_t){
            .file        = file,
            .line        = line,
            .start_value = t.value,
            .seen_value  = cur,
            .logid       = logid,
            .changed     = changed,
        };

        switch (changed ? t.write_action : t.read_action) {
        case c4m_wa_abort:
            watch_print_log(logid, i);
            c4m_printf("[red]Aborting for debugging.");
            abort();
        case c4m_wa_print:
            watch_print_log(logid, i);
            break;
        default:
            break;
        }

        if (changed) {
            t.value = cur;
        }
    }

    recursion_guard = false;
}

#endif
