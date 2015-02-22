#include "wal.h"

#include <sys/uio.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "btree.h"
#include "dbg.h"

int wal_init (struct WAL *wal, char *db_name, size_t page_size) {
	char wal_name[129] = {0};
	snprintf(wal_name, 129, "%s.wal", db_name);
	wal->fd = open(wal_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	wal->page_size = page_size;
	check(wal->fd != -1, "Failed to open file descriptor for WAL");
	return 0;
error:
	exit(-1);
}

/* struct WALHeader1 {
 * 	int32_t magic = 0xd5ab0bab;
 * 	int8_t  op;
 * 	size_t  key_size;
 * 	size_t  val_size;
 * }
 * 	char   *key;
 * 	char   *val;
 * */
int wal_write_begin(struct WAL *wal, int8_t op_type, void *key,
	            size_t key_size, void *val, size_t val_size) {
	struct WALHeader1 wal_line = WALHEADER1_INIT(
			.op=op_type, .key_size = key_size, .val_size = val_size);
	struct iovec vec[3];
	vec[0].iov_base = (void *)&wal_line;
	vec[0].iov_len = sizeof(struct WALHeader1);
	vec[1].iov_base = key;
	vec[1].iov_len = key_size;
	vec[2].iov_base = val;
	vec[2].iov_len = val_size;
	int retval = writev(wal->fd, vec, 3);
	check_diskw(retval, sizeof(struct WALHeader1) + key_size + val_size);
	return 0;
error:
	exit(-1);
}

/* struct WALHeader2 {
 * 	int32_t magic = 0xd5ab0bac;
 * }
 * 	char   *page_old;
 * 	char   *page_new;
 * */
int wal_write_append(struct WAL *wal, struct NodeHeader *header_old,
		     struct NodeHeader *header_new) {
	struct WALHeader2 wal_line = WALHEADER2_INIT();
	struct iovec vec[3];
	vec[0].iov_base = (void *)&wal_line;
	vec[0].iov_len = sizeof(struct WALHeader2);
	vec[1].iov_base = header_old;
	vec[1].iov_len = wal->page_size;
	vec[2].iov_base = header_new;
	vec[2].iov_len = wal->page_size;
	int retval = writev(wal->fd, vec, 3);
	check_diskw(retval, sizeof(struct WALHeader2) + 2 * wal->page_size);
	return 0;
error:
	exit(-1);

}

/* struct WALHeader3 {
 * 	int32_t magic = 0xd5ab0bad;
 * } */
int wal_write_finish(struct WAL *wal) {
	struct WALHeader3 wal_line = WALHEADER2_INIT();
	int retval = write(wal->fd, &wal_line, sizeof(struct WALHeader3));
	check_diskw(retval, sizeof(struct WALHeader3));
	return 0;
error:
	exit(-1);
}

int wal_free(struct WAL *wal) {
	wal->fd = close(wal->fd);
	check(wal->fd != -1, "Failed to close file descriptor for WAL");
	wal->fd = 0;
error:
	exit(-1);
}
