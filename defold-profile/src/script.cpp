// include the Defold SDK
#include <dmsdk/sdk.h>
#include <dmsdk/dlib/profile.h>

#include "profiler.h"

#define MODULE_NAME "profile"

#if defined(TEST_PROPERTY)
DM_PROPERTY_GROUP(rmtp_TestExt, "TestCounters", 0);
DM_PROPERTY_U32(rmtp_TestExtRandom, 0, PROFILE_PROPERTY_FRAME_RESET, "random number", &rmtp_TestExt);
DM_PROPERTY_U32(rmtp_TestExtFrames, 0, 0, "frame count", &rmtp_TestExt);
#endif

// static void DebugPrintProperty(HProperty property)
// {
//     const char* name            = PropertyGetName(property);
//     ProfilePropertyType type    = PropertyGetType(property);
//     ProfilePropertyValue value  = PropertyGetPrevValue(property);

//     // Debug prints
//     switch (type)
//     {
//         case PROFILE_PROPERTY_TYPE_GROUP:  printf("P: %s: //group\n", name); break;
//         case PROFILE_PROPERTY_TYPE_BOOL:   printf("P: %s: %d\n", name, value.m_Bool); break;
//         case PROFILE_PROPERTY_TYPE_S32:    printf("P: %s: %d\n", name, value.m_S32); break;
//         case PROFILE_PROPERTY_TYPE_U32:    printf("P: %s: %u\n", name, value.m_U32); break;
//         case PROFILE_PROPERTY_TYPE_S64:    printf("P: %s: %lld\n", name, value.m_S64); break;
//         case PROFILE_PROPERTY_TYPE_U64:    printf("P: %s: %llu\n", name, value.m_U64); break;
//         case PROFILE_PROPERTY_TYPE_F32:    printf("P: %s: %f\n", name, value.m_F32); break;
//         case PROFILE_PROPERTY_TYPE_F64:    printf("P: %s: %f\n", name, value.m_F32); break;
//         default: break;
//     }
// }

static int PushProperty(lua_State* L, bool all_properties, HProperty property)
{
    const char* name            = PropertyGetName(property);
    ProfilePropertyType type    = PropertyGetType(property);
    ProfilePropertyValue value  = PropertyGetPrevValue(property);

    //DebugPrintProperty(property);

    lua_pushstring(L, name);
    lua_setfield(L, -2, "name");

    switch (type)
    {
        case PROFILE_PROPERTY_TYPE_GROUP:   lua_pushnil(L); break;
        case PROFILE_PROPERTY_TYPE_BOOL:    lua_pushboolean(L, value.m_Bool); break;
        case PROFILE_PROPERTY_TYPE_S32:     lua_pushinteger(L, value.m_S32); break;
        case PROFILE_PROPERTY_TYPE_U32:     lua_pushinteger(L, value.m_U32); break;
        case PROFILE_PROPERTY_TYPE_S64:     lua_pushinteger(L, value.m_S64); break;
        case PROFILE_PROPERTY_TYPE_U64:     lua_pushinteger(L, value.m_U64); break;
        case PROFILE_PROPERTY_TYPE_F32:     lua_pushnumber(L, value.m_F32); break;
        case PROFILE_PROPERTY_TYPE_F64:     lua_pushnumber(L, value.m_F64); break;
        default:                            lua_pushstring(L, "unknown type"); break;
    }
    lua_setfield(L, -2, "value");

    switch (type)
    {
        case PROFILE_PROPERTY_TYPE_GROUP:   lua_pushstring(L, "Group"); break;
        case PROFILE_PROPERTY_TYPE_BOOL:    lua_pushstring(L, "Bool"); break;
        case PROFILE_PROPERTY_TYPE_S32:     lua_pushstring(L, "S32"); break;
        case PROFILE_PROPERTY_TYPE_U32:     lua_pushstring(L, "U32"); break;
        case PROFILE_PROPERTY_TYPE_S64:     lua_pushstring(L, "S64"); break;
        case PROFILE_PROPERTY_TYPE_U64:     lua_pushstring(L, "U64"); break;
        case PROFILE_PROPERTY_TYPE_F32:     lua_pushstring(L, "F32"); break;
        case PROFILE_PROPERTY_TYPE_F64:     lua_pushstring(L, "F64"); break;
        default:                            lua_pushstring(L, "unknown"); break;
    }
    lua_setfield(L, -2, "type");

    // lua_createtable(L, 0, 0);

    //     lua_pushstring(L, "child_value");
    //     lua_setfield(L, -2, "child_prop");

    // lua_setfield(L, -2, "_children");

    //lua_pushstring(L, "children");
    //lua_settable(L, -3);

    // lua_seti(L, -3, c);



    lua_createtable(L, 0, 0); // we don't know how many items beforehand

    // Each property type may have children
    PropertyIterator iter;
    PropertyIterateChildren(property, all_properties, &iter);
    uint32_t num_children = 0;
    while (PropertyIterateNext(&iter))
    {
        lua_createtable(L, 0, 0);
            PushProperty(L, all_properties, iter.m_Property);
            lua_rawseti(L, -2, ++num_children);
    }

    if (!num_children)
    {
        lua_pop(L, 1); // pop the "children" table
    }
    else
    {
        lua_setfield(L, -2, "children");
    }

    return 1;
}

static int GetProfileProperties(lua_State* L)
{
    bool all_properties = true;//lua_toboolean(L, 1);
    HProperty root = PropertyGetRoot();
    lua_createtable(L, 0, 0); // we don't know how many items beforehand
    PushProperty(L, all_properties, root);
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"get_properties", GetProfileProperties},
    {0, 0}
};

void ScriptInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}
