#include "wal.h"

#include <sys/uio.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "btree.h"
#include "dbg.h"
#include "cache.h"
#include <uthash.h>

static int wali_list_enqueue(struct WAL *wal, struct WALElem *elem) {
	pthread_mutex_lock (&wal->list_lock);
	if (!wal->list_tail) {
		wal->list_tail = wal->list_head = elem;
	} else {
		wal->list_tail->prev = elem;
		elem->next = wal->list_tail;
		wal->list_tail = elem;
	}
	pthread_mutex_unlock (&wal->list_lock);
	return 0;
}

static int wali_list_dequeue(struct WAL *wal) {
	pthread_mutex_lock (&wal->list_lock);
	if (wal->list_tail == wal->list_head) {
		wal->list_tail = wal->list_head = NULL;
	} else {
		wal->list_head = wal->list_head->prev;
		wal->list_head->next = NULL;
	}
	pthread_mutex_unlock (&wal->list_lock);
	return 0;
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
static int wali_write_begin(struct WAL *wal, int8_t op_type, void *key,
	            size_t key_size, void *val, size_t val_size) {
	struct WALHeader1 wal_line = WALHEADER1_INIT(.op = op_type,
						     .key_size = key_size,
						     .val_size = val_size);
	struct iovec vec[3];
	vec[0].iov_base = (void *)&wal_line;
	vec[0].iov_len  = sizeof(struct WALHeader1);
	vec[1].iov_base = key;
	vec[1].iov_len  = key_size;
	vec[2].iov_base = val;
	vec[2].iov_len  = val_size;

	struct WALElem elem = {
		.count = 3,
		.vec  = vec,
		.size = sizeof(struct WALHeader1) + key_size + val_size
	};
	pthread_cond_init  (&elem.written, NULL);
	pthread_mutex_init (&elem.used, NULL);
	pthread_mutex_lock (&elem.used);

	wali_list_enqueue (wal, &elem);
	pthread_cond_wait (&elem.written, &elem.used);

	pthread_mutex_unlock  (&elem.used);
	pthread_mutex_destroy (&elem.used);
	pthread_cond_destroy  (&elem.written);
	return 0;
}

/* struct WALHeader2 {
 * 	int32_t magic = 0xd5ab0bac;
 * }
 * 	char   *page_old;
 * 	char   *page_new;
 * */
static int wali_write_append(struct WAL *wal, struct NodeHeader *header_old,
		     struct NodeHeader *header_new) {
	struct WALHeader2 wal_line = WALHEADER2_INIT();
	struct iovec vec[3];
	vec[0].iov_base = (void *)&wal_line;
	vec[0].iov_len = sizeof(struct WALHeader2);
	vec[1].iov_base = header_old;
	vec[1].iov_len = wal->page_size;
	vec[2].iov_base = header_new;
	vec[2].iov_len = wal->page_size;

	struct WALElem elem = {
		.count = 3,
		.vec  = vec,
		.size = sizeof(struct WALHeader2) + 2 * wal->page_size
	};
	pthread_cond_init  (&elem.written, NULL);
	pthread_mutex_init (&elem.used, NULL);
	pthread_mutex_lock (&elem.used);

	wali_list_enqueue (wal, &elem);
	pthread_cond_wait (&elem.written, &elem.used);

	pthread_mutex_unlock  (&elem.used);
	pthread_mutex_destroy (&elem.used);
	pthread_cond_destroy  (&elem.written);
	return 0;

}

int wal_write_append(struct DB *db, pageno_t page) {
	struct CacheElem *elem = NULL;
	HASH_FIND_INT(db->pool->cache->hash, &page, elem);
	check(elem != NULL, "Can't find needed page")
	wali_write_append(db->wal, elem->cache, elem->prev);
	return elem->cache;
error:
	exit(-1);
}

/* struct WALHeader3 {
 * 	int32_t magic = 0xd5ab0bad;
 * } */
static int wali_write_finish(struct WAL *wal) {
	struct WALHeader3 wal_line = WALHEADER2_INIT();
	struct iovec vec[1];
	vec[0].iov_base = (void *)&wal_line;
	vec[0].iov_len = sizeof(struct WALHeader3);

	struct WALElem elem = {
		.count = 1,
		.vec  = vec,
		.size = sizeof(struct WALHeader3)
	};
	pthread_cond_init  (&elem.written, NULL);
	pthread_mutex_init (&elem.used, NULL);
	pthread_mutex_lock (&elem.used);

	wali_list_enqueue (wal, &elem);
	pthread_cond_wait (&elem.written, &elem.used);

	pthread_mutex_unlock  (&elem.used);
	pthread_mutex_destroy (&elem.used);
	pthread_cond_destroy  (&elem.written);
	return 0;
}

void *wal_loop(void *arg) {
	struct WAL *wal = (struct WAL *)arg;
	while (1) {
		usleep(1000);
		if (wal->list_head == NULL)
			continue;
		struct WALElem *elem = wal->list_head;
		pthread_mutex_lock(&elem->used);
		wali_list_dequeue(wal);
		int retval = writev(wal->fd, elem->vec, elem->count);
		check_diskw(retval, elem->size);
		pthread_cond_signal (&elem->written);
		pthread_mutex_unlock(&elem->used);
	}
	return NULL;
error:
	exit(-1);
}

int wal_init (struct DB *db, struct WAL *wal) {
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	char wal_name[129] = {0};
	snprintf(wal_name, 129, "%s.wal", db->db_name);
	wal->fd = open(wal_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	wal->page_size = db->pool->page_size;
	check(wal->fd != -1, "Failed to open file descriptor for WAL");
	wal->list_head = wal->list_tail = NULL;
	pthread_mutex_init(&wal->list_lock, NULL);
	pthread_create(&wal->thread, &attr, wal_loop, (void *)wal);

	pthread_attr_destroy(&attr);
	return 0;
error:
	exit(-1);
}

int wal_free(struct WAL *wal) {
	void *status;
	pthread_join(wal->thread, &status);
	if ((int )(*(int *)status) != 0)
		log_err("wal_loop exited with status %d", (int )(*(int *)status));
	pthread_mutex_destroy(&wal->list_lock);
	wal->fd = close(wal->fd);
	check(wal->fd != -1, "Failed to close file descriptor for WAL");
	wal->fd = 0;
error:
	exit(-1);
}
