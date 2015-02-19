#include <string.h>

#include "node.h"
#include "btree.h"
#include "search.h"

/**
 * @brief  B-Tree search operation
 *
 * @return Status
 */
pageno_t btreei_search(struct DB *db, struct BTreeNode *node,
		       void *key, size_t *val_len) {
	*val_len = 0;
	size_t pos = 0;
	int cmp = 0;
	while (pos < node->h->size) {
		cmp = strncmp(NODE_KEY_POS(node, pos), key, BTREE_KEY_LEN);
		if (cmp >= 0)
			break;
		pos++;
	}
	if (pos < node->h->size && cmp == 0)
		return node->vals[pos];
	if (node->h->flags & IS_LEAF)
		return 0;
	struct BTreeNode kid;
	node_btree_load(db, &kid, node->chld[pos]);
	pageno_t ret = btreei_search(db, &kid, key, val_len);
	node_free(db, &kid);
	return ret;
}
