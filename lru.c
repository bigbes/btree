#include <assert.h>
#include "dbg.h"
#include "cache.h"
#include "pagepool.h"
#include <uthash.h>
#include <utlist.h>

static int find_unused(struct CacheElem *l1, struct CacheElem *l2) {
	return (l1->used - l2->used);
}

struct CacheElem *lru_page_get_free(struct CacheBase *cache) {
	struct CacheElem *retval = NULL;
	struct CacheElem temp; temp.used = 0;
	LL_SEARCH(cache->list_tail, retval, &temp, find_unused);
	assert(retval);
	LL_DELETE(cache->list_tail, retval);
	retval->used = 1;
	LL_APPEND(cache->list_head, retval);
	return retval;
}

void *lru_page_get(struct CacheBase *cache, pageno_t page) {
	struct CacheElem *elem = NULL;
	HASH_FIND_INT(cache->hash, &page, elem);
	if (elem == NULL) {
		elem = lru_page_get_free(cache);
		elem->id = page;
		elem->used = 1;
		HASH_ADD_INT(cache->hash, id, elem);
		pool_read_into(cache->pool, page, elem->cache);
		log_info("Getting page %zd from disk", page);
	} else {
		log_info("Getting page %zd from memory", page);
	}
	return elem->cache;
}

int lru_page_free(struct CacheBase *cache, pageno_t page) {
	struct CacheElem *elem = NULL;
	HASH_FIND_INT(cache->hash, &page, elem);
	if (elem != NULL)
		elem->used = 0;
	return 0;
}
