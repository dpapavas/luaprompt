/* Copyright (C) 2012 Papavasileiou Dimitris
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#ifdef HAVE_IOCTL
#include <sys/ioctl.h>
#endif

#include <lualib.h>
#include <lauxlib.h>

#include "prompt.h"

#if LUA_VERSION_NUM == 501
#define lua_pushglobaltable(__L) lua_pushvalue (__L, LUA_GLOBALSINDEX)
#define LUA_OK 0
#define lua_rawlen lua_objlen
#endif

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)
#    include <readline.h>
#  else /* !defined(HAVE_READLINE_H) */
extern char *readline ();
#  endif /* !defined(HAVE_READLINE_H) */
#else

/* This is a simple readline-like function in case readline is not
 * available. */

#define MAXINPUT 1024

static char *readline(char *prompt)
{
    char *line = NULL;
    int k;

    line = malloc (MAXINPUT);

    fputs(prompt, stdout);
    fflush(stdout);

    if (!fgets(line, MAXINPUT, stdin)) {
        return NULL;
    }

    k = strlen (line);

    if (line[k - 1] == '\n') {
        line[k - 1] = '\0';
    }

    return line;
}

#endif /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else /* !defined(HAVE_HISTORY_H) */
extern void add_history ();
extern int write_history ();
extern int read_history ();
#  endif /* defined(HAVE_READLINE_HISTORY_H) */
#endif /* HAVE_READLINE_HISTORY */

static char *logfile;

#define print_output(...) fprintf (stdout, __VA_ARGS__), fflush(stdout)
#define print_error(...) fprintf (stderr, __VA_ARGS__), fflush(stderr)
#define absolute(_L, _i) (_i < 0 ? lua_gettop (_L) + _i + 1 : _i)

static int colorize = 1;
static const char *colors[] = {"\033[0m",
                               "\033[0;31m",
                               "\033[1;31m",
                               "\033[0;32m",
                               "\033[1;32m",
                               "\033[0;33m",
                               "\033[1;33m",
                               "\033[1m",
                               "\033[22m"};

#define COLOR(i) (colorize ? colors[i] : "")

#if LUA_VERSION_NUM == 501
#define EOF_MARKER "'<eof>'"
#else
#define EOF_MARKER "<eof>"
#endif

static lua_State *_L;
static int initialized = 0;
static char *chunkname, *prompts[2], *buffer = NULL;

#ifdef HAVE_LIBREADLINE

static void display_matches (char **matches, int num_matches, int max_length)
{
    print_output ("%s", COLOR(7));
    rl_display_match_list (matches, num_matches, max_length);
    print_output ("%s", COLOR(0));
    rl_on_new_line ();
}

static char *keyword_completions (const char *text, int state)
{
    static const char **c, *keywords[] = {
#if LUA_VERSION_NUM == 502
        "goto",
#endif
        "and", "break", "do", "else", "elseif", "end", "false", "for",
        "function", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while", NULL
    };

    int s, t;

    if (state == 0) {
        c = keywords - 1;
    }

    /* Loop through the list of keywords and return the ones that
     * match. */

    for (c += 1 ; *c ; c += 1) {
        s = strlen (*c);
        t = strlen(text);

        if (s >= t && !strncmp (*c, text, t)) {
            return strdup (*c);
        }
    }

    return NULL;
}

