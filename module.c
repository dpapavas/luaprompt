#include <string.h>

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
        const char *single, *multi;

        luap_getprompts(L, &single, &multi);
        lua_newtable(L);
        lua_pushstring(L, single);
        lua_rawseti(L, -2, 1);
        lua_pushstring(L, multi);
        lua_rawseti(L, -2, 2);
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
        const char *single, *multi;

        lua_rawgeti(L, 3, 1);
        single = lua_tostring(L, -1);
        lua_rawgeti(L, 3, 2);
        multi = lua_tostring(L, -1);
        lua_pop(L, 2);

        luap_setprompts(L, single, multi);
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

    luap_setname(L, "lua");
    luap_setprompts (L, ">  ", ">> ");

    /* Create the prompt table. */

    lua_newtable (L);

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

    /* Initialize the __index table. */

    lua_pushliteral(L, "prompts");
    update_index(L);

    lua_pushliteral(L, "colorize");
    update_index(L);

    lua_pushliteral(L, "history");
    update_index(L);

    lua_pushliteral(L, "name");
    update_index(L);

#if LUA_VERSION_NUM == 501
    luaL_register(L, NULL, functions);
#else
    luaL_setfuncs(L, functions, 0);
#endif

    return 1;
}
