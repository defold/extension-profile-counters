// Ext.cpp
// Extension lib defines
#define LIB_NAME "Ext"
#define MODULE_NAME "Ext"

// include the Defold SDK
#include <dmsdk/sdk.h>

static int Reverse(lua_State* L)
{
    // The number of expected items to be on the Lua stack
    // once this struct goes out of scope
    DM_LUA_STACK_CHECK(L, 1);

    // Check and get parameter string from stack
    char* str = (char*)luaL_checkstring(L, 1);

    // Reverse the string
    int len = strlen(str);
    for(int i = 0; i < len / 2; i++) {
        const char a = str[i];
        const char b = str[len - i - 1];
        str[i] = b;
        str[len - i - 1] = a;
    }

    // Put the reverse string on the stack
    lua_pushstring(L, str);

    // Return 1 item
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"reverse", Reverse},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeExt(dmExtension::AppParams* params)
{
    dmLogInfo("AppInitializeExt");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeExt(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    dmLogInfo("Registered %s Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeExt(dmExtension::AppParams* params)
{
    dmLogInfo("AppFinalizeExt");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeExt(dmExtension::Params* params)
{
    dmLogInfo("FinalizeExt");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateExt(dmExtension::Params* params)
{
    dmLogInfo("OnUpdateExt");
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ProfileCountExt, LIB_NAME, AppInitializeExt, AppFinalizeExt, InitializeExt, OnUpdateExt, 0, FinalizeExt)
