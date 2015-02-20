#include "meta.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "btree.h"
#include "dbg.h"

int meta_check(char *db_name) {
	char meta[129] = {0};
	snprintf(meta, 129, "%s.meta", db_name);
	return access(meta, F_OK);
}

static int meta_prepare(char *db_name, int load) {
	char meta[129] = {0};
	snprintf(meta, 129, "%s.meta", db_name);
	int oflags = O_CREAT | O_WRONLY;
	if (load) oflags = O_RDONLY;
	int meta_fd = open(meta, oflags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	check(meta_fd != -1, "Failed to open file descriptor for meta");
	return meta_fd;
error:
	exit(-1);
}

int meta_dump(char *db_name, struct DBC *dbc) {
	int meta_fd = meta_prepare(db_name, 0);
	ssize_t retval = write(meta_fd, (void *)dbc, sizeof(struct DBC));
	check_diskw(retval, sizeof(struct DBC));
	meta_fd = close(meta_fd);
	check(meta_fd != -1, "Failed to close file descriptor for meta");
	return 0;
error:
	exit(-1);
}

int meta_load(char *db_name, struct DBC *dbc) {
	int meta_fd = meta_prepare(db_name, 1);
	ssize_t retval = read(meta_fd, (void *)dbc, sizeof(struct DBC));
	check_diskr(retval, sizeof(struct DBC));
	meta_fd = close(meta_fd);
	check(meta_fd != -1, "Failed to close file descriptor for meta");
	return 0;
error:
	exit(-1);
}
