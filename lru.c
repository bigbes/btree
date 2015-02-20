#include <assert.h>

#include "dbg.h"
#include "cache.h"
#include "pagepool.h"

#include <uthash.h>
#include <utlist.h>

static int find_unused(struct CacheElem *l1, struct CacheElem *l2) {
	return (l1->flag & CACHE_USED);
}

static int find_dirty(struct CacheElem *l1, struct CacheElem *l2) {
	return !(l1->flag & CACHE_DIRTY);
}

struct CacheElem *lru_page_get_free(struct CacheBase *cache) {
	struct CacheElem *retval = NULL;
	LL_SEARCH(cache->list_tail, retval, NULL, find_unused);
	if (retval == NULL) {
		retval = cachei_page_alloc(cache);
	} else {
		LL_DELETE(cache->list_tail, retval);
	}
	retval->flag |= CACHE_USED;
	LL_APPEND(cache->list_head, retval);
	return retval;
}

void *lru_page_get(struct CacheBase *cache, pageno_t page) {
	struct CacheElem *elem = NULL;
	HASH_FIND_INT(cache->hash, &page, elem);
	if (elem == NULL) {
		elem = lru_page_get_free(cache);
		elem->id = page;
		elem->flag |= CACHE_USED;
		HASH_ADD_INT(cache->hash, id, elem);
		pool_read(cache->pool, page, elem->cache);
		log_info("Getting page %zd from disk", page);
	} else {
		log_info("Getting page %zd from memory", page);
	}
	return elem->cache;
}

int lru_page_free(struct CacheBase *cache, pageno_t page) {
	struct CacheElem *elem = NULL;
	HASH_FIND_INT(cache->hash, &page, elem);
	if (elem != NULL) {
		elem->flag &= (-1 - CACHE_USED);
	}
	return 0;
}
