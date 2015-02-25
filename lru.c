#include <assert.h>

#include "dbg.h"
#include "cache.h"
#include "pagepool.h"
#include "dumper.h"

#include <uthash.h>
#include <utlist.h>

static int find_unused(struct CacheElem *l1, struct CacheElem *l2) {
	return (l1->flag & CACHE_USED) || (l1->flag & CACHE_DIRTY);
}

static struct CacheElem *lru_page_get_free(struct CacheBase *cache) {
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

struct CacheElem *lrui_page_get(struct CacheBase *cache, pageno_t page, int nolock) {
	struct CacheElem *elem = NULL;
	HASH_FIND_INT(cache->hash, &page, elem);
	if (elem == NULL) {
		elem = lru_page_get_free(cache);
		if (!nolock) {
			pthread_mutex_lock(&elem->lock); 
			log_info("Locking page %zd", page);
		}
		elem->id = page;
		elem->flag |= CACHE_USED;
		HASH_ADD_INT(cache->hash, id, elem);
		//pool_read(cache->pool, page, elem->cache);
		dumper_readq_enqueue(cache, elem);
		pthread_cond_wait(&elem->rw_signal, &elem->lock);
		memcpy(elem->prev, elem->cache, cache->pool->page_size);
		log_info("Getting page %zd from disk", page);
	} else {
		if (!nolock) {
			pthread_mutex_lock(&elem->lock);
			log_info("Locking page %zd", page);
		}
		log_info("Getting page %zd from memory", page);
	}
	return elem;
}

void *lru_page_get(struct CacheBase *cache, pageno_t page) {
	return lrui_page_get(cache, page, 1)->cache;
}

int lru_page_free(struct CacheBase *cache, pageno_t page) {
	struct CacheElem *elem = NULL;
	HASH_FIND_INT(cache->hash, &page, elem);
	memcpy(elem->prev, elem->cache, cache->pool->page_size);
	pthread_mutex_unlock(&elem->lock);
	log_info("Unocking page %zd", page);
	if (elem != NULL) elem->flag &= (-1 - CACHE_USED);
	return 0;
}
