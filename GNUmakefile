#   GNUmakefile for pthread priority queues.
#   Author: Jens Schweikhardt

APP_C_SOURCE = pq.c
APP_H_SOURCE = pq.h
TST_C_SOURCE = test_pq.c unity.c
TST_H_SOURCE = unity.h unity_internals.h

#   CFLAGS: Flags only meaningful to the compiler:
#   These are understood by gcc and clang.
#
CFLAGS = -Wall -Wextra
CFLAGS += -g -std=c99 -pedantic
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wno-unused-parameter
CFLAGS += -D_XOPEN_SOURCE=600
#CFLAGS += -D_POSIX_VERSION=200809L

#   LDFLAGS: Flags only meaningful to the linker.
#
LDFLAGS = -lpthread

#   Manual page source files. These use the mandoc macros.
#
MAN3  := pq_create.3 pq_destroy.3 \
         pq_recv_nonbl.3 pq_recv_timed.3 \
         pq_send_nonbl.3 pq_send_timed.3

#   Manual pages ready for terminal, with ESC sequences.
#
TXT   := $(addsuffix .txt,$(MAN3))

#   Manual pages converted to markdown.
#
MD    := $(addsuffix .md,$(MAN3))

#   A literal ASCII escape charater.
#
ESC   := $(shell printf '\033')

################################################################################

#   How to turn manual page source into text for terminal.
#
%.3.txt: %.3
	groff -mandoc -Tutf8 $^ > $@

#   How to convert manual to markdown.
#
%.3.md: %.3.txt
	{ \
	echo '### $*';                                                \
	echo '<pre a="#$*">';                                         \
	sed -e 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g'               \
	    -e 's,$(ESC)\[1m\([^$(ESC)]*\)$(ESC)\[0m,<b>\1</b>,g'     \
	    -e 's,$(ESC)\[4m\([^$(ESC)]*\)$(ESC)\[24m,<i>\1</i>,g'    \
	    -e 's,$(ESC)\[1m\([^$(ESC)]*\)$(ESC)\[22m,<i>\1</i>,g'    \
	    -e 's,$(ESC)\[4m\([^$(ESC)]*\)$(ESC)\[0m,<i>\1</i>,g' $^; \
	echo '</pre>';                                                \
	} > $@

################################################################################

.PHONY: all
all: test docs

test_pq: pq.o test_pq.o unity.o

.PHONY: test
test: test_pq
	./$^

.PHONY: docs
docs: README.xhtml

README.xhtml: README.md
	pandoc -s -o $@ $^

README.md: BLURB.md $(MD)
	cat $^ > $@

.PHONY: clean
clean:
	rm -f *.o test_pq

.PHONY: lint
lint: $(APP_C_SOURCE)
	flexelint lint.lnt $^
	flexelint lint.lnt test_pq.c pq.c

.PHONY: tags
tags: $(APP_C_SOURCE) $(APP_H_SOURCE) /usr/include /usr/include/sys /usr/include/x86
	exctags -f $@ $^
	exctags -f $@.p --c-kinds=p $^

depend: $(APP_C_SOURCE) $(TST_C_SOURCE)
	$(CC) -MM $(APP_C_SOURCE) > depend
	$(CC) -MM $(TST_C_SOURCE) >> depend

sinclude depend

# vim: set syntax=make noexpandtab tabstop=8 sw=2:
