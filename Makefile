obj-m += hello.o
obj-m += hashtable_bench.o
obj-m += hashtable_crc32_test.o
obj-m += hashtable2_test.o
obj-m += htable.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)
CC ?= cc
HTABLE_TEST_CFLAGS ?= -std=gnu11 -g -O0 -Wall -Wextra -Werror

# Override MOD=<name> to target a specific module for insert/remove, e.g.:
#   make insert MOD=hashtable_bench
MOD ?= hello

.PHONY: all clean insert remove log bench htable-test

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

htable-test: htable-test/main.c htable/htable_list.h tests/include/linux/container_of.h
	$(CC) $(HTABLE_TEST_CFLAGS) -I. -Itests/include $< -o htable-test/htable-test

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) htable-test/htable-test

insert:
	sudo insmod $(MOD).ko $(ARGS)

remove:
	sudo rmmod $(MOD)

log:
	sudo dmesg | tail -n 20

# Convenience target to build & run the hashtable benchmark with optional
# NUM (num_entries) and BITS (hash_bits) overrides, then print the results.
NUM ?= 100000
BITS ?= 16

bench: all
	-sudo rmmod hashtable_bench 2>/dev/null
	sudo insmod hashtable_bench.ko num_entries=$(NUM) hash_bits=$(BITS)
	sudo dmesg | tail -n 10
	sudo rmmod hashtable_bench
