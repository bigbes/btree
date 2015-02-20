#ifndef _BTREE_META_H_
#define _BTREE_META_H_

#include "btree.h"

int meta_check(char *db_name);
int meta_load(char *db_name, struct DBC *dbc);
int meta_dump(char *db_name, struct DBC *dbc);

#endif /* _BTREE_META_H_ */
