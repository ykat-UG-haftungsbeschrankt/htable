obj-m += htable.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)
CC ?= cc
HTABLE_TEST_CFLAGS ?= -std=gnu11 -g -O0 -Wall -Wextra -Werror -msse4.2
HTABLE_TEST_SOURCES := tests/htable.c tests/htable_list.c \
		tests/swar.c
HTABLE_TEST_HEADERS := htable/htable_index.h htable/htable_stats.h \
		htable/htable_list.h htable/htable_list_iter.h \
		htable/htable_types.h htable/swar.h htable/swar32.h \
		htable/swar64.h htable/swar128.h

# Override MOD=<name> to target a specific module for insert/remove.
MOD ?= htable

.PHONY: all clean insert remove log htable-test swar-test

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

htable-test: $(HTABLE_TEST_SOURCES) $(HTABLE_TEST_HEADERS)
	$(CC) $(HTABLE_TEST_CFLAGS) -I. $(HTABLE_TEST_SOURCES) \
		-o htable-test/htable-test

swar-test: tests/swar.c htable/htable_types.h htable/swar.h \
		htable/swar32.h htable/swar64.h htable/swar128.h
	$(CC) $(HTABLE_TEST_CFLAGS) -I. -DSWAR_TEST_STANDALONE $< \
		-o tests/swar-test

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) htable-test/htable-test tests/swar-test

insert:
	sudo insmod $(MOD).ko $(ARGS)

remove:
	sudo rmmod $(MOD)

log:
	sudo dmesg | tail -n 20
