/* Copyright (C) 2012-2023 Dimitris Papavasiliou, Boris Nagaev
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

#include <string.h>
#include <unistd.h>

#include <lualib.h>
#include <lauxlib.h>

#include "prompt.h"

#if LUA_VERSION_NUM == 501
#define LUA_OK 0
#endif

static int describe (lua_State *L)
{
    lua_pushstring(L, luap_describe(L, 1));
    return 1;
}

static int enter (lua_State *L)
{
    luap_enter(L);
    return 0;
}

static int call (lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_type(L, 1) != LUA_TFUNCTION) {
        lua_settop(L, 0);
        lua_pushboolean(L, 0);
    } else {
        if(luap_call(L, lua_gettop(L) - 1) != LUA_OK) {
            lua_pushboolean(L, 0);
        }
    }

    return lua_gettop(L);
}

static void update_index (lua_State *L)
{
    const char *k;

    k = lua_tostring(L, -1);

    if (!k) {
        k = "";
    }

    if (!strcmp(k, "prompts")) {
        const char *prompts[2];
        int i;

        luap_getprompts(L, &prompts[0], &prompts[1]);
        lua_newtable(L);
        luap_getpromptfuncs(L);

        for (i = 0 ; i < 2 ; i += 1) {
            if (lua_isnil (L, i - 2)) {
                lua_pushstring(L, prompts[i]);
                lua_replace(L, i - 3);
            }
        }

        lua_rawseti(L, -3, 2);
        lua_rawseti(L, -2, 1);
    } else if (!strcmp(k, "colorize")) {
        int colorize;

        luap_getcolor(L, &colorize);
        lua_pushboolean(L, colorize);
    } else if (!strcmp(k, "history")) {
        const char *history;

        luap_gethistory(L, &history);

        if (history) {
            lua_pushstring(L, history);
        } else {
            lua_pushboolean(L, 0);
        }
    } else if (!strcmp(k, "name")) {
        const char *name;

        luap_getname(L, &name);
        lua_pushstring(L, name);
    }

    /* Update the __index table. */

    luaL_getmetafield(L, -3, "__index");
    lua_insert(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

static int prompt_newindex (lua_State *L)
{
    const char *k;

    k = lua_tostring(L, 2);

    if (!k) {
        k = "";
    }

    if (!strcmp(k, "prompts")) {
        const char *prompts[2];
        int i;

        for (i = 0 ; i < 2 ; i += 1) {
            lua_rawgeti(L, 3, i + 1);
            if (lua_isstring(L, -1)) {
                prompts[i] = lua_tostring(L, -1);
            } else {
                prompts[i] = "";
            }

            if (!lua_isfunction(L, -1)) {
                lua_pop(L, 1);
                lua_pushnil(L);
            }
        }

        luap_setprompts(L, prompts[0], prompts[1]);
        luap_setpromptfuncs(L);
    } else if (!strcmp(k, "colorize")) {
        luap_setcolor(L, lua_toboolean(L, 3));
    } else if (!strcmp(k, "history")) {
        luap_sethistory(L, lua_tostring(L, 3));
    } else if (!strcmp(k, "name")) {
        luap_setname(L, lua_tostring(L, 3));
    } else {
        lua_rawset (L, 1);
        return 0;
    }

    /* Update the __index table. */

    lua_pop(L, 1);
    update_index(L);

    return 0;
}

int luaopen_prompt(lua_State* L) {
    static const luaL_Reg functions[] = {
        {"describe", describe},
        {"call", call},
        {"enter", enter},
        {NULL, NULL},
    };

    /* Set default values. */

    luap_setname(L, "lua");
    luap_setprompts (L, ">  ", ">> ");

    /* Create the prompt table. */

#if LUA_VERSION_NUM == 501
    luaL_register(L, "prompt", functions);
#else
    lua_newtable (L);
#endif

    {
        lua_newtable (L);

        /* __index */

        lua_newtable(L);
        lua_setfield (L, -2, "__index");

        /* __newindex */

        lua_pushcfunction (L, prompt_newindex);
        lua_setfield (L, -2, "__newindex");

        lua_setmetatable (L, -2);
    }

    lua_pushliteral(L, "version");
    lua_pushliteral(L, LUAP_VERSION);
    lua_settable(L, -3);

    lua_pushliteral(L, "copyrights");
    lua_createtable(L, 2, 0);
    lua_pushstring(L,
                   "luaprompt " LUAP_VERSION " Copyright (C) "
                   "2012-2023 Dimitris Papavasiliou, Boris Nagaev" );
    lua_rawseti(L, -2, 1);

#if LUA_VERSION_NUM == 501
    lua_pushstring(L, LUA_VERSION " " LUA_COPYRIGHT);
#else
    lua_pushliteral(L, LUA_COPYRIGHT);
#endif
    lua_rawseti(L, -2, 2);
    lua_settable(L, -3);

    lua_pushliteral(L, "interactive");
    lua_pushboolean (L, (isatty (STDIN_FILENO) &&
                         isatty (STDOUT_FILENO)));
    lua_settable(L, -3);

    /* Initialize the __index table. */

    lua_pushliteral(L, "prompts");
    update_index(L);

    lua_pushliteral(L, "colorize");
    update_index(L);

    lua_pushliteral(L, "history");
    update_index(L);

    lua_pushliteral(L, "name");
    update_index(L);

#if LUA_VERSION_NUM != 501
    luaL_setfuncs(L, functions, 0);
#endif

    return 1;
}
