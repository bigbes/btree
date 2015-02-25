#ifndef   _BTREE_LRU_H_
#define   _BTREE_LRU_H_
void *lru_page_get (struct CacheBase *cache, pageno_t page);
struct CacheElem *lrui_page_get (struct CacheBase *cache, pageno_t page, int nolock);
int   lru_page_free(struct CacheBase *cache, pageno_t page);
#endif /* _BTREE_LRU_H_ */
