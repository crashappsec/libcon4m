#include "hatrack/witchhat.h"

enum64(witchhat_flag_t,
       WITCHHAT_F_MOVING   = 0x8000000000000000,
       WITCHHAT_F_MOVED    = 040000000000000000,
       WITCHHAT_F_INITED   = 0x2000000000000000,
       WITCHHAT_EPOCH_MASK = 0x1fffffffffffffff);

/* These need to be non-static because tophat and hatrack_dict both
 * need them, so that they can call in without a second call to
 * MMM. But, they should be considered "friend" functions, and not
 * part of the public API.
 *
 * Actually, hatrack_dict no longer uses Witchhat, it uses Crown, but
 * I'm going to explicitly leave these here, instead of going back to
 * making them static.
 */
witchhat_store_t *witchhat_store_new(uint64_t);
void             *witchhat_store_get(witchhat_store_t *, hatrack_hash_t, bool *);
void             *witchhat_store_put(witchhat_store_t *, witchhat_t *, hatrack_hash_t, void *, bool *, uint64_t);
void             *witchhat_store_replace(witchhat_store_t *, witchhat_t *, hatrack_hash_t, void *, bool *, uint64_t);
bool              witchhat_store_add(witchhat_store_t *, witchhat_t *, hatrack_hash_t, void *, uint64_t);
void             *witchhat_store_remove(witchhat_store_t *, witchhat_t *, hatrack_hash_t, bool *, uint64_t);