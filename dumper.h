#ifndef   _BTREE_DUMPER_H_
#define   _BTREE_DUMPER_H_

#include <pthread.h>

#include "btree.h"
#include "cache.h"

int dumper_init (struct DB *db, struct PagePool *pp);
int dumper_free (struct PagePool *pp);
int dumper_readq_enqueue(struct CacheBase *cache, struct CacheElem *elem);

#endif /* _BTREE_DUMPER_H_ */
