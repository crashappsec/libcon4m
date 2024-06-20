/*
 * Currently, we're using select() here, not epoll(), etc.
 */
#include "con4m.h"

/* The way we use the below two IO functions assumes that, while they
 * may be interrupted, they won't be blocked.
 *
 * Therefore, we need to be sure not to:
 *
 * 1) use select() to make sure an op can be ready.
 * 2) We limit ourselves to PIPE_BUF per write.
 *
 * In the switchboard code, we will always select on any open fds that
 * have open subscribers. For writers, we will only select on their
 * fds if there are messages waiting to be written.
 *
 * We do this by having a message queue attached to readers. So in the
 * first possible select cycle, (assuming there's no string being
 * routed into a file descriptor), we will only select() for read
 * fds.
 *
 * There's no write delay though, as when we re-enter the select loop,
 * we'd expect the fds for write to all be ready for data, so that
 * select() call will return immediately.
 */
ssize_t
c4m_sb_read_one(int fd, char *buf, size_t nbytes)
{
    ssize_t n;

    while (true) {
        n = read(fd, buf, nbytes);

        if (n == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
        }

        return n;
    }
}

bool
c4m_sb_write_data(int fd, char *buf, size_t nbytes)
{
    size_t  towrite, written = 0;
    ssize_t result;

    do {
        if (nbytes - written > SSIZE_MAX) {
            towrite = SSIZE_MAX;
        }
        else {
            towrite = nbytes - written;
        }
        if ((result = write(fd, buf + written, towrite)) >= 0) {
            written += result;
        }
        else if (errno != EINTR && errno != EAGAIN) {
            return false;
        }
    } while (written < nbytes);

    return true;
}

static inline void
register_fd(c4m_switchboard_t *ctx, int fd)
{
    if (ctx->max_fd <= fd) {
        ctx->max_fd = fd + 1;
    }
}

int
c4m_sb_party_fd(c4m_party_t *party)
{
    if (party->c4m_party_type == C4M_PT_FD) {
        return party->info.fdinfo.fd;
    }
    else {
        return party->info.listenerinfo.fd;
    }
}

static inline c4m_party_instr_t *
get_sstr_obj(c4m_party_t *party)
{
    return &party->info.rstrinfo;
}

static inline c4m_party_outstr_t *
get_dstr_obj(c4m_party_t *party)
{
    return &party->info.wstrinfo;
}

static inline c4m_party_fd_t *
get_fd_obj(c4m_party_t *party)
{
    return &party->info.fdinfo;
}

static inline c4m_party_listener_t *
get_listener_obj(c4m_party_t *party)
{
    return &party->info.listenerinfo;
}

/* Here we link together readers so we can walk through them to build
 * the read FD set, and to do memory management.
 *
 * Non-FD data structures are put on the 'loners' list.
 */
static inline void
register_read_fd(c4m_switchboard_t *ctx, c4m_party_t *read_from)
{
    register_fd(ctx, c4m_sb_party_fd(read_from));

    read_from->next_reader   = ctx->parties_for_reading;
    ctx->parties_for_reading = read_from;
}

static inline void
register_writer_fd(c4m_switchboard_t *ctx, c4m_party_t *write_to)
{
    register_fd(ctx, c4m_sb_party_fd(write_to));
    write_to->next_writer    = ctx->parties_for_writing;
    ctx->parties_for_writing = write_to;
}

/* 'Loner' is a horrible name for this; it's taking the party metaphor
 * too far. This is just a list of c4m_party_t objects that will not
 * appear on either the reader linked list or the writer linked list;
 * generally we'll want this for cleanup after the fact.
 */
static inline void
register_loner(c4m_switchboard_t *ctx, c4m_party_t *loner)
{
    loner->next_loner = ctx->party_loners;
    ctx->party_loners = loner;
}

/*
 * Here, we're handed a c4m_party_t and told it's a listener. You must
 * pass in a sockfd that is already open, and has already had listen()
 * called on it.
 *
 * When there's something ready to accept(), we'll get triggered that
 * a read is available. We then accept() for you, and pass that to a
 * callback.
 *
 * You can then register the fd connection in the same switchboard if
 * you like. It's not done for you at this level, though.
 *
 * If `close_on_destroy` is true, we will call close() on the fd for
 * you whenever the switchboard is torn down.
 */
void
c4m_sb_init_party_listener(c4m_switchboard_t *ctx,
                           c4m_party_t       *party,
                           int                sockfd,
                           c4m_accept_cb_t    callback,
                           bool               stop_when_closed,
                           bool               close_on_destroy)
{
    /* Stop on close should really only be applied to stdin/out/err,
     * and socket FDs. For subprocesses, add the flag when registering
     * them.
     */
    party->c4m_party_type    = C4M_PT_LISTENER;
    party->open_for_read     = true;
    party->close_on_destroy  = close_on_destroy;
    ctx->parties_for_reading = party;
    party->can_read_from_it  = true;
    party->can_write_to_it   = false;

    c4m_party_listener_t *lobj = get_listener_obj(party);
    lobj->fd                   = sockfd;
    lobj->accept_cb            = (c4m_accept_decl)callback;
    lobj->saved_flags          = fcntl(sockfd, F_GETFL, 0);

    register_read_fd(ctx, party);

    if (stop_when_closed) {
        party->stop_on_close = true;
    }
}

// allocate and call sb_init_party_listener.
c4m_party_t *
c4m_sb_new_party_listener(c4m_switchboard_t *ctx,
                          int                sockfd,
                          c4m_accept_cb_t    callback,
                          bool               stop_when_closed,
                          bool               close_on_destroy)
{
    c4m_party_t *result = (c4m_party_t *)c4m_gc_alloc(c4m_party_t);
    c4m_sb_init_party_listener(ctx,
                               result,
                               sockfd,
                               callback,
                               stop_when_closed,
                               close_on_destroy);

    return result;
}