static char *table_key_completions (const char *text, int state)
{
    static const char *c, *token;
    static char oper;

    if (state == 0) {
        /* Scan to the beginning of the to-be-completed token. */

        for (c = text + strlen (text) - 1;
             c >= text && *c != '.' && *c != ':' && *c != '[';
             c -= 1);

        if (c > text) {
            oper = *c;
            token = c + 1;

            /* Get the iterable value, the keys of which we wish to
             * complete. */

            lua_pushliteral (_L, "return ");
            lua_pushlstring (_L, text, token - text - 1);
            lua_concat (_L, 2);

            if (luaL_loadstring (_L, lua_tostring (_L, -1)) ||
                lua_pcall (_L, 0, 1, 0) ||
                (lua_type (_L, -1) != LUA_TUSERDATA &&
                 lua_type (_L, -1) != LUA_TTABLE)) {

                return NULL;
            }
        } else {
            oper = 0;
            token = text;

            lua_pushglobaltable(_L);
        }

        /* Call the standard pairs function. */

        lua_getglobal (_L, "pairs");
        lua_insert (_L, -2);
        if(lua_type (_L, -2) != LUA_TFUNCTION ||
           lua_pcall (_L, 1, 3, 0)) {
            return NULL;
        }
    }

    /* Iterate the table/userdata and generate matches. */

    while (lua_pushvalue(_L, -3), lua_insert (_L, -3),
           lua_pushvalue(_L, -2), lua_insert (_L, -4),
           lua_pcall (_L, 2, 2, 0) == 0) {
        char *candidate;
        size_t l, m;
        int nospace;

        if (lua_isnil(_L, -2)) {
            return NULL;
        }

        /* Pop the value, keep the key. */

        nospace = (lua_type (_L, -1) == LUA_TTABLE ||
                   lua_type (_L, -1) == LUA_TUSERDATA);

        lua_pop (_L, 1);

        /* We're mainly interested in strings at this point but if
         * we're completing for the table[key] syntax we consider
         * numeric keys too. */

        if (lua_type (_L, -1) == LUA_TSTRING ||
            (oper == '[' && lua_type (_L, -1) == LUA_TNUMBER)) {
            if (oper == '[') {
                if (lua_type (_L, -1) == LUA_TNUMBER) {
                    lua_Number n;
                    int i;

                    n = lua_tonumber (_L, -1);
                    i = lua_tointeger (_L, -1);

                    /* If this isn't an integer key, we may as well
                     * forget about it. */

                    if ((lua_Number)i == n) {
                        l = asprintf (&candidate, "%d]", i);
                    } else {
                        continue;
                    }
                } else {
                    char q;

                    q = token[0];
                    if (q != '"' && q != '\'') {
                        q = '"';
                    }

                    l = asprintf (&candidate, "%c%s%c]",
                                  q, lua_tostring (_L, -1), q);
                }
            } else {
                candidate = strdup((char *)lua_tolstring (_L, -1, &l));
            }

            m = strlen(token);

            if (l >= m && !strncmp (token, candidate, m)) {
                char *match;

                if (token > text) {
                    /* Were not completing a global variable.  Put the
                     * completed string together out of the table and
                     * the key. */

                    match = (char *)malloc ((token - text) + l + 1);
                    strncpy (match, text, token - text);
                    strcpy (match + (token - text), candidate);

                    free(candidate);
                } else {
                    /* Return the whole candidate as is, to be freed
                     * by Readline. */

                    match = candidate;
                }

                /* Suppress the newline when completing a table
                 * or other potentially complex value. */

                if (nospace) {
                    rl_completion_suppress_append = 1;
                }

                return match;
            } else {
                free(candidate);
            }
        }
    }

    return NULL;
}

static char *generator (const char *text, int state)
{
    static int which, completed;
    char *match = NULL;

    if (state == 0) {
        which = 0;
        completed = 0;
    }

    /* Try to complete a keyword. */

    if (which == 0) {
        match = keyword_completions (text, state);

        if (!match /* && !completed */) {
            which = 1;
            state = 0;
        } else {
            completed = 1;
        }
    }

    /* Try to complete a table access. */

    if (which == 1) {
        match = table_key_completions (text, state);

        if (!match && !completed) {
            which = 2;
            state = 0;
        } else {
            completed = 1;
        }
    }

    /* Try to complete a filename. */

    if (which == 2) {
        const char *start;

        /* Ignore any unquoted characters, we'll only be trying to
         * complete file names inside strings. */

        for (start = text;
             *start && (start == text ||
                        (*(start - 1) != '\'' && *(start - 1) != '"'));
             start += 1);

        if (*start != '\0') {
            int n;

            /* Don't append a space at the end of the match.  It isn't
             * very helpful in this context. */

            rl_completion_suppress_append = 1;

            match = rl_filename_completion_function (start, state);
            if (match) {
                /* If a match was produced, prepend the unquoted
                 * characters. */

                n = strlen (match) + 1;

                match = (char *)realloc (match, n + start - text);
                memmove (match + (start - text), match, n);
                strncpy (match, text, start - text);
            }
        }
    }

    return match;
}

