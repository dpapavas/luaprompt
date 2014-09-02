PREFIX=/usr/local
BINDIR=${PREFIX}/bin
LIBDIR=$(PREFIX)/lib/lua/5.2

LUA_CFLAGS=`pkg-config --cflags lua5.2`
LUA_LDFLAGS=`pkg-config --libs lua5.2`

CFLAGS= -g -Wall -Wextra -Wno-unused-parameter -I.. -DHAVE_ASPRINTF
CFLAGS+= -DHAVE_LIBREADLINE -DHAVE_READLINE_READLINE_H -DHAVE_READLINE_HISTORY -DHAVE_READLINE_HISTORY_H
CFLAGS+= -D_GNU_SOURCE

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
