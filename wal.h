#ifndef _BTREE_WAL_H_
#define _BTREE_WAL_H_

#include "btree.h"

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

struct WALElem {
	int count;
	size_t size;
	struct iovec *vec;
	pthread_cond_t  written;
	pthread_mutex_t used;
	struct WALElem *next;
	struct WALElem *prev;
};

struct WAL {
	int fd;
	size_t page_size;
	pthread_t thread;
	struct WALElem *list_head;
	struct WALElem *list_tail;
	pthread_mutex_t list_lock;
};

struct WALHeader1 {
	int32_t  magic; /* 0xd5ab0bab */
	int8_t   op;
#define OP_INSERT 0x00
#define OP_DELETE 0x01
	int8_t   key_size;
	int64_t  val_size;
};

struct WALHeader2 {
	int32_t magic; /* 0xd5ab0bac */
};

struct WALHeader3 {
	int32_t magic; /* 0xd5ab0bad */
};

#define WALHEADER1_INIT(...) { .magic = 0xd5ab0bab, ## __VA_ARGS__ }
#define WALHEADER2_INIT(...) { .magic = 0xd5ab0bac, ## __VA_ARGS__ }
#define WALHEADER3_INIT(...) { .magic = 0xd5ab0bad, ## __VA_ARGS__ }

int wal_write_begin(struct DB *db, int8_t op_type, void *key,
		size_t key_size, void *val, size_t val_size);
int wal_write_append(struct DB *db, pageno_t page);
int wal_write_finish(struct DB *db);
int wal_init (struct DB *db, struct WAL *wal);
int wal_free(struct WAL *wal);

#endif /* _BTREE_WAL_H_ */
