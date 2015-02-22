#ifndef _BTREE_WAL_H_
#define _BTREE_WAL_H_

#include <stdint.h>
#include <stddef.h>

struct WAL {
	int fd;
	size_t page_size;
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

#endif /* _BTREE_WAL_H_ */