/*
 * Set up a party object for a non-listener file descriptor.  The file
 * descriptor does NOT have to be non-blocking; it does not matter
 * whether or not it is, actually.
 *
 * The `perms` field should be O_RDONLY, O_WRONLY or O_RDWR.
 *
 * The `stop_when_closed` field closes down the switchboard when I/O
 * to this fd fails.
 *
 * If `close_on_destroy` is true, we will call close() on the fd for
 * you whenever the switchboard is torn down.
 */
void
c4m_sb_init_party_fd(c4m_switchboard_t *ctx,
                     c4m_party_t       *party,
                     int                fd,
                     int                perms,
                     bool               stop_when_closed,
                     bool               close_on_destroy,
                     bool               proxy_close)
{
    memset(party, 0, sizeof(c4m_party_t));

    party->c4m_party_type   = C4M_PT_FD;
    party->close_on_destroy = close_on_destroy;
    party->can_read_from_it = false;
    party->can_write_to_it  = false;

    c4m_party_fd_t *fd_obj = get_fd_obj(party);
    fd_obj->first_msg      = NULL;
    fd_obj->last_msg       = NULL;
    fd_obj->subscribers    = NULL;
    fd_obj->proxy_close    = proxy_close;
    fd_obj->fd             = fd;

    if (perms != O_WRONLY) {
        party->open_for_read    = true;
        party->can_read_from_it = true;
        register_read_fd(ctx, party);
    }
    if (perms != O_RDONLY) {
        party->open_for_write  = true;
        party->can_write_to_it = true;
        register_writer_fd(ctx, party);
    }
    if (stop_when_closed) {
        party->stop_on_close = true;
    }
}

c4m_party_t *
c4m_sb_new_party_fd(c4m_switchboard_t *ctx,
                    int                fd,
                    int                perms,
                    bool               stop_when_closed,
                    bool               close_on_destroy,
                    bool               proxy_close)
{
    c4m_party_t *result = (c4m_party_t *)c4m_gc_alloc(c4m_party_t);
    c4m_sb_init_party_fd(ctx,
                         result,
                         fd,
                         perms,
                         stop_when_closed,
                         close_on_destroy,
                         proxy_close);

    return result;
}

/*
 * This is for sending strings to a fd. You can send the same input
 * buffer to multiple processes, or reuse an object to rechange the
 * string; the string gets processed at the time you call `route()`
 * with one of these as the `read_from` parameter.
 */
void
c4m_sb_init_party_input_buf(c4m_switchboard_t *ctx,
                            c4m_party_t       *party,
                            char              *input,
                            size_t             len,
                            bool               dup,
                            bool               close_fd_when_done)
{
    char *to_set = input;

    if (dup) {
        to_set = (char *)c4m_gc_alloc(len + 1);
        memcpy(to_set, input, len);
    }
    party->open_for_read     = true;
    party->open_for_write    = false;
    party->can_read_from_it  = true;
    party->can_write_to_it   = false;
    party->c4m_party_type    = C4M_PT_STRING;
    c4m_party_instr_t *sobj  = get_sstr_obj(party);
    sobj->strbuf             = to_set;
    sobj->len                = len;
    sobj->close_fd_when_done = close_fd_when_done;

    register_loner(ctx, party);
}

c4m_party_t *
c4m_sb_new_party_input_buf(c4m_switchboard_t *ctx,
                           char              *input,
                           size_t             len,
                           bool               dup,
                           bool               close_fd_when_done)
{
    c4m_party_t *result = (c4m_party_t *)c4m_gc_alloc(c4m_party_t);
    c4m_sb_init_party_input_buf(ctx,
                                result,
                                input,
                                len,
                                dup,
                                close_fd_when_done);

    return result;
}

void
c4m_sb_party_input_buf_new_string(c4m_party_t *party,
                                  char        *input,
                                  size_t       len,
                                  bool         dup,
                                  bool         close_fd_when_done)
{
    if (party->c4m_party_type != C4M_PT_STRING || !party->can_read_from_it) {
        return;
    }
    c4m_party_instr_t *sobj  = get_sstr_obj(party);
    sobj->len                = len;
    sobj->close_fd_when_done = close_fd_when_done;

    if (dup) {
        sobj->strbuf = c4m_gc_alloc(len + 1);
        memcpy(sobj->strbuf, input, len);
    }
    else {
        sobj->strbuf = input;
    }
}

/*
 * When you want to capture process output, but don't need it until
 * the process is closes, this is your huckleberry. You can use one of
 * these per fd you want to capture, or you can combine them.
 *
 * So if you want to combine stdin / stdout into one stream, you can
 * do it.
 *
 * The `tag` field is used to distinguish multiple output buffers when
 * the switchboard is closed down.
 */
void
c4m_sb_init_party_output_buf(c4m_switchboard_t *ctx,
                             c4m_party_t       *party,
                             char              *tag,
                             size_t             buflen)
{
    if (buflen < PIPE_BUF) {
        buflen = PIPE_BUF;
    }

    size_t n = buflen / PIPE_BUF;

    if (buflen % PIPE_BUF) {
        n++;
    }

    party->can_read_from_it = false;
    party->can_write_to_it  = true;
    party->open_for_read    = false;
    party->open_for_write   = true;
    party->c4m_party_type   = C4M_PT_STRING;

    c4m_party_outstr_t *dobj = get_dstr_obj(party);
    dobj->strbuf             = (char *)c4m_gc_array_alloc(PIPE_BUF, n);
    dobj->len                = n * PIPE_BUF;
    dobj->step               = party->info.wstrinfo.len;
    dobj->tag                = tag;
    dobj->ix                 = 0;

    register_loner(ctx, party);
}

