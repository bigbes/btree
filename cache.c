#include <math.h>
#include <stdio.h>

#include "cache.h"
#include "dbg.h"
#include "btree.h"

#include <utlist.h>

struct CacheElem *cachei_page_alloc(struct CacheBase *cache) {
	struct CacheElem *elem = calloc(1, sizeof(struct CacheElem));
	check_mem(elem, sizeof(struct CacheElem));
	elem->cache = calloc(1, cache->pool->page_size);
	check_mem(elem->cache, cache->pool->page_size);
	return elem;
error:
	exit(-1);
}

int cachei_page_free(struct CacheElem *elem) {
	free(elem->cache);
	free(elem);
	return 0;
}

int cache_init(struct CacheBase *cache, struct PagePool *pool, size_t cache_size) {
	pageno_t count = floor(((double)cache_size)/pool->page_size);
	cache->cache_size = cache_size;
	cache->hash = NULL;
	cache->pool = pool;
	cache->list_tail = cache->list_head = cachei_page_alloc(cache);
	struct CacheElem *elem = NULL;
	while (count-- > 0) {
		elem = cachei_page_alloc(cache);
		elem->next = cache->list_tail;
		cache->list_tail = elem;
	}
	cache_print(cache);
	return 0;
}

int cache_free(struct CacheBase *cache) {
	struct CacheElem *el1, *el2;
	HASH_ITER(hh, cache->hash, el1, el2) {
		HASH_DEL(cache->hash, el1);
	}
	el1 = cache->list_tail;
	while ((el2 = el1)) {
		el1 = el2->next;
		cachei_page_free(el2);
	}
	cache->list_tail = cache->list_head = NULL;
	cache->pool = NULL;
	cache->hash = NULL;
	return 0;
}

int cache_print(struct CacheBase *cache) {
	int count_1 = 0; struct CacheElem *temp;
	int count_2 = 0;
	LL_COUNT(cache->list_tail, temp, count_1);
	LL_FOREACH(cache->list_tail, temp)
		if (temp->flag && CACHE_USED) count_2++;
	log_info("==================================================");
	log_info("Cache stats: (all: %d, used: %d)", count_1, count_2);
	log_info("==================================================");
	return 0;
}