static char **complete (const char *text, int start, int end)
{
    char **matches;
    int h;

    h = lua_gettop (_L);
    rl_completion_suppress_append = 0;
    matches = rl_completion_matches (text, generator);
    lua_settop (_L, h);

    return matches;
}
#endif

static void finish ()
{
#ifdef HAVE_READLINE_HISTORY
    /* Save the command history on exit. */

    if (logfile) {
        write_history (logfile);
    }
#endif
}

static int traceback(lua_State *L)
{
    lua_Debug ar;
    int i;

    if (lua_isnoneornil (L, 1) ||
        (!lua_isstring (L, 1) &&
         !luaL_callmeta(L, 1, "__tostring"))) {
        lua_pushliteral(L, "(no error message)");
    }

    if (lua_gettop (L) > 1) {
        lua_replace (L, 1);
        lua_settop (L, 1);
    }

    /* Print the Lua stack. */

    lua_pushstring(L, "\n\nStack trace:\n");

    for (i = 0 ; lua_getstack (L, i, &ar) ; i += 1) {
        lua_getinfo(_L, "Snl", &ar);

        if (!strcmp (ar.what, "C")) {
            lua_pushfstring(L, "\t#%d %s[C]:%s in function '%s%s%s'\n",
                            i, COLOR(7), COLOR(8), COLOR(7), ar.name,
                            COLOR(8));
        } else if (!strcmp (ar.what, "main")) {
            lua_pushfstring(L, "\t#%d %s%s:%d:%s in the main chunk\n",
                            i, COLOR(7), ar.short_src, ar.currentline,
                            COLOR(8));
        } else if (!strcmp (ar.what, "Lua")) {
            lua_pushfstring(L, "\t#%d %s%s:%d:%s in function '%s%s%s'\n",
                            i, COLOR(7), ar.short_src, ar.currentline,
                            COLOR(8), COLOR(7), ar.name, COLOR(8));
        }
    }

    if (i == 0) {
        lua_pushstring (L, "No activation records.\n");
    }

    lua_concat (L, lua_gettop(L));

    return 1;
}

static int execute ()
{
    int i, h_0, h, status;

    h_0 = lua_gettop(_L);
    status = luap_call (_L, 0);
    h = lua_gettop (_L) - h_0 + 1;

    for (i = h ; i > 0 ; i -= 1) {
        const char *result;

        result = luap_describe (_L, -i);

        if (result) {
            if (h == 1) {
                print_output ("%s%s%s\n", COLOR(3), result, COLOR(0));
            } else {
                print_output ("%s%d%s: %s%s\n", COLOR(4), h - i + 1, COLOR(3), result, COLOR(0));
            }
        }
    }

    lua_settop (_L, h_0 - 1);

    return status;
}

/* This is the pretty-printing related stuff. */

static char *dump;
static int length, offset, indent, column, linewidth, ancestors;

#define dump_literal(s) (check_fit(sizeof(s) - 1), strcpy (dump + offset, s), offset += sizeof(s) - 1, column += width(s))
#define dump_character(c) (check_fit(1), dump[offset] = c, offset += 1, column += 1)

static int width (const char *s)
{
    const char *c;
    int n, discard = 0;

    /* Calculate the printed width of the chunk s ignoring escape
     * sequences. */

    for (c = s, n = 0 ; *c ; c += 1) {
        if (!discard && *c == '\033') {
            discard = 1;
        }

        if (!discard) {
            n+= 1;
        }

        if (discard && *c == 'm') {
            discard = 0;
        }
    }

    return n;
}

static void check_fit (int size)
{
    /* Check if a chunk fits in the buffer and expand as necessary. */

    if (offset + size + 1 > length) {
        length = offset + size + 1;
        dump = (char *)realloc (dump, length * sizeof (char));
    }
}

