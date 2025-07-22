#include <dmsdk/dlib/atomic.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/extension/extension.h>

#include <stdio.h>
#include <stdlib.h> // rand
#include <string.h> // memset

#include "profiler.h"
#include "script.h"

// NOTE: This is mostly copied from profiler_basic.cpp in the Defold repo.

// ****************************************************************************

struct Property
{
    const char*             m_Name;
    const char*             m_Description;
    uint32_t                m_NameHash;
    uint32_t                m_Flags;
    ProfileIdx              m_Parent;
    ProfileIdx              m_Sibling;
    ProfileIdx              m_FirstChild;
    ProfilePropertyValue    m_DefaultValue;
    ProfilePropertyType     m_Type;
};

struct PropertyData
{
    ProfilePropertyValue    m_Value;
    ProfilePropertyValue    m_PrevValue; // This is the one that we collect from the script function
    uint8_t                 m_Used : 1;
    uint8_t                 m_PrevUsed : 1;
};

static dmMutex::HMutex  g_Lock = 0;
static int32_atomic_t   g_ProfileInitialized = 0;
static int32_atomic_t   g_PropertyInitialized = 0;
static const uint32_t   g_MaxPropertyCount = 256;
static Property         g_Properties[g_MaxPropertyCount];
static PropertyData     g_PropertyData[g_MaxPropertyCount];


static bool IsProfileInitialized()
{
    return dmAtomicGet32(&g_ProfileInitialized) != 0;
}

#define CHECK_INITIALIZED() \
    if (!IsProfileInitialized()) \
        return;

// ****************************************************************************
// Properties

static inline bool IsValidIndex(ProfileIdx idx)
{
    return idx != PROFILE_PROPERTY_INVALID_IDX;
}

static Property* AllocateProperty(ProfileIdx idx)
{
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    if (idx >= g_MaxPropertyCount)
        return 0;
    return &g_Properties[idx];
}

static Property* GetPropertyFromIdx(ProfileIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    DM_MUTEX_SCOPED_LOCK(g_Lock);
    return &g_Properties[idx];
}

static PropertyData* GetPropertyDataFromIdx(ProfileIdx idx)
{
    if (!IsValidIndex(idx))
        return 0;
    return &g_PropertyData[idx];
}


#define ALLOC_PROP_AND_CHECK(IDX)                       \
    PropertyInitialize();                               \
    Property* prop = AllocateProperty(IDX);             \
    if (!prop)                                          \
        return;

#define GET_PROPDATA_AND_CHECK(IDX)                     \
    if (!IsProfileInitialized())                        \
        return;                                         \
    DM_MUTEX_SCOPED_LOCK(g_Lock);                       \
    PropertyData* data = GetPropertyDataFromIdx(IDX);   \
    if (!data)                                          \
        return;

// Invoked when first property is initialized
static void PropertyInitialize()
{
    if (dmAtomicIncrement32(&g_PropertyInitialized) != 0)
        return;

    memset(g_Properties, 0, sizeof(g_Properties));
    memset(g_PropertyData, 0, sizeof(g_PropertyData));

    g_Lock = dmMutex::New();

    g_Properties[0].m_Name = "Root";
    g_Properties[0].m_NameHash = dmHashString32(g_Properties[0].m_Name);
    g_Properties[0].m_Type = PROFILE_PROPERTY_TYPE_GROUP;
    g_Properties[0].m_Parent = PROFILE_PROPERTY_INVALID_IDX;
    g_Properties[0].m_FirstChild = PROFILE_PROPERTY_INVALID_IDX;
    g_Properties[0].m_Sibling = PROFILE_PROPERTY_INVALID_IDX;
    g_PropertyData[0].m_Used = 1; // used == 0, means we won't traverse it during display
}

