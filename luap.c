/* Copyright (C) 2013 Papavasileiou Dimitris
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lualib.h>
#include <lauxlib.h>
#include <getopt.h>
#include <unistd.h>

#include "prompt.h"

#if !defined(LUA_INIT)
#define LUA_INIT "LUA_INIT"
#endif

#if LUA_VERSION_NUM == 502
#define LUA_INITVERSION  \
        LUA_INIT "_" LUA_VERSION_MAJOR "_" LUA_VERSION_MINOR
#endif

#if LUA_VERSION_NUM == 501
#define COPYRIGHT LUA_VERSION "  " LUA_COPYRIGHT
#else
#define COPYRIGHT LUA_COPYRIGHT
#endif

#define LUAP_VERSION "0.3.1"

static int interactive = 0, colorize = 1;

static void greet()
{
    fprintf(stdout, "%s" COPYRIGHT "\n%s"
            "luap " LUAP_VERSION " Copyright (C) 2013 Dimitris Papavasiliou\n",
            colorize ? "\033[1m" : "",
            colorize ? "\033[0m" : "");
}

static void dostring (lua_State *L, const char *string, const char *name)
{
    if (luaL_loadbuffer(L, string, strlen(string), name)) {
        fprintf(stderr,
                "%s%s%s\n",
                colorize ? "\033[0;31m" : "",
                lua_tostring (L, -1),
                colorize ? "\033[0m" : "");
        exit (EXIT_FAILURE);
    } else if (luap_call (L, 0)) {
        exit (EXIT_FAILURE);
    }
}

static void dofile (lua_State *L, const char *name)
{
    if (luaL_loadfile(L, name)) {
        fprintf(stderr,
                "%s%s%s\n",
                colorize ? "\033[0;31m" : "",
                lua_tostring (L, -1),
                colorize ? "\033[0m" : "");
        exit (EXIT_FAILURE);
    } else if (luap_call (L, 0)) {
        exit (EXIT_FAILURE);
    }
}

int main(int argc, char **argv)
{
    lua_State *L;
    char option;
    int done = 0;

    L = luaL_newstate();

    luap_setname (L, "lua");

    /* Open the standard libraries. */

    lua_gc(L, LUA_GCSTOP, 0);
    luaL_openlibs(L);
    lua_gc(L, LUA_GCRESTART, 0);

    if (L == NULL) {
        fprintf(stderr,
                "%s: Could not create Lua state (out of memory).\n",
                argv[0]);

        return EXIT_FAILURE;
    }

    if (!isatty (STDOUT_FILENO) ||
        !isatty (STDERR_FILENO)) {
        colorize = 0;
    }

    luap_setcolor (L, colorize);

    /* Take care of the LUA_INIT environment variable. */

    {
        const char *name, *init;

#if LUA_VERSION_NUM == 502
        name = "=" LUA_INITVERSION;
        init = getenv(name + 1);

        if (init == NULL) {
#endif
            name = "=" LUA_INIT;
            init = getenv(name + 1);
#if LUA_VERSION_NUM == 502
        }
#endif

        if (init) {
            if (init[0] == '@') {
                dofile(L, init + 1);
            } else {
                dostring(L, init, name);
            }
        }
    }

    /* Parse the command line. */

    while ((option = getopt (argc, argv, "ivphe:l:")) != -1) {
        if (option == 'i') {
            interactive = 1;
        } else if (option == 'v') {
            greet();
            done = 1;
        } else if (option == 'p') {
            colorize = 0;
            luap_setcolor (L, colorize);
        } else if (option == 'e') {
            dostring (L, optarg, "=(command line)");
            done = 1;
        } else if (option == 'l') {
            lua_getglobal (L, "require");
            lua_pushstring (L, optarg);
            if (luap_call (L, 1)) {
                return EXIT_FAILURE;
            } else {
                lua_setglobal (L, optarg);
            }
        } else if (option == 'h') {
            printf (
                "Usage: %s [OPTION...] [[SCRIPT] ARGS]\n\n"
                "Options:\n"
                "  -h       Display this help message\n"
                "  -e STMT  Execute string 'STMT'\n"
                "  -l NAME  Require library 'NAME'\n"
                "  -p       Force plain, uncolored output\n"
                "  -v       Print version information\n"
                "  -i       Enter interactive mode after executing SCRIPT\n",
                argv[0]);

            done = 1;
        } else {
            exit(1);
        }
    }

    if (argc > optind) {
        char *name;
        int i;

        if (!strcmp (argv[optind], "-")) {
            name = NULL;
        } else {
            name = argv[optind];
        }

        /* Collect all command line arguments into a table. */

        lua_createtable (L, argc - optind - 1, optind + 1);

        for (i = 0 ; i <= argc ; i += 1) {
            lua_pushstring (L, argv[i]);
            lua_rawseti (L, -2, i - optind);
        }

        lua_setglobal (L, "arg");

        /* Load the script and call it. */

        dofile (L, name);

        done = 1;
    }

    if (!done || interactive) {
        if (isatty (STDIN_FILENO)) {
            char *home;

            greet();
            fprintf (stdout, "\n");

            home = getenv("HOME");

            {
                char path[strlen(home) + sizeof("/.lua_history")];

                strcpy(path, home);
                strcat(path, "/.lua_history");

                luap_sethistory (L, path);
            }

            luap_setprompts (L, ">  ", ">> ");
            luap_enter(L);
        } else {
            dofile (L, NULL);
        }
    }

    lua_close(L);

    return EXIT_SUCCESS;
}
