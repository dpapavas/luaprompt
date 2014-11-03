PREFIX=/usr/local
BINDIR=${PREFIX}/bin
LIBDIR=$(PREFIX)/lib/lua/5.2

LUA_CFLAGS=`pkg-config --cflags lua5.2`
LUA_LDFLAGS=`pkg-config --libs lua5.2`

CFLAGS= -g -Wall -Wextra -Wno-unused-parameter -I.. -DHAVE_ASPRINTF
CFLAGS+= -DHAVE_LIBREADLINE -DHAVE_READLINE_READLINE_H -DHAVE_READLINE_HISTORY -DHAVE_READLINE_HISTORY_H
CFLAGS+= -D_GNU_SOURCE

# Comment out the following to suppress completion of certain kinds of
# symbols.

CFLAGS+= -DCOMPLETE_KEYWORDS	# Keywords such as for, while, etc.
CFLAGS+= -DCOMPLETE_MODULES     # Module names.
CFLAGS+= -DCOMPLETE_TABLE_KEYS	# Table keys, including global variables.
CFLAGS+= -DCOMPLETE_FILE_NAMES	# File names.

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

# The autocompleter can complete module names as if they were already
# require'd and available as a global variable.  Once the module name
# is fully completed a further tab press loads the module and exports
# it as a global variable so that all further tab-completions now
# apply to the module's table.
#
# Uncomment the following line to disable this functionality.  Module
# names will then only be completed inside strings (for use with
# require).

# CFLAGS+= -DNO_MODULE_LOAD

# Uncomment to make the auto-completer ask for confirmation before
# loading a module.

# CFLAGS+= -DCONFIRM_MODULE_LOAD

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
