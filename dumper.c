#include "dumper.h"

#include "btree.h"
#include "dbg.h"

#include <pthread.h>
#include <errno.h>

static int dumperi_readq_check(struct CacheBase *cache) {
	pthread_mutex_lock(&cache->readq_lock);
	int res = (cache->readq != NULL);
	pthread_mutex_unlock(&cache->readq_lock);
	return res;
}

int dumper_readq_enqueue(struct CacheBase *cache, struct CacheElem *elem) {
	pthread_mutex_lock(&cache->readq_lock);
	if (cache->readq == NULL) {
		cache->readq = cache->readq_tail = elem;
	} else {
		cache->readq_tail->rq_next = elem;
		cache->readq_tail = elem;
	}
	pthread_mutex_unlock(&cache->readq_lock);
	return 0;
}
/*
 * Dequeue head element from read_queue;
 */
static int dumperi_readq_dequeue(struct CacheBase *cache) {
	pthread_mutex_lock(&cache->readq_lock);
	struct CacheElem *elem = cache->readq;
	if (cache->readq == NULL) {
	} else if (cache->readq->rq_next == NULL) {
		cache->readq = cache->readq_tail = NULL;
	} else {
		cache->readq = cache->readq->next;
		elem->rq_next = NULL;
	}
	pthread_mutex_unlock(&cache->readq_lock);
	return 0;
}

static int dumper_page_dump(struct CacheBase *cache, struct CacheElem *elem) {
	int retval = pool_write(cache->pool, elem->cache, cache->pool->page_size, elem->id, 0);
	log_info("Page %zd has been dumped", elem->id);
	return retval;
}

static int dumper_page_load(struct CacheBase *cache, struct CacheElem *elem) {
	int retval = pool_read(cache->pool, elem->id, elem->cache);
	memcpy(elem->prev, elem->cache, cache->pool->page_size);
	pthread_cond_signal(&elem->rw_signal);
	log_info("Page %zd has been loaded", elem->id);
	return retval;	
}

#ifndef   pthread_mutex_timedlock
#include <sys/time.h>
int pthread_mutex_timedlock (pthread_mutex_t *mutex,
			     struct timespec *timeout) {
	struct timeval timenow;
	struct timespec sleepytime;
	int retcode;

	/* This is just to avoid a completely busy wait */
	sleepytime.tv_sec = 0;
	sleepytime.tv_nsec = 100000;

	while ((retcode = pthread_mutex_trylock (mutex)) == EBUSY) {
		gettimeofday (&timenow, NULL);

		if (timenow.tv_sec >= timeout->tv_sec &&
				(timenow.tv_usec * 1000) >= timeout->tv_nsec) {
			return ETIMEDOUT;
		}
	nanosleep (&sleepytime, NULL);
	}
	return retcode;
}
#endif /* pthread_mutex_timedlock */

void *dumper_loop(void *arg) {
	int *procret = calloc(1, sizeof(int));
	struct timespec ts = {.tv_sec = 0, .tv_nsec = 200000};

	log_info("Creating RW Thread");
	struct PagePool *pp = (struct PagePool *)arg;
	struct CacheElem *elem_w = pp->cache->list_tail;
	while (pp->dumper_enable) {
		elem_w = elem_w->next;
		if (elem_w == NULL)
			elem_w =  pp->cache->list_tail;
		if (elem_w->flag & CACHE_DIRTY) {
			int retval = pthread_mutex_timedlock(&elem_w->lock, &ts);
			if (retval == ETIMEDOUT) continue;
			check(retval == 0, "Failed to lock mutex");
			dumper_page_dump(pp->cache, elem_w);
			elem_w->flag &= (-1 - CACHE_DIRTY);
			pthread_mutex_unlock(&elem_w->lock);
		}
		while (dumperi_readq_check(pp->cache)) {
			struct CacheElem *elem_r = pp->cache->readq;
			pthread_mutex_lock(&elem_r->lock);
			dumper_page_load(pp->cache, elem_r);
			dumperi_readq_dequeue(pp->cache);
			pthread_cond_signal(&elem_r->rw_signal);
			pthread_mutex_unlock(&elem_r->lock);
		}
	}
	elem_w = pp->cache->list_tail;
	while (elem_w) {
		if (elem_w->flag & CACHE_DIRTY) {
			pthread_mutex_lock(&elem_w->lock);
			dumper_page_dump(pp->cache, elem_w);
			elem_w->flag &= (-1 - CACHE_DIRTY);
			pthread_mutex_unlock(&elem_w->lock);
		}
		elem_w = elem_w->next;
	}
	return procret;
error:
	*procret = 1;
	return procret;
}

int dumper_init (struct DB *db, struct PagePool *pp) {
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	pp->dumper_enable = 1;

	pthread_create(&pp->dumper, &attr, dumper_loop, (void *)pp);

	pthread_attr_destroy(&attr);
	return 0;
}

int dumper_free (struct PagePool *pp) {
	void *status;
	pthread_mutex_lock(&pp->cache->readq_lock);
	pp->dumper_enable = 0;
	pthread_mutex_unlock(&pp->cache->readq_lock);
	pthread_join(pp->dumper, &status);
	log_err("dumper_loop exited with status %d", (int )(*(int *)status));
	free(status);
	return 0;
}
