// Copyright 2020-2023 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <dmsdk/sdk.h> // lua.h
#include <dmsdk/dlib/profile.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/mutex.h>
#include <stdio.h>
#include <string.h>

struct Context
{
    dmMutex::HMutex m_Mutex;
    rmtProperty     m_RootProperty;
} g_Context;

RMT_API void rmt_DefoldInitialize()
{
    g_Context.m_Mutex = dmMutex::New();

    rmtProperty* root_property = &g_Context.m_RootProperty;
    root_property->initialised = RMT_TRUE;
    root_property->type = RMT_PropertyType_rmtGroup;
    root_property->value.Bool = RMT_FALSE;
    root_property->flags = RMT_PropertyFlags_NoFlags;
    root_property->name = "Root Property";
    root_property->description = "";
    root_property->defaultValue.Bool = RMT_FALSE;
    root_property->parent = NULL;
    root_property->firstChild = NULL;
    root_property->lastChild = NULL;
    root_property->nextSibling = NULL;
    root_property->nameHash = 0;
    root_property->uniqueID = 0;
}

RMT_API void rmt_DefoldFinalize()
{
    dmMutex::Delete(g_Context.m_Mutex);
}

// Null implementations of the Remotery api
RMT_API void _rmt_BeginCPUSample(rmtPStr name, rmtU32 flags, rmtU32* hash_cache)
{
}

RMT_API void _rmt_EndCPUSample(void)
{
}

// From Remotery.c (modified a bit)
static void RegisterProperty(rmtProperty* property, rmtBool can_lock)
{
    if (property->initialised == RMT_FALSE)
    {
        // Apply for a lock once at the start of the recursive walk
        if (can_lock)
        {
            dmMutex::Lock(g_Context.m_Mutex);
        }

        // Multiple threads accessing the same property can apply for the lock at the same time as the `initialised` property for
        // each of them may not be set yet. One thread only will get the lock successfully while the others will only come through
        // here when the first thread has finished initialising. The first thread through will have `initialised` set to RMT_FALSE
        // while all other threads will see it in its initialised state. Skip those so that we don't register multiple times.
        if (property->initialised == RMT_FALSE)
        {
            // With no parent, add this to the root property
            rmtProperty* parent_property = property->parent;
            if (parent_property == NULL)
            {
                property->parent = &g_Context.m_RootProperty;
                parent_property = property->parent;
            }

            // Walk up to parent properties first in case they haven't been registered
            RegisterProperty(parent_property, RMT_FALSE);

            // Link this property into the parent's list
            if (parent_property->firstChild != NULL)
            {
                parent_property->lastChild->nextSibling = property;
                parent_property->lastChild = property;
            }
            else
            {
                parent_property->firstChild = property;
                parent_property->lastChild = property;
            }

            // Calculate the name hash and send it to the viewer
/// DEFOLD
            const char* name = property->name;
            if (name[0]=='r' && name[1]=='m' && name[2]=='t' && name[3]=='p' && name[4]=='_')
            {
                name = name + 5;
            }

            property->nameHash = dmHashString32(name);

            //QueueAddToStringTable(g_Remotery->mq_to_rmt_thread, property->nameHash, name, name_len, NULL);
/// END DEFOLD

            // // Generate a unique ID for this property in the tree
            // property->uniqueID = parent_property->uniqueID;
            // property->uniqueID = HashCombine(property->uniqueID, property->nameHash);

            property->initialised = RMT_TRUE;
        }

        // Unlock on the way out of recursive walk
        if (can_lock)
        {
            dmMutex::Unlock(g_Context.m_Mutex);
        }
    }
}