static int is_identifier (const char *s, int n)
{
    int i;

    /* Check whether a string can be used as a key without quotes and
     * braces. */

    for (i = 0 ; i < n ; i += 1) {
        if (!isalpha(s[i]) &&
            (i == 0 || !isalnum(s[i])) &&
            s[i] != '_') {
            return 0;
        }
    }

    return 1;
}

static void break_line ()
{
    int i;

    check_fit (indent + 1);

    /* Add a line break. */

    dump[offset] = '\n';

    /* And indent to the current level. */

    for (i = 1 ; i <= indent ; i += 1) {
        dump[offset + i] = ' ';
    }

    offset += indent + 1;
    column = indent;
}

static void dump_string (const char *s, int n)
{
    int l;

    /* Break the line if the current chunk doesn't fit but it would
     * fit if we started on a fresh line at the current indent. */

    l = width(s);

    if (column + l > linewidth && indent + l <= linewidth) {
        break_line();
    }

    check_fit (n);

    /* Copy the string to the buffer. */

    memcpy (dump + offset, s, n);
    dump[offset + n] = '\0';

    offset += n;
    column += l;
}

static void describe (lua_State *L, int index)
{
    char *s;
    size_t n;
    int type;

    index = absolute (L, index);
    type = lua_type (L, index);

    if (luaL_getmetafield (L, index, "__tostring")) {
        lua_pushvalue (L, index);
        lua_pcall (L, 1, 1, 0);
        s = (char *)lua_tolstring (L, -1, &n);
        lua_pop (L, 1);

        dump_string (s, n);
    } else if (type == LUA_TNUMBER) {
        /* Copy the value to avoid mutating it. */

        lua_pushvalue (L, index);
        s = (char *)lua_tolstring (L, -1, &n);
        lua_pop (L, 1);

        dump_string (s, n);
    } else if (type == LUA_TSTRING) {
        int i, started, score, level, uselevel = 0;

        s = (char *)lua_tolstring (L, index, &n);

        /* Scan the string to decide how to print it. */

        for (i = 0, score = n, started = 0 ; i < (int)n ; i += 1) {
            if (s[i] == '\n' || s[i] == '\t' ||
                s[i] == '\v' || s[i] == '\r') {
                /* These characters show up better in a long sting so
                 * bias towards that. */

                score += linewidth / 2;
            } else if (s[i] == '\a' || s[i] == '\b' ||
                       s[i] == '\f' || !isprint(s[i])) {
                /* These however go better with an escaped short
                 * string (unless you like the bell or weird
                 * characters). */

                score -= linewidth / 4;
            }

            /* Check what long string delimeter level to use so that
             * the string won't be closed prematurely. */

            if (!started) {
                if (s[i] == ']') {
                    started = 1;
                    level = 0;
                }
            } else {
                if (s[i] == '=') {
                    level += 1;
                } else if (s[i] == ']') {
                    if (level >= uselevel) {
                        uselevel = level + 1;
                    }
                } else {
                    started = 0;
                }
            }
        }

        if (score > linewidth) {
            /* Dump the string as a long string. */

            dump_character ('[');
            for (i = 0 ; i < uselevel ; i += 1) {
                dump_character ('=');
            }
            dump_literal ("[\n");

            dump_string (s, n);

            dump_character (']');
            for (i = 0 ; i < uselevel ; i += 1) {
                dump_character ('=');
            }
            dump_literal ("]");
        } else {
            /* Escape the string as needed and print it as a normal
             * string. */

            dump_literal ("\"");

            for (i = 0 ; i < (int)n ; i += 1) {
                if (s[i] == '"' || s[i] == '\\') {
                    dump_literal ("\\");
                    dump_character (s[i]);
                } else if (s[i] == '\a') {
                    dump_literal ("\\a");
                } else if (s[i] == '\b') {
                    dump_literal ("\\b");
                } else if (s[i] == '\f') {
                    dump_literal ("\\f");
                } else if (s[i] == '\n') {
                    dump_literal ("\\n");
                } else if (s[i] == '\r') {
                    dump_literal ("\\r");
                } else if (s[i] == '\t') {
                    dump_literal ("\\t");
                } else if (s[i] == '\v') {
                    dump_literal ("\\v");
                } else if (isprint(s[i])) {
                    dump_character (s[i]);
                } else {
                    char t[5];
                    size_t n;

                    n = sprintf (t, "\\%03u", ((unsigned char *)s)[i]);
                    dump_string (t, n);
                }
            }

            dump_literal ("\"");
        }
    } else if (type == LUA_TNIL) {
        n = asprintf (&s, "%snil%s", COLOR(7), COLOR(8));
        dump_string (s, n);
        free(s);
    } else if (type == LUA_TBOOLEAN) {
        n = asprintf (&s, "%s%s%s",
                      COLOR(7),
                      lua_toboolean (L, index) ? "true" : "false",
                      COLOR(8));
        dump_string (s, n);
        free(s);
    } else if (type == LUA_TFUNCTION) {
        n = asprintf (&s, "<%sfunction:%s %p>",
                      COLOR(7), COLOR(8), lua_topointer (L, index));
        dump_string (s, n);
        free(s);
    } else if (type == LUA_TUSERDATA) {
        n = asprintf (&s, "<%suserdata:%s %p>",
                      COLOR(7), COLOR(8), lua_topointer (L, index));

        dump_string (s, n);
        free(s);
    } else if (type == LUA_TTHREAD) {
        n = asprintf (&s, "<%sthread:%s %p>",
                      COLOR(7), COLOR(8), lua_topointer (L, index));
        dump_string (s, n);
        free(s);
    } else if (type == LUA_TTABLE) {
        int i, l, n, oldindent, multiline, nobreak;

        /* Check if table is too deeply nested. */

        if (indent > 8 * linewidth / 10) {
            char *s;
            size_t n;

            n = asprintf (&s, "{ %s...%s }", COLOR(7), COLOR(8));
            dump_string (s, n);
            free(s);

            return;
        }

        /* Check if the table introduces a cycle by checking whether
         * it is a back-edge (that is equal to an ancestor table. */

        lua_rawgeti (L, LUA_REGISTRYINDEX, ancestors);
        n = lua_rawlen(L, -1);

        for (i = 0 ; i < n ; i += 1) {
            lua_rawgeti (L, -1, n - i);
#if LUA_VERSION_NUM == 501
            if(lua_equal (L, -1, -3)) {
#else
            if(lua_compare (L, -1, -3, LUA_OPEQ)) {
#endif
                char *s;
                size_t n;

                n = asprintf (&s, "{ %s[%d]...%s }",
                              COLOR(7), -(i + 1), COLOR(8));
                dump_string (s, n);
                free(s);
                lua_pop (L, 2);

                return;
            }

            lua_pop (L, 1);
        }

        /* Add the table to the ancestor list and pop the ancestor
         * list table. */

        lua_pushvalue (L, index);
        lua_rawseti (L, -2, n + 1);
        lua_pop (L, 1);

        /* Open the table and update the indentation level to the
         * current column. */

        dump_literal ("{ ");
        oldindent = indent;
        indent = column;
        multiline = 0;
        nobreak = 0;

        l = lua_rawlen (L, index);

        /* Traverse the array part first. */

        for (i = 0 ; i < l ; i += 1) {
            lua_pushinteger (L, i + 1);
            lua_gettable (L, index);

            /* Start a fresh line when dumping tables to make sure
             * there's plenty of room. */

            if (lua_istable (L, -1)) {
                if (!nobreak) {
                    break_line();
                }

                multiline = 1;
            }

            nobreak = 0;

            /* Dump the value and separating comma. */

            describe (L, -1);
            dump_literal (", ");

            if (lua_istable (L, -1) && i != l - 1) {
                break_line();
                nobreak = 1;
            }

            lua_pop (L, 1);
        }

        /* Now for the hash part. */

        lua_pushnil (L);
        while (lua_next (L, index) != 0) {
            if (!lua_type (L, -2) == LUA_TNUMBER ||
                lua_tonumber (L, -2) != lua_tointeger (L, -2) ||
                lua_tointeger (L, -2) < 1 ||
                lua_tointeger (L, -2) > l) {

                /* Keep each key-value pair on a separate line. */

                break_line ();
                multiline  = 1;

                /* Dump the key and value. */

                if (lua_type (L, -2) == LUA_TSTRING) {
                    char *s;
                    size_t n;

                    s = (char *)lua_tolstring (L, -2, &n);

                    if(is_identifier (s, n)) {
                        dump_string (COLOR(7), strlen(COLOR(7)));
                        dump_string (s, n);
                        dump_string (COLOR(8), strlen(COLOR(8)));
                    } else {
                        dump_literal ("[");
                        describe (L, -2);
                        dump_literal ("]");
                    }
                } else {
                    dump_literal ("[");
                    describe (L, -2);
                    dump_literal ("]");
                }

                dump_literal (" = ");
                describe (L, -1);
                dump_literal (",");
            }

            lua_pop (L, 1);
        }

        /* Remove the table from the ancestor list. */

        lua_rawgeti (L, LUA_REGISTRYINDEX, ancestors);
        lua_pushnil (L);
        lua_rawseti (L, -2, n + 1);
        lua_pop (L, 1);

        /* Pop the indentation level. */

        indent = oldindent;

        if (multiline) {
            break_line();
            dump_literal ("}");
        } else {
            dump_literal (" }");
        }
    }
}

char *luap_describe (lua_State *L, int index)
{
#ifdef HAVE_IOCTL
    struct winsize w;

    /* Initialize the state. */

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) < 0) {
        linewidth = 80;
    } else {
        linewidth = w.ws_col;
    }
