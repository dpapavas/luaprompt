/* Copyright (C) 2015 Boris Nagaev
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

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "prompt.h"

int lua_luap_setprompts(lua_State* L) {
    const char* single = luaL_checkstring(L, 1);
    const char* multi = luaL_checkstring(L, 2);
    luap_setprompts(L, single, multi);
    return 0;
}

int lua_luap_sethistory(lua_State* L) {
    const char* file = luaL_checkstring(L, 1);
    luap_sethistory(L, file);
    return 0;
}

int lua_luap_setname(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    luap_setname(L, name);
    return 0;
}

int lua_luap_setcolor(lua_State* L) {
    int enable = lua_toboolean(L, 1);
    luap_setcolor(L, enable);
    return 0;
}

int lua_luap_enter(lua_State* L) {
    luap_enter(L);
    return 0;
}

int lua_luap_describe(lua_State* L) {
    const char* description = luap_describe(L, 1);
    lua_pushstring(L, description);
    return 1;
}

int lua_luap_version(lua_State* L) {
    lua_pushliteral(L, LUAP_VERSION);
    return 1;
}

static const luaL_Reg luap_functions[] = {
    {"setprompts", lua_luap_setprompts},
    {"sethistory", lua_luap_sethistory},
    {"setname", lua_luap_setname},
    {"setcolor", lua_luap_setcolor},
    {"enter", lua_luap_enter},
    {"describe", lua_luap_describe},
    {"version", lua_luap_version},
    {NULL, NULL},
};

#if LUA_VERSION_NUM == 501
#define compat_setfuncs(L, funcs) luaL_register(L, NULL, funcs)
#else
#define compat_setfuncs(L, funcs) luaL_setfuncs(L, funcs, 0)
#endif

int luaopen_prompt(lua_State* L) {
    lua_newtable(L);
    compat_setfuncs(L, luap_functions);
    return 1;
}
