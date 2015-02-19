#ifndef _BTREE_SEARCH_H_
#define _BTREE_SEARCH_H_

pageno_t btreei_search(struct DB *db, struct BTreeNode *node,
		       void *key, size_t *val_len);

#endif /* _BTREE_SEARCH_H_ */