#else
    linewidth = 80;
#endif

    index = absolute (L, index);
    offset = 0;
    indent = 0;
    column = 0;

    /* Create a table to hold the ancestors for checking for cycles
     * when printing table hierarchies. */

    lua_newtable (L);
    ancestors = luaL_ref (L, LUA_REGISTRYINDEX);

    describe (L, index);

    luaL_unref (L, LUA_REGISTRYINDEX, ancestors);

    return dump;
}

/* These are custom commands. */

#ifdef HAVE_LIBREADLINE
static int describe_stack (int count, int key)
{
    int i, h;

    print_output ("%s", COLOR(7));

    h = lua_gettop (_L);

    if (count < 0) {
        i = h + count + 1;

        if (i > 0 && i <= h) {
            print_output ("\nValue at stack index %d(%d):\n%s%s",
                          i, -h + i - 1, COLOR(3), luap_describe (_L, i));
        } else {
            print_error ("Invalid stack index.\n");
        }
    } else {
        if (h > 0) {
            print_output ("\nThe stack contains %d values.\n", h);
            for (i = 1 ; i <= h ; i += 1) {
                print_output ("\n%d(%d):\t%s", i, -h + i - 1, lua_typename(_L, lua_type(_L, i)));
            }
        } else {
            print_output ("\nThe stack is empty.");
        }
    }

    print_output ("%s\n", COLOR(0));

    rl_on_new_line ();

    return 0;
}
#endif

