PREFIX=/usr/local
BINDIR=${PREFIX}/bin
LIBDIR=$(PREFIX)/lib/lua/5.2

LUA_CFLAGS=`pkg-config --cflags lua5.2`
LUA_LDFLAGS=`pkg-config --libs lua5.2`

CFLAGS= -g -Wall -Wextra -Wno-unused-parameter -I.. -DHAVE_ASPRINTF
CFLAGS+= -DHAVE_LIBREADLINE -DHAVE_READLINE_READLINE_H -DHAVE_READLINE_HISTORY -DHAVE_READLINE_HISTORY_H
CFLAGS+= -D_GNU_SOURCE

# Uncomment the following line and customize the prefix as desired to
# keep the auto-completer from considering certain table keys (and
# hence global variables) for completion.

# CFLAGS+= '-DHIDDEN_KEY_PREFIX="_"'

# When completing certain kinds of values, such as tables or
# functions, the completer also appends certain useful suffixes such
# as '.', '[' or '('. Normally these are appended only when the
# value's name has already been fully entered, or previously fully
# completed, so that one can still complete the name without the
# suffix.  In order to append the suffix one then only has to press
# the completion key one more time.
#
# Uncomment the following line to make the completer always append
# these suffixes.

# CFLAGS+= -DALWAYS_APPEND_SUFFIXES

LDFLAGS=-lreadline -lhistory

INSTALL=/usr/bin/install

all: luap

luap: luap.c prompt.c prompt.h
	$(CC) -o luap ${CFLAGS} ${LUA_CFLAGS} luap.c prompt.c ${LDFLAGS} ${LUA_LDFLAGS}

dist: luap
	if [ -e /tmp/prompt ]; then rm -rf /tmp/prompt; fi
	mkdir /tmp/prompt
	cp luap.c Makefile prompt.c prompt.h README ChangeLog /tmp/prompt
	cd /tmp; tar zcf luaprompt.tar.gz prompt/

install: luap
	if [ -e luap ]; then $(INSTALL) luap $(BINDIR)/; fi

uninstall:
	rm -f $(BINDIR)/luap

clean:
	rm -f luap *~