c4m_party_t *
c4m_sb_new_party_output_buf(c4m_switchboard_t *ctx, char *tag, size_t buflen)
{
    c4m_party_t *result = (c4m_party_t *)c4m_gc_alloc(c4m_party_t);
    c4m_sb_init_party_output_buf(ctx, result, tag, buflen);

    return result;
}

/*
 * This sets up a callback that can receive incremental data read from
 * a file descriptor (NOT a listener though).
 *
 * Any state information you can carry via the `extra` state in
 * either c4m_switchboard_t or c4m_party_t.
 *
 * see `c4m_sb_get_extra()`, `c4m_sb_set_extra()`,
 *     `c4m_sb_get_c4m_party_extra()` and `c4m_sb_set_c4m_party_extra()`.
 */
void
c4m_sb_init_party_callback(c4m_switchboard_t *ctx,
                           c4m_party_t       *party,
                           c4m_sb_cb_t        cb)
{
    party->open_for_read        = false;
    party->open_for_write       = true;
    party->can_read_from_it     = false;
    party->can_write_to_it      = true;
    party->c4m_party_type       = C4M_PT_CALLBACK;
    party->info.cbinfo.callback = (c4m_sb_cb_t)cb;

    register_loner(ctx, party);
}

/*
 * This is used to register a process and associate it with its read/write
 * file descriptors (via party objects).
 */
void
c4m_sb_monitor_pid(c4m_switchboard_t *ctx,
                   pid_t              pid,
                   c4m_party_t       *stdin_fd_party,
                   c4m_party_t       *stdout_fd_party,
                   c4m_party_t       *stderr_fd_party,
                   bool               shutdown)
{
    c4m_monitor_t *monitor = (c4m_monitor_t *)c4m_gc_alloc(c4m_monitor_t);

    monitor->pid                  = pid;
    monitor->stdin_fd_party       = stdin_fd_party;
    monitor->stdout_fd_party      = stdout_fd_party;
    monitor->stderr_fd_party      = stderr_fd_party;
    monitor->next                 = ctx->pid_watch_list;
    monitor->shutdown_when_closed = shutdown;
    ctx->pid_watch_list           = monitor;
}

/*
 * Retrieve any user-defined pointer for a switchboard object.
 */
void *
c4m_sb_get_extra(c4m_switchboard_t *ctx)
{
    return ctx->extra;
}

/*
 * Set the user-defined pointer for a switchboard object.
 */
void
c4m_sb_set_extra(c4m_switchboard_t *ctx, void *ptr)
{
    ctx->extra = ptr;
}

/*
 * Retrieve any user-defined pointer for a party object.
 */
void *
c4m_sb_get_c4m_party_extra(c4m_party_t *party)
{
    return party->extra;
}

/*
 * Set the user-defined pointer for a party object.
 */
void
c4m_sb_set_party_extra(c4m_party_t *party, void *ptr)
{
    party->extra = ptr;
}

static inline void
add_heap(c4m_switchboard_t *ctx)
{
    c4m_sb_heap_t *old        = ctx->heap;
    int            elem_space = ctx->heap_elems * sizeof(c4m_sb_msg_t);

    ctx->heap           = c4m_gc_raw_alloc(elem_space + sizeof(c4m_sb_heap_t),
                                 GC_SCAN_ALL);
    ctx->heap->next     = old;
    ctx->heap->cur_cell = 0;
}

static inline c4m_sb_msg_t *
get_msg_slot(c4m_switchboard_t *ctx)
{
    c4m_sb_msg_t  *result;
    c4m_sb_heap_t *heap;

    if (!ctx->heap || (ctx->heap->cur_cell >= ctx->heap_elems)) {
        add_heap(ctx);
    }

    if (ctx->freelist != NULL) {
        result        = ctx->freelist;
        ctx->freelist = result->next;

#ifdef C4M_SB_DEBUG
        printf("get_slot: freelist (%p). New freelist: %p\n",
               result,
               ctx->freelist);
#endif
        return result;
    }

    heap   = ctx->heap;
    result = &(heap->cells[heap->cur_cell++]);

    memset(result, 0, sizeof(c4m_sb_msg_t));
    return result;
}

/* Doesn't mean we call free(), just that we can hand it out again
 * when get_msg_slot() is called.
 *
 * Being good citizens, we zero out the len and data fields.
 */
static inline void
free_msg_slot(c4m_switchboard_t *ctx, c4m_sb_msg_t *slot)
{
    slot->next    = ctx->freelist;
    slot->len     = 0;
    ctx->freelist = slot;

    memset(slot->data, 0, C4M_SB_MSG_LEN);
}

/*
 * Internal function to enqueue any messages of size up to PIPE_BUF to
 * writable file descriptors.
 */
static inline void
publish(c4m_switchboard_t *ctx, char *buf, ssize_t len, c4m_party_t *party)
{
    if (!party->open_for_write) {
        return;
    }

    c4m_party_fd_t *receiver = get_fd_obj(party);
    c4m_sb_msg_t   *msg      = get_msg_slot(ctx);

    if (len) {
        memcpy(msg->data, buf, len);
        msg->len = len;
    }
    else {
        msg->len = 0;
    }

    msg->next = NULL;

    if (receiver->first_msg == NULL) {
        receiver->first_msg = msg;
    }
    else {
        receiver->last_msg->next = msg;
    }

    receiver->last_msg = msg;

#ifdef C4M_SB_DEBUG
    printf(">>Enqueued message for fd %d", c4m_sb_party_fd(party));
    print_hex(receiver->last_msg->data, receiver->last_msg->len, ":");
#endif
}

/*
 * Route a party that we read from, to a party that we write to.
 * If the mix is invalid, then this returns 'false'.
 *
 * Listeners cannot be routed; you supply a callback when you
 * register them.
 *
 * Strings can be routed only to FD writers; the strings are totally
 * enqueued at the time of the route() call, by calling publish()
 * on chunks up to PIPE_BUF in size.
 *
 * FDs for read can be routed to FD writers, strings for output,
 * or to callbacks.
 *
 * You can use the same FDs in multiple routings, no problem. That
 * means you can send output from a single read fd to as many places
 * as you like, and you may similarly send multiple input sources into
 * a single output file descriptor.
 */
bool
c4m_sb_route(c4m_switchboard_t *ctx,
             c4m_party_t       *read_from,
             c4m_party_t       *write_to)
{
    if (read_from == NULL || write_to == NULL) {
        return false;
    }

    if (!read_from->open_for_read || !write_to->open_for_write) {
        return false;
    }

    if (read_from->c4m_party_type & (C4M_PT_LISTENER | C4M_PT_CALLBACK)) {
        return false;
    }

    if (write_to->c4m_party_type & C4M_PT_LISTENER) {
        return false;
    }

    if (read_from->c4m_party_type == C4M_PT_STRING) {
        if (write_to->c4m_party_type != C4M_PT_FD) {
            return false;
        }
        c4m_party_instr_t *s         = get_sstr_obj(read_from);
        size_t             remaining = s->len;
        char              *p         = s->strbuf;
        char              *end       = p + remaining;

        while (p < (end - C4M_SB_MSG_LEN)) {
            publish(ctx, p, C4M_SB_MSG_LEN, write_to);
            p += C4M_SB_MSG_LEN;
        }
        if (p != end) {
            publish(ctx, p, end - p, write_to);
        }

        if (s->close_fd_when_done) {
            publish(ctx, NULL, 0, write_to);
        }

        return true;
    }
    else {
        // Will be C4M_PT_FD.
        c4m_party_fd_t *r_fd_obj = get_fd_obj(read_from);

        c4m_subscription_t *subscription;
        subscription = c4m_gc_alloc(c4m_subscription_t);

        if (write_to->c4m_party_type == C4M_PT_FD) {
#if defined(C4M_SB_DEBUG) || defined(C4M_SB_TEST)
            printf("sub(src_fd=%d, dst_fd=%d)\n",
                   c4m_sb_party_fd(read_from),
                   c4m_sb_party_fd(write_to));
#endif
        }
        else if (write_to->c4m_party_type == C4M_PT_CALLBACK) {
#if defined(C4M_SB_DEBUG) || defined(C4M_SB_TEST)
            printf("sub(src_fd=%d, dst = callback)\n",
                   c4m_sb_party_fd(read_from));
#endif
        }
        else {
            c4m_party_outstr_t *dob = get_dstr_obj(write_to);
            if (!dob->tag) {
                return false;
            }
#if defined(C4M_SB_DEBUG) || defined(C4M_SB_TEST)
            printf("sub(src=%d, tag=%s)\n",
                   c4m_sb_party_fd(read_from),
                   dob->tag);
#endif
        }
        subscription->subscriber = write_to;
        subscription->next       = r_fd_obj->subscribers;
        r_fd_obj->subscribers    = subscription;
    }

    return true;
}

/*
 * Pause the specified routing (unsubscribe), if one is active.
 * Returns `true` if the subscription was marked as paused.
 *
 * This does not consider whether the fds are closed.
 *
 * Currently, there's no explicit facility for removing subscriptions.
 * Just pause it and never unpause it!
 */
bool
c4m_sb_pause_route(c4m_switchboard_t *ctx,
                   c4m_party_t       *read_from,
                   c4m_party_t       *write_to)
{
    if (read_from == NULL || write_to == NULL) {
        return false;
    }

    c4m_party_fd_t     *reader = get_fd_obj(read_from);
    c4m_subscription_t *cur    = reader->subscribers;

    while (cur != NULL) {
        if (cur->subscriber != write_to) {
            cur = cur->next;
            continue;
        }
        if (cur->paused) {
            return false;
        }
        else {
            cur->paused = true;
            return true;
        }
    }
    return false;
}

/*
 * Resumes the specified subscription.  Returns `true` if resumption
 * was successful, and `false` if not, including cases where the
 * subscription was already active..
 */
bool
c4m_sb_resume_route(c4m_switchboard_t *ctx,
                    c4m_party_t       *read_from,
                    c4m_party_t       *write_to)
{
    if (read_from == NULL || write_to == NULL) {
        return false;
    }

    c4m_party_fd_t     *reader = get_fd_obj(read_from);
    c4m_subscription_t *cur    = reader->subscribers;

    while (cur != NULL) {
        if (cur->subscriber != write_to) {
            cur = cur->next;
            continue;
        }
        if (cur->paused) {
            cur->paused = true;
            return true;
        }
        else {
            return false;
        }
    }
    return false;
}

/*
 * Returns true if the subscription is active, and in an unpaused state.
 */
bool
c4m_sb_route_is_active(c4m_switchboard_t *ctx,
                       c4m_party_t       *read_from,
                       c4m_party_t       *write_to)
{
    if (read_from == NULL || write_to == NULL) {
        return false;
    }

    if (!read_from->open_for_read || !write_to->open_for_write) {
        return false;
    }

    c4m_party_fd_t     *reader = get_fd_obj(read_from);
    c4m_subscription_t *cur    = reader->subscribers;

    while (cur != NULL) {
        if (cur->subscriber != write_to) {
            cur = cur->next;
            continue;
        }
        return !cur->paused;
    }
    return false;
}

/*
 * Returns true if the subscription is active, but paused.
 */
