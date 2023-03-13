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

#include <dmsdk/dlib/profile.h>
#include <stdio.h>

namespace dmProfile
{
    bool IsInitialized()
    {
        return false;
    }

    void SetThreadName(const char* name)
    {
    }

    HProfile BeginFrame()
    {
        return (HProfile)1; // Return a "valid" handle to enable the profiling
    }

    void EndFrame(HProfile profile)
    {
        rmt_PropertyFrameResetAll();
    }

    uint64_t GetTicksPerSecond()
    {
        return 1000000;
    }

    void AddCounter(const char* name, uint32_t amount)
    {

    }

    void LogText(const char* text, ...)
    {
    }

    void ProfileScope::StartScope(const char* name, uint64_t* name_hash)
    {
    }

    void ProfileScope::EndScope()
    {
    }

    void ScopeBegin(const char* name, uint64_t* name_hash)
    {
    }

    void ScopeEnd()
    {
    }

} // namespace dmProfile

