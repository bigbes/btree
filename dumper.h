#ifndef   _BTREE_DUMPER_H_
#define   _BTREE_DUMPER_H_

#include <pthread.h>

#include "btree.h"
#include "cache.h"

struct Dumper {
	pthread_t         dumper_thread;
};

#endif /* _BTREE_DUMPER_H_ */