int luap_call (lua_State *L, int n) {
    int h, status;

    /* Push the error handler onto the stack. */

    h = lua_gettop(L) - n;
    lua_pushcfunction (L, traceback);
    lua_insert (L, h);

    /* Try to execute the supplied chunk and keep note of any return
     * values. */

    status = lua_pcall(L, n, LUA_MULTRET, h);

    /* Print all returned values with proper formatting. */

    if (status != LUA_OK) {
        print_error ("%s%s%s\n", COLOR(1), lua_tostring (L, -1), COLOR(0));
        lua_pop (L, 1);
    }

    lua_remove (L, h);

    return status;
}

void luap_setprompts(lua_State *L, const char *single, const char *multi)
{
    prompts[0] = (char *)realloc (prompts[0], strlen (single) + 16);
    prompts[1] = (char *)realloc (prompts[1], strlen (multi) + 16);
#ifdef HAVE_LIBREADLINE
    sprintf (prompts[0], "\001%s\002%s\001%s\002",
             COLOR(6), single, COLOR(0));
    sprintf (prompts[1], "\001%s\002%s\001%s\002",
             COLOR(6), multi, COLOR(0));
#else
    sprintf (prompts[0], "%s%s%s", COLOR(6), single, COLOR(0));
    sprintf (prompts[0], "%s%s%s", COLOR(6), multi, COLOR(0));
#endif
}