bool
c4m_sb_route_is_paused(c4m_switchboard_t *ctx,
                       c4m_party_t       *read_from,
                       c4m_party_t       *write_to)
{
    if (read_from == NULL || write_to == NULL) {
        return false;
    }

    if (!read_from->open_for_read || !write_to->open_for_write) {
        return false;
    }

    c4m_party_fd_t     *reader = get_fd_obj(read_from);
    c4m_subscription_t *cur    = reader->subscribers;

    while (cur != NULL) {
        if (cur->subscriber != write_to) {
            cur = cur->next;
            continue;
        }
        return cur->paused;
    }
    return false;
}

/*
 * Returns true if the subscription is active, whether or not it is
 * paused.
 */
bool
c4m_sb_is_subscribed(c4m_switchboard_t *ctx,
                     c4m_party_t       *read_from,
                     c4m_party_t       *write_to)
{
    if (read_from == NULL || write_to == NULL) {
        return false;
    }

    if (!read_from->open_for_read || !write_to->open_for_write) {
        return false;
    }

    c4m_party_fd_t     *reader = get_fd_obj(read_from);
    c4m_subscription_t *cur    = reader->subscribers;

    while (cur != NULL) {
        if (cur->subscriber != write_to) {
            cur = cur->next;
            continue;
        }
        return true;
    }
    return false;
}

/*
 * Initializes a switchboard object, primarily zeroing out the
 * contents, and setting up message buffering.
 */
void
c4m_sb_init(c4m_switchboard_t *ctx, size_t heap_size)
{
    memset(ctx, 0, sizeof(c4m_switchboard_t));
    ctx->heap_elems = heap_size;
    add_heap(ctx);
}

/*
 * This internal function walks a switchboard's parties_for_reading
 * and parties_for_writing fields to figure out what to add to the
 * fd_sets that we pass to select().
 *
 * The read fds are added as long as there are subscribers that are
 * attached that isn't listed as closed.
 *
 * Write fds are added if there's explicitly something in their
 * message queue; first_msg will be non-NULL.
 *
 * We also track to make sure that *some* party is open for either
 * reading that has a valid subscriber, OR at least one party open for
 * writing that has enqueued messages.
 *
 * If neither of these conditions are met, we shut down.
 * down.
 */
static inline void
set_fdinfo(c4m_switchboard_t *ctx)
{
    c4m_party_t *cur;
    bool         open = false; // If this stays false, we give up.

    FD_ZERO(&ctx->readset);
    FD_ZERO(&ctx->writeset);

    cur = ctx->parties_for_reading;

    while (cur != NULL) {
        c4m_party_fd_t     *r_fd_obj    = get_fd_obj(cur);
        c4m_subscription_t *subscribers = r_fd_obj->subscribers;

        if (cur->open_for_read) {
            while (subscribers != NULL) {
                if (cur->c4m_party_type == C4M_PT_FD) {
                    c4m_party_t *onesub = subscribers->subscriber;

                    if (onesub && onesub->open_for_write) {
                        FD_SET(c4m_sb_party_fd(cur), &ctx->readset);
                        open = true;
                        break;
                    }
                }
                else {
                    open = true;
                    FD_SET(c4m_sb_party_fd(cur), &ctx->readset);
                    break;
                }
                subscribers = subscribers->next;
            }
        }
        cur = cur->next_reader;
    }

    cur = ctx->parties_for_writing;
    while (cur != NULL) {
        if (cur->c4m_party_type == C4M_PT_FD && cur->open_for_write && cur->info.fdinfo.first_msg != NULL) {
            open = true;
            FD_SET(c4m_sb_party_fd(cur), &ctx->writeset);
        }
        cur = cur->next_writer;
    }

    // If nothing w/ a file descriptor is left standing, then we will
    // finish up.
    if (!open) {
        ctx->done = true;
    }
}

/*
 * Setting this timeout sets how long we will wait before timing out
 * on a single select() call. Without setting it, select() will wait
 * indefinitely long.
 *
 * When there's a timeout, if there's a progress_callback, we call it.
 * But if there's no progress callback, the switchboard will exit.
 */
void
c4m_sb_set_io_timeout(c4m_switchboard_t *ctx, struct timeval *timeout)
{
    if (timeout == NULL) {
        ctx->io_timeout_ptr = NULL;
    }
    else {
        ctx->io_timeout_ptr = &ctx->io_timeout;
        memcpy(ctx->io_timeout_ptr, timeout, sizeof(struct timeval));
    }
}

void
c4m_sb_clear_io_timeout(c4m_switchboard_t *ctx)
{
    ctx->io_timeout_ptr = NULL;
}

// After select(), test an FD to see if it's ready for read.
static inline bool
reader_ready(c4m_switchboard_t *ctx, c4m_party_t *party)
{
    if (party->open_for_read && FD_ISSET(c4m_sb_party_fd(party), &ctx->readset) != 0) {
        FD_CLR(c4m_sb_party_fd(party), &ctx->writeset);
        return true;
    }
    return false;
}

// After select(), test an FD to see if it's ready for write.
static inline bool
writer_ready(c4m_switchboard_t *ctx, c4m_party_t *party)
{
    if (party->open_for_write && FD_ISSET(c4m_sb_party_fd(party), &ctx->writeset) != 0) {
        return true;
    }
    return false;
}

/*
 * If a fd is reading data, and one of the places we want to send it
 * is to a string that is returned once at the end, we don't bother to
 * enqueue, we directly add to the sink at the time the source
 * produces the data, using this function.
 */
