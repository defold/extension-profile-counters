// myextension.cpp
// Extension lib defines
#define LIB_NAME "MyExtension"
#define MODULE_NAME "myextension"

// include the Defold SDK
#include <dmsdk/dlib/profile.h>
#include <dmsdk/sdk.h>
#include <dmsdk/external/remotery/Remotery.h>

DM_PROPERTY_U32(rmtp_MyValue, 0, FrameReset, "a value", 0);
DM_PROPERTY_EXTERN(rmtp_GameObject);
DM_PROPERTY_EXTERN(rmtp_ScriptCount);
DM_PROPERTY_U32(rmtp_CrapCount, 0, FrameReset, "# components", &rmtp_GameObject);

namespace dmProfile 
{
    static Remotery* g_Remotery = 0;
    
    typedef void* HProfile;
    typedef void* HProperty;
       
    void ProfileScope::StartScope(const char* name, uint64_t* name_hash) {}
    void ProfileScope::EndScope() {}   
    bool IsInitialized() { return false; }
    void LogText(const char* text, ...) {}
    void AddCounter(const char* name, uint32_t amount) {}
    void SetThreadName(const char* name) {}
    HProfile BeginFrame() { return 0; }
    void EndFrame(HProfile profile) {}

    // property iterator *****************************
        
    struct PropertyIterator
    {
        // public
        rmtProperty* m_Property;
        PropertyIterator();
        // private
        void* m_IteratorImpl;
        ~PropertyIterator();
    };
    
    PropertyIterator::PropertyIterator()
    : m_Property(0)
    , m_IteratorImpl(0)
    {
    }

    PropertyIterator::~PropertyIterator()
    {
        delete (rmtPropertyIterator*)m_IteratorImpl;
    }

    PropertyIterator* PropertyIterateChildren(rmtProperty* property, PropertyIterator* iter)
    {
        iter->m_Property = 0;

        rmtPropertyIterator* rmt_iter = new rmtPropertyIterator;
        rmt_PropertyIterateChildren(rmt_iter, property);

        iter->m_IteratorImpl = (void*)rmt_iter;
        return iter;
    }

    bool PropertyIterateNext(PropertyIterator* iter)
    {
        rmtPropertyIterator* rmt_iter = (rmtPropertyIterator*)iter->m_IteratorImpl;
        bool result = rmt_PropertyIterateNext(rmt_iter);
        iter->m_Property = rmt_iter->property;
        return result;
    }   

    // from profiler.cpp
    
    static void ProcessProperty(int indent, rmtProperty* property)
    {
        //const char* name = dmProfile::PropertyGetName(property);
        //uint32_t name_hash = dmHashString32(name?name:"<empty_property_name>");

        //dmProfile::PropertyType type = dmProfile::PropertyGetType(property);
        //dmProfile::PropertyValue value = dmProfile::PropertyGetValue(property);

        // do something here
        printf("processing property...(%i): ", indent);
        for (int steps_up = indent; steps_up >= 0; steps_up--)
        {
            rmtProperty* theproperty;
            theproperty = property;
            for (int i=0; i < steps_up; i++)
            {
                theproperty = property->parent;
            }
            printf("%s", rmt_PropertyGetName(theproperty) );
            if (steps_up > 0)
                printf(".");
        }
        printf("\n");
        
        //printf("property name: %s\n", rmt_PropertyGetName( (rmtProperty*) property));
    }

    static void TraversePropertyTree(int indent, rmtProperty* property)
    {
        ProcessProperty(indent, property);

        dmProfile::PropertyIterator iter;
        dmProfile::PropertyIterateChildren(property, &iter);
        while (dmProfile::PropertyIterateNext(&iter))
        {
            TraversePropertyTree(indent + 1, iter.m_Property);
        }
    } 

    static void PropertyTreeCallback(void* _ctx, rmtProperty* root)
    {
        //if (g_ProfilerCurrentFrame == 0) // Possibly in the process of shutting down
        //return;

        // DM_MUTEX_SCOPED_LOCK(g_ProfilerMutex);        

        dmProfile::PropertyIterator iter;
        dmProfile::PropertyIterateChildren(root, &iter);
        while (dmProfile::PropertyIterateNext(&iter))
        {
            TraversePropertyTree(0, iter.m_Property);
        }
    }

     
}


static int GetCounters(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    Remotery* remotery = _rmt_GetGlobalInstance();
    printf("g_remotery: %p\n", remotery);

    rmt_PropertySnapshotAll();
     
    return 0;
}


// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"getcounters", GetCounters},
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

dmExtension::Result AppInitializeMyExtension(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeMyExtension(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    printf("Registered %s Extension\n", MODULE_NAME);

    
    rmtSettings* settings = rmt_Settings();    
    settings->snapshot_callback = dmProfile::PropertyTreeCallback;
    settings->snapshot_context = 0;
    
    rmt_CreateGlobalInstance(&dmProfile::g_Remotery);
    
    
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeMyExtension(dmExtension::AppParams* params)
{
    if ( dmProfile::g_Remotery)
    {
        rmt_DestroyGlobalInstance(dmProfile::g_Remotery);
        dmProfile::g_Remotery = 0;   
    }
    
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeMyExtension(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}


// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// MyExtension is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(MyExtension, LIB_NAME, AppInitializeMyExtension, AppFinalizeMyExtension, InitializeMyExtension, 0, 0, FinalizeMyExtension)

