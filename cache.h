#ifndef _BTREE_CACHE_H_
#define _BTREE_CACHE_H_

#include "btree.h"
#include "pagepool.h"

#include <uthash.h>

#include <pthread.h>

struct CacheElem {
	pageno_t id;
	void *cache;
	void *prev;
	int flag;
#define CACHE_USED  0x01
#define CACHE_DIRTY 0x02
#define CACHE_EMPTY 0x04
	struct CacheElem *next;
	UT_hash_handle hh;
	pthread_mutex_t lock;
	pthread_cond_t  rw_signal;
	struct CacheElem *rq_next;
};

/*
 *  /LRU/
 *  list_tail (recently used)
 *  ||                           list_head (most unused)
 *  ||                                                ||
 *  \/                                                \/
 *  |----|----|----|----|----|----|----|----|----|----|
 */
struct CacheBase {
	struct PagePool *pool;
	size_t cache_size;
	struct CacheElem *list_tail; /* That's where the most popular are  */
	struct CacheElem *list_head; /* That's where the least popular are */
	struct CacheElem *hash;      /* HashTable for fast search of preloaded pages */
	struct CacheElem *readq;
	struct CacheElem *readq_tail;
	pthread_mutex_t   readq_lock;
};

int               cache_init         (struct CacheBase *cache, struct PagePool *pool,
				      size_t cache_size);
int               cache_free         (struct CacheBase *cache);
int 		  cache_print	     (struct CacheBase *cache);

struct CacheElem *cachei_page_alloc   (struct CacheBase *cache);
int 		  cachei_page_free    (struct CacheElem *elem );

#ifndef   LRU
#  define   LRU
#endif /* LRU */

#ifdef    LRU
#  include "lru.h"
#  define  cache_page_get(cache, page)  lru_page_get(cache, page)
#  define  cachei_page_get(cache, page) lrui_page_get(cache, page, 1)
#  define  cache_page_free(cache, page) lru_page_free(cache, page)
#endif /* LRU */

#endif  /* _BTREE_CACHE_H_ */