static inline void
add_data_to_string_out(c4m_party_outstr_t *party, char *buf, ssize_t len)
{
#ifdef C4M_SB_DEBUG
    printf("tag = %s, buf = %s, len = %d\n", party->tag, buf, len);
    print_hex(buf, len, ">> add_data_to_string_out: ");
#endif

    if (party->ix + len >= party->len) {
        int newlen = party->len + len + party->step;
        int rem    = newlen % party->step;

        newlen -= rem;
        party->strbuf = realloc(party->strbuf, newlen);

        if (party->strbuf == 0) {
#ifdef C4M_SB_DEBUG
            printf("REALLOC FAILED.  Skipping capture.\n");
#endif
            return;
        }

        party->len = newlen;
        memset(party->strbuf + party->ix, 0, newlen - party->ix);
    }

    memcpy(&party->strbuf[party->ix], buf, len);
    party->ix += len;
}

/*
 * This handles reading from fd sources. String sources never call
 * this, as they get processed in full when sinks subscribe to them.
 *
 * Listeners are handled by a different function (handle_one_accept
 * below).
 *
 * When reading one chunk, we then loop through our subscribers, and
 * take action based on the subscriber type.
 *
 * If the subscriber is a writable fd, we call publish(), which will
 * enqueue for that fd, as long as it's open (it's ignored otherwise).
 *
 * If the subscriber is a string or callback, these things can never
 * be closed, and we process the data right away.
 *
 * Note that this is only called in one of two cases:
 *
 * 1) select() told us there's something to read, in which case,
 *    read_one() should never return a zero-length string (errors
 *    would result in a -1)
 *
 * 2) We know a subprocess is done and we're draining the read side of
 *    that file descriptor. In that case, we know when we're
 *    returned a 0-length value, it's time to mark the read side
 *    as done too.
 */
static inline void
handle_one_read(c4m_switchboard_t *ctx, c4m_party_t *party)
{
    char buf[C4M_SB_MSG_LEN + 1] = {
        0,
    };
    ssize_t read_result = c4m_sb_read_one(c4m_sb_party_fd(party),
                                          buf,
                                          C4M_SB_MSG_LEN);

    if (read_result <= 0) {
        if (read_result < 0) {
            party->found_errno = errno;
        }
        party->open_for_read = false;
#ifdef C4M_SB_DEBUG
        printf("Shut down reading on fd %d in h1r top\n",
               c4m_sb_party_fd(party));
#endif
        if (party->stop_on_close) {
            ctx->done = true;
        }

        /*
         * When there is no input read, we need to propagate close
         * to all the subscribers.
         * This is driven by proxy_close subscriber attribute.
         * As this happens via event-loop we can do that by passing
         * empty write message to the subscriber
         * (see comment in handle_one_write)
         */
        if (party->c4m_party_type == C4M_PT_FD) {
            c4m_party_fd_t     *obj     = get_fd_obj(party);
            c4m_subscription_t *sublist = obj->subscribers;
            while (sublist != NULL) {
                c4m_party_t *sub = sublist->subscriber;
                if (sub->c4m_party_type == C4M_PT_FD) {
                    c4m_party_fd_t *sub_fd = get_fd_obj(sub);
                    if (sub_fd->proxy_close) {
                        publish(ctx, NULL, 0, sub);
                    }
                }
                sublist = sublist->next;
            }
        }
    }
    else {
#ifdef C4M_SB_DEBUG
        printf(">>One read from fd %d", c4m_sb_party_fd(party));
        print_hex(buf, read_result, ": ");
#endif

        c4m_party_fd_t     *obj     = get_fd_obj(party);
        c4m_subscription_t *sublist = obj->subscribers;

        while (sublist != NULL) {
            c4m_party_t *sub = sublist->subscriber;

            if (!sublist->paused) {
                switch (sub->c4m_party_type) {
                case C4M_PT_FD:
                    publish(ctx, buf, read_result, sub);
                    break;
                case C4M_PT_STRING:
                    add_data_to_string_out(get_dstr_obj(sub),
                                           buf,
                                           read_result);
                    break;
                case C4M_PT_CALLBACK:
                    (*sub->info.cbinfo.callback)(ctx->extra,
                                                 sub->extra,
                                                 buf,
                                                 (size_t)read_result);
                    break;
                default:
                    break;
                }
            }
            sublist = sublist->next;
        }
    }
}

/*
 * This just accepts a socket and calls the provided callback to
 * decide what to do with it.
 */
static inline void
handle_one_accept(c4m_switchboard_t *ctx, c4m_party_t *party)
{
    int                   listener_fd  = c4m_sb_party_fd(party);
    c4m_party_listener_t *listener_obj = get_listener_obj(party);
    struct sockaddr       address;
    socklen_t             address_len;

    while (true) {
        int sockfd = accept(listener_fd, &address, &address_len);

        if (sockfd >= 0) {
            (*listener_obj->accept_cb)(ctx, sockfd, &address, &address_len);
            break;
        }
        if (errno == EINTR || errno == EAGAIN) {
            continue;
        }
        if (errno == ECONNABORTED) {
            break;
        }
        party->found_errno   = errno;
        party->open_for_read = false;
        break;
    }
}

/*
 * This function handles writing to a writable file descriptor, where
 * select() has told us it's ready to receive a write. We simply
 * attempt to write one queued message to it, then remove that message
 * from the queue.
 *
 * The only exception is if the message is a null length, which is an
 * instruction to actually call close() on the fd.
 */
