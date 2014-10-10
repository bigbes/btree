all:
	gcc btree.c pagepool.c -std=c99 -shared -fPIC -o libmydb.so

gen_workload:
	@make -C example
	@python test/gen_workload.py --output workload
	@python test/runner.py --new --workload workload.in --so example/libdbsophia.so

gen_workload_custom:
	@python test/gen_workload.py --output workload --config example/example.schema.yml
	@python test/runner.py --new --workload workload.in --so example/libdbsophia.so

runner:
	@python test/runner.py --workload workload.in --so ./libmydb.so