static void ResetProperties()
{
    for (uint32_t i = 0; i < g_MaxPropertyCount; ++i)
    {
        Property* prop = &g_Properties[i];
        PropertyData* data = &g_PropertyData[i];

        // For easy access in the script, as the script function may be run before the properties we want to get
        // and they may be currently 0 (i.e. reset on each frame end)
        data->m_PrevValue = data->m_Value;
        data->m_PrevUsed = data->m_Used;

        data->m_Used = prop->m_Type == PROFILE_PROPERTY_TYPE_GROUP ? 1 : 0;

        if ((prop->m_Flags & PROFILE_PROPERTY_FRAME_RESET) == PROFILE_PROPERTY_FRAME_RESET)
        {
            data->m_Value = prop->m_DefaultValue;
        }
    }
}

static void SetupProperty(Property* prop, ProfileIdx idx, const char* name, const char* desc, uint32_t flags, ProfileIdx parentidx)
{
    if (name[0]=='r' && name[1]=='m' && name[2]=='t' && name[3]=='p' && name[4]=='_')
    {
        name = name + 5;
    }

    memset(prop, 0, sizeof(Property));
    prop->m_Flags        = flags;
    prop->m_Name         = name;
    prop->m_NameHash     = dmHashString32(name);
    prop->m_Description  = desc;
    prop->m_FirstChild   = PROFILE_PROPERTY_INVALID_IDX;
    prop->m_Sibling      = PROFILE_PROPERTY_INVALID_IDX;
    prop->m_Parent       = parentidx;

    // TODO: append them last so that they'll render in the same order they were registered
    Property* parent = GetPropertyFromIdx(prop->m_Parent);
    if (parent)
    {
        prop->m_Sibling = parent->m_FirstChild;
        parent->m_FirstChild = idx;
    }
    if (prop->m_Parent == 0)
    {
        assert(idx != 0);
    }
}

static void ProfileCreatePropertyGroup(void*, const char* name, const char* desc, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(prop, idx, name, desc, 0, parent);
    prop->m_Type = PROFILE_PROPERTY_TYPE_GROUP;
}

static void ProfileCreatePropertyBool(void*, const char* name, const char* desc, int value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_Bool = (bool)value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_BOOL;
}

static void ProfileCreatePropertyS32(void*, const char* name, const char* desc, int32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_S32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_S32;
}

static void ProfileCreatePropertyU32(void*, const char* name, const char* desc, uint32_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_U32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_U32;
}

static void ProfileCreatePropertyF32(void*, const char* name, const char* desc, float value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_F32 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_F32;
}

static void ProfileCreatePropertyS64(void*, const char* name, const char* desc, int64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_S64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_S64;
}

static void ProfileCreatePropertyU64(void*, const char* name, const char* desc, uint64_t value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_U64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_U64;
}

static void ProfileCreatePropertyF64(void*, const char* name, const char* desc, double value, uint32_t flags, ProfileIdx idx, ProfileIdx parent)
{
    ALLOC_PROP_AND_CHECK(idx);
    SetupProperty(prop, idx, name, desc, flags, parent);
    prop->m_DefaultValue.m_F64 = value;
    prop->m_Type = PROFILE_PROPERTY_TYPE_F64;
}

static void ProfilePropertySetBool(void*, ProfileIdx idx, int v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_Bool = v;
    data->m_Used = 1;
}