static inline void
handle_one_write(c4m_switchboard_t *ctx, c4m_party_t *party)
{
    c4m_party_fd_t *fdobj = get_fd_obj(party);
    c4m_sb_msg_t   *msg   = fdobj->first_msg;

    if (!msg) {
        return;
    }

    if (msg && (msg->next == NULL || msg == fdobj->last_msg)) {
        fdobj->first_msg = NULL;
        fdobj->last_msg  = NULL;
    }
    else {
        fdobj->first_msg = msg->next;
    }

    if (!msg->len) {
/* Real messages should always have lengths. We get passed a
 * zero-length message only if a string was fed in for input,
 * with instructions for us to close after it's consumed.
 *
 * The close instruction is communicated by sending a null
 * message. So when we see it, we mark ourselves as closed.
 */
#ifdef C4M_SB_DEBUG
        printf("0-length write; shutting down write-side of fd.\n");
#endif
        party->open_for_write = false;
        if (!party->open_for_read) {
            close(c4m_sb_party_fd(party));
        }
        free_msg_slot(ctx, msg);
        return;
    }

#ifdef C4M_SB_DEBUG
    printf("Writing from queue to fd %d", c4m_sb_party_fd(party));
    print_hex(msg->data, msg->len, ": ");
#endif

    if (!c4m_sb_write_data(c4m_sb_party_fd(party), msg->data, msg->len)) {
        party->found_errno = errno;

        party->open_for_write = false;
        if (!party->open_for_read) {
            close(c4m_sb_party_fd(party));
        }

        if (party->stop_on_close) {
            ctx->done = true;
        }
        /* If the write failed, we'll never try to write again.
         * The message slot we tried to write gets freed below
         * no matter what, but let's free any other queued slots
         * here.
         */
        c4m_sb_msg_t *to_free = fdobj->first_msg;

        while (to_free) {
            c4m_sb_msg_t *next = to_free->next;

            free_msg_slot(ctx, to_free);
            to_free = next;
        }
        fdobj->first_msg = NULL;
        fdobj->last_msg  = NULL;
        return;
    }

    free_msg_slot(ctx, msg);
}

// Not much here, just identify fds ready for read and dispatch.
static inline void
handle_ready_reads(c4m_switchboard_t *ctx)
{
    c4m_party_t *reader = ctx->parties_for_reading;

    while (reader != NULL) {
        if (reader_ready(ctx, reader)) {
            FD_CLR(c4m_sb_party_fd(reader), &ctx->readset);

            if (reader->c4m_party_type == C4M_PT_FD) {
                handle_one_read(ctx, reader);
            }
            else {
                handle_one_accept(ctx, reader);
            }
        }
        reader = reader->next_reader;
    }
}

// Dispatch for any fds ready for writing.
static inline void
handle_ready_writes(c4m_switchboard_t *ctx)
{
    c4m_party_t *writer = ctx->parties_for_writing;

    while (writer != NULL) {
        if (writer_ready(ctx, writer)) {
            FD_CLR(c4m_sb_party_fd(writer), &ctx->readset);

            handle_one_write(ctx, writer);
        }
        writer = writer->next_writer;
    }
}

// Doesn't seem to be used anymore?

#if 0
// If a subprocess shut down, clean up.
static inline void
subproc_mark_closed(c4m_monitor_t *proc, bool error)
{
    proc->closed = true;

    if (error) {
        proc->found_errno = errno;
    }

    // We can mark the write side of this as closed right away. But we
    // can't do the same w/ the read side; we need to drain them
    // first.

    if (proc->stdin_fd_party) {
        proc->stdin_fd_party->open_for_write = false;
    }
}
#endif

static bool
c4m_sb_default_check_exit_conditions(c4m_switchboard_t *ctx)
{
    c4m_monitor_t *subproc          = ctx->pid_watch_list;
    bool           close_if_drained = false;

    while (subproc) {
        if (subproc->closed && subproc->shutdown_when_closed) {
            close_if_drained = true;
            break;
        }
        subproc = subproc->next;
    }

    if (!close_if_drained) {
        return false;
    }

    if (subproc->stdout_fd_party && subproc->stdout_fd_party->open_for_read) {
        return false;
    }
    else {
#if defined(C4M_SB_DEBUG) || defined(C4M_SB_TEST)
        printf("Subproc stdout is closed for business.\n");
#endif
    }

    if (subproc->stderr_fd_party && subproc->stderr_fd_party->open_for_read) {
#if defined(C4M_SB_DEBUG) || defined(C4M_SB_TEST)
        printf("Read side of stderr is still open.\n");
#endif
        return false;
    }

    c4m_party_t *writers = ctx->parties_for_writing;

    while (writers) {
        if (writers->c4m_party_type == C4M_PT_FD) {
            c4m_party_fd_t *fd_writer = get_fd_obj(writers);

#if defined(C4M_SB_DEBUG) || defined(C4M_SB_TEST)
            printf("checking for pending writes to fd %d\n", fd_writer->fd);
#endif
            if (writers->open_for_write && fd_writer->first_msg) {
#if defined(C4M_SB_DEBUG) || defined(C4M_SB_TEST)
                printf("Don't exit yet.\n");
#endif
                return false;
            }
#if defined(C4M_SB_DEBUG) || defined(C4M_SB_TEST)
            if (writers->open_for_write) {
                printf("No queue.\n");
            }
            else {
                printf("Already closed.\n");
            }
#endif
        }
        writers = writers->next_writer;
    }

    return true;
}

void
process_status_check(c4m_monitor_t *subproc, bool wait_on_exit)
{
    int stat_info;
    int flag;

    if (wait_on_exit) {
        flag = 0;
    }
    else {
        flag = WNOHANG;
    }

    if (subproc->closed) {
        return;
    }

    while (true) {
        switch (waitpid(subproc->pid, &stat_info, flag)) {
        case 0:
            return; // Process is sill running.
        case -1:
            if (errno == EINTR) {
                continue;
            }
            subproc->closed      = true;
            subproc->found_errno = errno;
            return;
        default:
            subproc->closed      = true;
            subproc->exit_status = WEXITSTATUS(stat_info);

            if (WIFSIGNALED(stat_info)) {
                subproc->term_signal = WTERMSIG(stat_info);
            }
            return;
        }
    }
}

