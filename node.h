#ifndef _BTREE_NODE_H_
#define _BTREE_NODE_H_

#include "btree.h"

int  node_btree_load (struct DB *db, struct BTreeNode *node, pageno_t page);
int  node_data_load  (struct DB *db, struct DataNode *node, pageno_t page);
int  node_btree_dump (struct DB *db, struct BTreeNode *node);
int  node_data_dump  (struct DB *db, struct DataNode *node);
int  node_deallocate (struct DB *db, pageno_t pos);
void node_free       (struct DB *db, void *node);

#endif /* _BTREE_NODE_H_ */