// From Remotery.c (modified a bit)
static void PropertyFrameReset(rmtProperty* first_property)
{
    rmtProperty* property;
    for (property = first_property; property != NULL; property = property->nextSibling)
    {
        PropertyFrameReset(property->firstChild);

        // TODO(don): It might actually be quicker to sign-extend assignments but this gives me a nice debug hook for now
        rmtBool changed = RMT_FALSE;
        switch (property->type)
        {
            case RMT_PropertyType_rmtGroup:
                break;

            case RMT_PropertyType_rmtBool:
                changed = property->lastFrameValue.Bool != property->value.Bool;
                break;

            case RMT_PropertyType_rmtS32:
            case RMT_PropertyType_rmtU32:
            case RMT_PropertyType_rmtF32:
                changed = property->lastFrameValue.U32 != property->value.U32;
                break;

            case RMT_PropertyType_rmtS64:
            case RMT_PropertyType_rmtU64:
            case RMT_PropertyType_rmtF64:
                changed = property->lastFrameValue.U64 != property->value.U64;
                break;
        }

        if (changed)
        {
            property->prevValue = property->lastFrameValue;
            //property->prevValueFrame = rmt->propertyFrame;
        }

        property->lastFrameValue = property->value;

        if ((property->flags & RMT_PropertyFlags_FrameReset) != 0)
        {
            property->value = property->defaultValue;
        }
    }
}

// Note, the value is actually added before this call. See Remotery.h:769: _rmt_PropertySet
RMT_API void _rmt_PropertySetValue(rmtProperty* property)
{
    RegisterProperty(property, RMT_TRUE);
}

// Note, the value is actually added before this call. See Remotery.h:776: _rmt_PropertyAdd
RMT_API void _rmt_PropertyAddValue(rmtProperty* property, rmtPropertyValue add_value)
{
    RegisterProperty(property, RMT_TRUE);
}

RMT_API rmtError _rmt_PropertySnapshotAll()
{
    return RMT_ERROR_NONE;
}

RMT_API void _rmt_PropertyFrameResetAll()
{
    DM_MUTEX_SCOPED_LOCK(g_Context.m_Mutex);
    PropertyFrameReset(&g_Context.m_RootProperty);
}

static int rmt_PropertyCollect(lua_State* L, rmtProperty* first_property)
{
    lua_createtable(L, 0, 0); // we don't know how many items beforehand

    rmtProperty* property;
    for (property = first_property; property != NULL; property = property->nextSibling)
    {
        const char* name = property->name;
        if (name[0]=='r' && name[1]=='m' && name[2]=='t' && name[3]=='p' && name[4]=='_')
        {
            name = name + 5;
        }

        switch (property->type)
        {
            case RMT_PropertyType_rmtGroup:
                if (property->firstChild)
                {
                    lua_pushstring(L, name);
                    rmt_PropertyCollect(L, property->firstChild);
                    lua_settable(L, -3);
                }
                continue;
                break;

            case RMT_PropertyType_rmtBool:  lua_pushboolean(L, property->lastFrameValue.Bool); break;
            case RMT_PropertyType_rmtS32:   lua_pushinteger(L, property->lastFrameValue.S32); break;
            case RMT_PropertyType_rmtU32:   lua_pushinteger(L, property->lastFrameValue.U32); break;
            case RMT_PropertyType_rmtS64:   lua_pushinteger(L, property->lastFrameValue.S64); break;
            case RMT_PropertyType_rmtU64:   lua_pushinteger(L, property->lastFrameValue.U64); break;

            case RMT_PropertyType_rmtF32:   lua_pushnumber(L, property->lastFrameValue.F32); break;
            case RMT_PropertyType_rmtF64:   lua_pushnumber(L, property->lastFrameValue.F64); break;


            default:
                continue;
                break;
        }

        lua_setfield(L, -2, name);

        // switch (property->type)
        // {
        //     case RMT_PropertyType_rmtS32:   printf("P: %s: %d\n", name, property->lastFrameValue.S32); break;
        //     case RMT_PropertyType_rmtU32:   printf("P: %s: %u\n", name, property->lastFrameValue.U32); break;
        //     case RMT_PropertyType_rmtS64:   printf("P: %s: %lld\n", name, property->lastFrameValue.S64); break;
        //     case RMT_PropertyType_rmtU64:   printf("P: %s: %llu\n", name, property->lastFrameValue.U64); break;

        //     case RMT_PropertyType_rmtF32:   printf("P: %s: %f\n", name, property->lastFrameValue.F32); break;
        //     case RMT_PropertyType_rmtF64:   printf("P: %s: %f\n", name, property->lastFrameValue.F32); break;
        //     default: break;
        // }
    }
    return 1;
}

RMT_API int rmt_PropertyCollect(lua_State* L)
{
    DM_MUTEX_SCOPED_LOCK(g_Context.m_Mutex);
    return rmt_PropertyCollect(L, g_Context.m_RootProperty.firstChild);
}
