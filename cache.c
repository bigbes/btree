#include "cache.h"

#include <math.h>
#include <stdio.h>
#include "btree.h"

#include <utlist.h>

int cache_init(struct CacheBase *cache, struct PagePool *pool, size_t cache_size) {
	pageno_t count = floor(((double)cache_size)/pool->pageSize);
	cache->cache_size = cache_size;
	cache->list_head = cache->list_tail = NULL;
	cache->hash = NULL;
	cache->list_tail = cache->list_head = calloc(1, sizeof(struct CacheElem));
	cache->pool = pool;
	assert(cache->list_head);
	cache->list_head->cache = calloc(1, pool->pageSize);
	assert(cache->list_head->cache);
	struct CacheElem *alloc;
	while (count-- > 0) {
		alloc = calloc(1, sizeof(struct CacheElem)); assert(alloc);
		alloc->cache = calloc(1, pool->pageSize);
		assert(alloc->cache);
		LL_PREPEND(cache->list_tail, alloc);
	}
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
		free(el2->cache);
		free(el2);
	}
	cache->pool = NULL;
	cache->list_tail = NULL;
	cache->list_head = NULL;
	cache->hash = NULL;
	return 0;
}

int cache_print(struct CacheBase *cache) {
	int count = 0; struct CacheElem *temp;
	LL_COUNT(cache->list_tail, temp, count);
	printf("==============================\n");
	printf("Cache stats:\n");
	printf("Overall pages: %d\n", count);
	count = 0;
	LL_FOREACH(cache->list_tail, temp) {
		if (temp->used == 1) count++;
	}
	printf("Used pages: %d\n", count);
	printf("==============================\n");
	return 0;
}