static void ProfilePropertySetS32(void*, ProfileIdx idx, int32_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_S32 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetU32(void*, ProfileIdx idx, uint32_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_U32 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetF32(void*, ProfileIdx idx, float v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_F32 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetS64(void*, ProfileIdx idx, int64_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_S64 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetU64(void*, ProfileIdx idx, uint64_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_U64 = v;
    data->m_Used = 1;
}

static void ProfilePropertySetF64(void*, ProfileIdx idx, double v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_F64 = v;
    data->m_Used = 1;
}

static void ProfilePropertyAddS32(void*, ProfileIdx idx, int32_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_S32 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddU32(void*, ProfileIdx idx, uint32_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_U32 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddF32(void*, ProfileIdx idx, float v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_F32 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddS64(void*, ProfileIdx idx, int64_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_S64 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddU64(void*, ProfileIdx idx, uint64_t v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_U64 += v;
    data->m_Used = 1;
}

static void ProfilePropertyAddF64(void*, ProfileIdx idx, double v)
{
    GET_PROPDATA_AND_CHECK(idx);
    data->m_Value.m_F64 += v;
    data->m_Used = 1;
}

static void ProfilePropertyReset(void*, ProfileIdx idx)
{
    GET_PROPDATA_AND_CHECK(idx);
    Property* prop = GetPropertyFromIdx(idx);
    data->m_Value = prop->m_DefaultValue;
    data->m_Used = 0;
}

// Iterators


PropertyIterator::PropertyIterator()
    : m_Property(0)
    , m_IteratorImpl(0)
    , m_AllProperties(false)
{
}

PropertyIterator::~PropertyIterator()
{
}

#define CHECK_HPROPERTY(HPROP)                          \
    ProfileIdx      idx  = (ProfileIdx)HPROP;          \
    PropertyData*   data = GetPropertyDataFromIdx(idx); \
    Property*       prop = GetPropertyFromIdx(idx);     \
    (void)data; (void)prop;

PropertyIterator* PropertyIterateChildren(HProperty hproperty, bool all_properties, PropertyIterator* iter)
{
    CHECK_HPROPERTY(hproperty);
    iter->m_AllProperties = all_properties;
    iter->m_Property = 0;
    iter->m_IteratorImpl = (void*)(uintptr_t)prop->m_FirstChild;
    return iter;
}

bool PropertyIterateNext(PropertyIterator* iter)
{
    ProfileIdx idx = (ProfileIdx)(uintptr_t)iter->m_IteratorImpl;
    if (!IsValidIndex(idx))
        return false;

    PropertyData* data = GetPropertyDataFromIdx(idx);

    if (!iter->m_AllProperties)
    {
        // let's skip the non used ones
        while (!data->m_PrevUsed)
        {
            Property* prop = GetPropertyFromIdx(idx);
            idx = prop->m_Sibling;
            if (!IsValidIndex(idx))
            {
                return false;
            }
            data = GetPropertyDataFromIdx(idx);
        }
    }

    Property* prop = GetPropertyFromIdx(idx);
    iter->m_Property = (HProperty)idx;
    iter->m_IteratorImpl = (void*)(uintptr_t)prop->m_Sibling;
    return true;
}

// Property accessors

uint32_t PropertyGetNameHash(HProperty hproperty)
{
    CHECK_HPROPERTY(hproperty)
    return prop->m_NameHash;
}

const char* PropertyGetName(HProperty hproperty)
{
    CHECK_HPROPERTY(hproperty)
    return prop->m_Name;
}

const char* PropertyGetDesc(HProperty hproperty)
{
    CHECK_HPROPERTY(hproperty)
    return prop->m_Description;
}

ProfilePropertyType PropertyGetType(HProperty hproperty)
{
    CHECK_HPROPERTY(hproperty)
    return prop->m_Type;
}

ProfilePropertyValue PropertyGetValue(HProperty hproperty)
{
    CHECK_HPROPERTY(hproperty)
    return data->m_Value;
}

ProfilePropertyValue PropertyGetPrevValue(HProperty hproperty)
{
    CHECK_HPROPERTY(hproperty)
    return data->m_PrevValue;
}

HProperty PropertyGetRoot()
{
    if (!IsProfileInitialized())
        return PROFILE_PROPERTY_INVALID_IDX;
    return 0; // root index
}

// ****************************************************************************
// Frames

static void FrameBegin(void* ctx)
{
}

static void FrameEnd(void* ctx)
{
    (void)ctx;
    CHECK_INITIALIZED();
    DM_MUTEX_SCOPED_LOCK(g_Lock);

    ResetProperties();
}


// ****************************************************************************
// Profiler

static void* CreateListener()
{
    assert(!IsProfileInitialized());

    dmAtomicIncrement32(&g_ProfileInitialized);

    return (void*)(uintptr_t)1;
}

static void DestroyListener(void* listener)
{
    CHECK_INITIALIZED();
    dmAtomicDecrement32(&g_ProfileInitialized);
    dmMutex::Delete(g_Lock);
    g_Lock = 0;
}


// ****************************************************************************
// Extension

static const char* g_ProfilerName = "ProfilerCounter";
static ProfileListener g_Listener = {};

static dmExtension::Result AppInitialize(dmExtension::AppParams* params)
{
    g_Listener.m_Create         = CreateListener;
    g_Listener.m_Destroy        = DestroyListener;
    g_Listener.m_SetThreadName  = 0;

    g_Listener.m_FrameBegin     = FrameBegin;
    g_Listener.m_FrameEnd       = FrameEnd;
    g_Listener.m_ScopeBegin     = 0;            // static void ScopeBegin(void* ctx, const char* name, uint64_t name_hash);
    g_Listener.m_ScopeEnd       = 0;            // static void ScopeEnd(void* ctx, const char* name, uint64_t name_hash);

    g_Listener.m_LogText        = 0;            // static void  LogText(void* listener, const char* text);

    g_Listener.m_CreatePropertyGroup = ProfileCreatePropertyGroup;
    g_Listener.m_CreatePropertyBool = ProfileCreatePropertyBool;
    g_Listener.m_CreatePropertyS32 = ProfileCreatePropertyS32;
    g_Listener.m_CreatePropertyU32 = ProfileCreatePropertyU32;
    g_Listener.m_CreatePropertyF32 = ProfileCreatePropertyF32;
    g_Listener.m_CreatePropertyS64 = ProfileCreatePropertyS64;
    g_Listener.m_CreatePropertyU64 = ProfileCreatePropertyU64;
    g_Listener.m_CreatePropertyF64 = ProfileCreatePropertyF64;

    g_Listener.m_PropertySetBool = ProfilePropertySetBool;
    g_Listener.m_PropertySetS32 = ProfilePropertySetS32;
    g_Listener.m_PropertySetU32 = ProfilePropertySetU32;
    g_Listener.m_PropertySetF32 = ProfilePropertySetF32;
    g_Listener.m_PropertySetS64 = ProfilePropertySetS64;
    g_Listener.m_PropertySetU64 = ProfilePropertySetU64;
    g_Listener.m_PropertySetF64 = ProfilePropertySetF64;
    g_Listener.m_PropertyAddS32 = ProfilePropertyAddS32;
    g_Listener.m_PropertyAddU32 = ProfilePropertyAddU32;
    g_Listener.m_PropertyAddF32 = ProfilePropertyAddF32;
    g_Listener.m_PropertyAddS64 = ProfilePropertyAddS64;
    g_Listener.m_PropertyAddU64 = ProfilePropertyAddU64;
    g_Listener.m_PropertyAddF64 = ProfilePropertyAddF64;
    g_Listener.m_PropertyReset = ProfilePropertyReset;

    ProfileRegisterProfiler(g_ProfilerName, &g_Listener);
    dmLogInfo("Registered profiler %s", g_ProfilerName);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Initialize(dmExtension::Params* params)
{
    // Init Lua
    ScriptInit(params->m_L);
    return dmExtension::RESULT_OK;
}

#if defined(TEST_PROPERTY)

// Allows access from a different file
DM_PROPERTY_EXTERN(rmtp_TestExtRandom);
DM_PROPERTY_EXTERN(rmtp_TestExtFrames);

static dmExtension::Result Update(dmExtension::Params* params)
{
    DM_PROPERTY_SET_U32(rmtp_TestExtRandom, rand());
    DM_PROPERTY_ADD_U32(rmtp_TestExtFrames, 1);

    return dmExtension::RESULT_OK;
}

#else
    #define Update 0
#endif

DM_DECLARE_EXTENSION(ProfilerCounter, g_ProfilerName, AppInitialize, AppFinalize, Initialize, Update, 0, 0);
