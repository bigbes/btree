#ifndef _BTREE_INSERT_H_
#define _BTREE_INSERT_H_

int btreei_insert(struct DB *db, struct BTreeNode *node,
		  char *key, char *val, int val_len);

#endif /* _BTREE_INSERT_H_ */