/*
 * Every time we finish a select, check to see if we need to invoke
 * the progress callback, and if we should exit.
 *
 * This involves testing exit conditions-- first, if the progress
 * callback returns `true`, that indicates we should exit. Second, if
 * no callback was set for progress monitoring, and the select()
 * timeout fired, then we are done.
 *
 * Third, we look at each subprocess to see if it's fully exited (all
 * its read fds are closed). If it has, and if we're supposed to shut
 * down when it does, subproc_mark_closed() above will set the 'done'
 * flag for us.
 */
static inline void
handle_loop_end(c4m_switchboard_t *ctx)
{
    c4m_monitor_t *subproc = ctx->pid_watch_list;

    while (subproc != NULL) {
        process_status_check(subproc, false);
        subproc = subproc->next;
    }

    if (!ctx->progress_callback) {
        // clang-format off
        ctx->progress_callback =
	    (c4m_progress_decl)c4m_sb_default_check_exit_conditions;
        // clang-format on
    }

    if (!ctx->progress_on_timeout_only) {
        if ((*ctx->progress_callback)(ctx)) {
            ctx->done = true;
        }
    }
}

#if 0
/*
 * Used only to make sure we don't free registered readers when
 * they're also registered writers; we wait until we process the
 * registered writer list.
 *
 * No longer needed w/ GC.
 */
static bool
is_registered_writer(c4m_switchboard_t *ctx, c4m_party_t *target)
{
    c4m_party_t *cur = ctx->parties_for_writing;

    while (cur) {
        if (target == cur) {
            return true;
        }
        cur = cur->next_writer;
    }

    return false;
}
#endif

/*
 * With GC, this is currently just about closing file descriptors.
 *
 * Note that this does NOT free the switchboard object,
 * just any internal data structures.
 */

void
c4m_sb_destroy(c4m_switchboard_t *ctx, bool free_parties)
{
    c4m_party_t *cur, *next;

    cur = ctx->parties_for_reading;

    while (cur) {
        if (cur->close_on_destroy) {
            if (cur->c4m_party_type & (C4M_PT_FD | C4M_PT_LISTENER)) {
                close(c4m_sb_party_fd(cur));
            }
            cur = cur->next_reader;
        }
    }

    cur = ctx->parties_for_writing;

    while (cur) {
        if (cur->close_on_destroy && cur->c4m_party_type == C4M_PT_FD) {
            close(c4m_sb_party_fd(cur));
        }
        next = cur->next_writer;
        cur  = next;
    }
}

/*
 * Extract results from the switchbaord.
 */
void
c4m_sb_get_results(c4m_switchboard_t *ctx, c4m_capture_result_t *result)
{
    c4m_party_outstr_t *strobj;
    c4m_party_t        *party    = ctx->party_loners; // Look for str outputs.
    int                 capcount = 0;
    int                 ix       = 0;

    if (result->inited) {
        return;
    }

    result->inited = true;

    while (party) {
        if (party->c4m_party_type == C4M_PT_STRING && party->can_write_to_it) {
            capcount++;
        }
        party = party->next_loner;
    }

    result->num_captures = capcount;
    result->captures     = c4m_gc_array_alloc(c4m_one_capture_t, capcount + 1);

    party = ctx->party_loners;

    while (party) {
        if (party->c4m_party_type == C4M_PT_STRING && party->can_write_to_it) {
            c4m_one_capture_t *r = result->captures + ix;

            strobj = get_dstr_obj(party);
            r->tag = strobj->tag;
            r->len = strobj->ix;

            if (strobj->ix) {
                r->contents = strobj->strbuf;

                strobj->strbuf = 0;
                strobj->ix     = 0;
            }
            else {
                r->contents = NULL;
            }
            ix += 1;
        }
        party = party->next_loner;
    }
}

char *
c4m_sb_result_get_capture(c4m_capture_result_t *ctx,
                          char                 *tag,
                          bool                  caller_borrow)
{
    char *result;

    for (int i = 0; i < ctx->num_captures; i++) {
        if (!strcmp(ctx->captures[i].tag, tag)) {
            result = ctx->captures[i].contents;

            if (!caller_borrow) {
                ctx->captures[i].contents = NULL;
            }
            return result;
        }
    }
    return NULL;
}

/*
 * A noop w/ GC.
 */
void
c4m_sb_result_destroy(c4m_capture_result_t *ctx)
{
}

/*
 * Returns true if there are any open writers that have enqueued items.
 */
static bool
waiting_writes(c4m_switchboard_t *ctx)
{
    c4m_party_t *writer = ctx->parties_for_writing;

    while (writer) {
        if (writer->open_for_write) {
            c4m_party_fd_t *fd_obj = get_fd_obj(writer);

            if (fd_obj->first_msg != NULL) {
                return true;
            }
        }
        writer = writer->next_writer;
    }

    return false;
}

/*
 * Runs a switchboard; to completion if you pass `true` to the loop
 * parameter, but doesn't post-process in any way. So it doesn't
 * create a result object, nor does it clean up any memory.
 */
bool
c4m_sb_operate_switchboard(c4m_switchboard_t *ctx, bool loop)
{
    if (ctx->done && !waiting_writes(ctx)) {
        return true;
    }
    do {
        set_fdinfo(ctx);
        if (c4m_sb_default_check_exit_conditions(ctx)) {
            return true;
        }
        if (ctx->done && !waiting_writes(ctx)) {
            return true;
        }
        ctx->fds_ready = select(ctx->max_fd,
                                &ctx->readset,
                                &ctx->writeset,
                                NULL,
                                ctx->io_timeout_ptr);
        if (ctx->fds_ready > 0) {
            handle_ready_reads(ctx);
            handle_ready_writes(ctx);
        }
        if (ctx->fds_ready >= 0) {
            handle_loop_end(ctx);
        }
        else {
            // select returned error
            if (errno == EINTR) {
                continue;
            }
            ctx->done = true;
            return false;
        }
    } while (loop);
    return false;
}
