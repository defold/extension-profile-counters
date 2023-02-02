// include the Defold SDK
#include <dmsdk/sdk.h>
#include <dmsdk/dlib/profile.h>

#define LIB_NAME "ProfileCounters"
#define MODULE_NAME "profile"

//#define TEST_PROPERTY
#if defined(TEST_PROPERTY)
DM_PROPERTY_GROUP(rmtp_TestExt, "TestCounters");
DM_PROPERTY_U32(rmtp_TestExtRandom, 0, FrameReset, "random number", &rmtp_TestExt);
DM_PROPERTY_U32(rmtp_TestExtFrames, 0, NoFlags, "frame count", &rmtp_TestExt);
#endif

void rmt_DefoldInitialize();
void rmt_DefoldFinalize();
int rmt_PropertyCollect(lua_State* L);

static int GetProfileProperties(lua_State* L)
{
    return rmt_PropertyCollect(L);
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"get_properties", GetProfileProperties},
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
    rmt_DefoldInitialize();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeExt(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeExt(dmExtension::AppParams* params)
{
    rmt_DefoldFinalize();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeExt(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateExt(dmExtension::Params* params)
{
#if defined(TEST_PROPERTY)
    DM_PROPERTY_SET_U32(rmtp_TestExtRandom, rand());
    DM_PROPERTY_ADD_U32(rmtp_TestExtFrames, 1);
#endif

    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ProfileCountExt, LIB_NAME, AppInitializeExt, AppFinalizeExt, InitializeExt, OnUpdateExt, 0, FinalizeExt)
