#ifndef _BTREE_META_H_
#define _BTREE_META_H_

#include "btree.h"

struct Metadata {
	size_t pool_size;
	size_t page_size;
	pageno_t header_page;
};

int meta_check(char *db_name);
int meta_load(char *db_name, struct Metadata *md);
int meta_dump(char *db_name, struct Metadata *md);

#endif /* _BTREE_META_H_ */
