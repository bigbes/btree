all:
	gcc btree.c pagepool.c cache.c lru.c \
		node.c meta.c wal.c              \
		search.c insert.c delete.c       \
		-std=c99 -g -O0 -ggdb -Wall      \
		-I./third_party/
lib:
	gcc btree.c pagepool.c cache.c lru.c \
		node.c meta.c wal.c              \
		search.c insert.c delete.c       \
		-std=c99 -g -O0 -ggdb -Wall      \
		-shared -fPIC -I./third_party/   \
		-o libmydb.so
libopt:
	gcc btree.c pagepool.c cache.c lru.c \
		node.c meta.c wal.c              \
		search.c insert.c delete.c       \
		-std=c99 -DNDEBUG -O2 -Wall      \
		-shared -fPIC -I./third_party/   \
		-o libmydb.so
