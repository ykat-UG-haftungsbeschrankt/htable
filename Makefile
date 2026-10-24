obj-m += htable.o

KDIR ?= /lib/modules/$(shell uname -r)/build
KERNEL_BUILD_DIR := $(realpath $(KDIR))
KERNEL_COMMON_CANDIDATE := $(patsubst %-amd64,%-common,$(KERNEL_BUILD_DIR))
KERNEL_COMMON_DIR ?= $(if $(wildcard $(KERNEL_COMMON_CANDIDATE)/include/linux/hashtable.h),$(KERNEL_COMMON_CANDIDATE),$(KERNEL_BUILD_DIR))
KERNEL_ARCH ?= x86
PWD := $(CURDIR)
CC ?= cc
HTABLE_TEST_CFLAGS ?= -std=gnu11 -g -O0 -Wall -Wextra -Werror -msse4.2
BENCH_CFLAGS ?= -std=gnu11 -O2 -Wall -Wextra -Werror -msse4.2
# Kernel-internal headers emit userspace-only warnings under -Wextra.
LINUX_BENCH_CFLAGS ?= -std=gnu11 -O2
LINUX_BENCH_CPPFLAGS := -D__KERNEL__ -DMODULE \
	-include $(KERNEL_COMMON_DIR)/include/linux/kconfig.h \
	-isystem $(KERNEL_COMMON_DIR)/include \
	-isystem $(KERNEL_COMMON_DIR)/arch/$(KERNEL_ARCH)/include \
	-isystem $(KERNEL_BUILD_DIR)/include \
	-isystem $(KERNEL_BUILD_DIR)/arch/$(KERNEL_ARCH)/include \
	-isystem $(KERNEL_BUILD_DIR)/arch/$(KERNEL_ARCH)/include/generated \
	-isystem $(KERNEL_BUILD_DIR)/include/generated \
	-isystem $(KERNEL_COMMON_DIR)/include/uapi \
	-isystem $(KERNEL_COMMON_DIR)/arch/$(KERNEL_ARCH)/include/uapi \
	-isystem $(KERNEL_BUILD_DIR)/include/generated/uapi \
	-isystem $(KERNEL_BUILD_DIR)/arch/$(KERNEL_ARCH)/include/generated/uapi
HTABLE_TEST_SOURCES := tests/htable.c tests/htable_list.c \
		tests/swar.c
HTABLE_TEST_HEADERS := htable/htable_index.h htable/htable_stats.h \
		htable/htable_list.h htable/htable_list_iter.h \
		htable/htable_types.h htable/swar.h htable/swar32.h \
		htable/swar64.h htable/swar128.h

# Override MOD=<name> to target a specific module for insert/remove.
MOD ?= htable

.PHONY: all clean insert remove log htable-test swar-test \
	htable-bench linux-hashtable-bench benchmark

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

htable-test: $(HTABLE_TEST_SOURCES) $(HTABLE_TEST_HEADERS)
	$(CC) $(HTABLE_TEST_CFLAGS) -I. $(HTABLE_TEST_SOURCES) \
		-o htable-test/htable-test

swar-test: tests/swar.c htable/htable_types.h htable/swar.h \
		htable/swar32.h htable/swar64.h htable/swar128.h
	$(CC) $(HTABLE_TEST_CFLAGS) -I. -DSWAR_TEST_STANDALONE $< \
		-o tests/swar-test

htable-bench: tests/htable_bench.c tests/htable_bench_common.h \
		htable/htable.h htable/htable_iter.h htable/htable_table_iter.h \
		htable/htable_table.h htable/htable_list.h htable/htable_index.h \
		htable/htable_stats.h htable/htable_types.h htable/swar.h \
		htable/swar32.h htable/swar64.h htable/swar128.h
	$(CC) $(BENCH_CFLAGS) -I. $< -o tests/htable-bench

linux-hashtable-bench: tests/linux_hashtable_bench.c \
		tests/htable_bench_common.h htable/htable_types.h \
		$(KERNEL_COMMON_DIR)/include/linux/hashtable.h
	$(CC) $(LINUX_BENCH_CFLAGS) $(LINUX_BENCH_CPPFLAGS) -I. $< \
		-o tests/linux-hashtable-bench

benchmark: htable-bench linux-hashtable-bench
	./tests/htable-bench
	./tests/linux-hashtable-bench

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) htable-test/htable-test tests/swar-test \
		tests/htable-bench tests/linux-hashtable-bench

insert:
	sudo insmod $(MOD).ko $(ARGS)

remove:
	sudo rmmod $(MOD)

log:
	sudo dmesg | tail -n 20
