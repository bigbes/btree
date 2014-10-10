all:
	@$(MAKE) -C third_party/sophia
	cp third_party/sophia/db/libsophia.a ./
	gcc bdb_sphia.c -std=c99 -shared -fPIC -o libdbsophia.so -lsophia -L. -Wall

gen_workload:
	@python ../test/gen_workload.py --output workload
	@python ../test/runner.py --new --workload workload.in --so ./libdbsophia.so

gen_workload_custom:
	@python ../test/gen_workload.py --output workload --config example.schema.yml
	@python ../test/runner.py --new --workload workload.in --so ./libdbsophia.so

runner:
	@python ../test/runner.py --workload workload.in --so ./libdbsophia.so