void luap_sethistory(lua_State *L, const char *file)
{
    logfile = realloc (logfile, strlen(file) + 1);
    strcpy (logfile, file);
}

void luap_setcolor(lua_State *L, int enable)
{
    colorize = enable;
}

void luap_setname(lua_State *L, const char *name)
{
    chunkname = (char *)realloc (chunkname, strlen(name) + 2);
    chunkname[0] = '=';
    strcpy (chunkname + 1, name);
}

void luap_enter(lua_State *L)
{
    int incomplete = 0, s = 0, t = 0, l;
    char *line, *prepended;

    /* Save the state since it needs to be passed to some readline
     * callbacks. */

    _L = L;

    if (!initialized) {
#ifdef HAVE_LIBREADLINE
        rl_basic_word_break_characters = " \t\n`@$><=;|&{(";
        rl_attempted_completion_function = complete;
        rl_completion_display_matches_hook = display_matches;

        rl_add_defun ("lua-describe-stack", describe_stack, META('s'));
#endif

#ifdef HAVE_READLINE_HISTORY
        /* Load the command history if there is one. */

        if (logfile) {
            read_history (logfile);
        }
#endif
        if (!chunkname) {
            luap_setname (L, "lua");
        }

        if (!prompts[0]) {
            luap_setprompts (L, ">  ", ">> ");
        }

        atexit (finish);

        initialized = 1;
    }

    while ((line = readline (incomplete ? prompts[1] : prompts[0]))) {
        int status;

        if (*line == '\0') {
            continue;
        }

#ifdef HAVE_READLINE_HISTORY
        /* Add the line to the history if non-empty. */
            add_history (line);
#endif

        /* Add/copy the line to the buffer. */

        if (incomplete) {
            s += strlen (line) + 1;

            if (s > t) {
                buffer = (char *)realloc (buffer, s + 1);
                t = s;
            }

            strcat (buffer, "\n");
            strcat (buffer, line);
        } else {
            s = strlen (line);

            if (s > t) {
                buffer = (char *)realloc (buffer, s + 1);
                t = s;
            }

            strcpy (buffer, line);
        }

        /* Try to execute the line with a return prepended first.  If
         * this works we can show returned values. */

        l = asprintf (&prepended, "return %s", buffer);

        if (luaL_loadbuffer(_L, prepended, l, chunkname) == LUA_OK) {
            execute();

            incomplete = 0;
        } else {
            lua_pop (_L, 1);

            /* Try to execute the line as-is. */

            status = luaL_loadbuffer(_L, buffer, s, chunkname);

            incomplete = 0;

            if (status == LUA_ERRSYNTAX) {
                const char *message;
                const int k = sizeof(EOF_MARKER) / sizeof(char) - 1;
                size_t n;

                message = lua_tolstring (_L, -1, &n);

                /* If the error message mentions an unexpected eof
                 * then consider this a multi-line statement and wait
                 * for more input.  If not then just print the error
                 * message.*/

                if ((int)n > k &&
                    !strncmp (message + n - k, EOF_MARKER, k)) {
                    incomplete = 1;
                } else {
                    print_error ("%s%s%s\n", COLOR(1), lua_tostring (_L, -1), COLOR(0));
                }

                lua_pop (_L, 1);
            } else if (status == LUA_ERRMEM) {
                print_error ("%s%s%s\n", COLOR(1), lua_tostring (_L, -1), COLOR(0));
                lua_pop (_L, 1);
            } else {
                /* Try to execute the loaded chunk. */

                execute ();
                incomplete = 0;
            }
        }

        free (prepended);
        free (line);
    }

    print_output ("\n");
}
