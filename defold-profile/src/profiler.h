#ifndef DM_PROFILER_H
#define DM_PROFILER_H

#include <stdint.h>
#include <dmsdk/dlib/profile.h>

typedef ProfileIdx  HProperty;

// Property Iterator

struct PropertyIterator
{
// public
    HProperty m_Property;
    PropertyIterator();
// private
    void* m_IteratorImpl;
    bool m_AllProperties;
    ~PropertyIterator();
};

 // all_properties==false: only those properteis that were changed during the frame.
PropertyIterator*       PropertyIterateChildren(HProperty property, bool all_properties, PropertyIterator* iter);
bool                    PropertyIterateNext(PropertyIterator* iter);

// Property accessors

HProperty               PropertyGetRoot();
uint32_t                PropertyGetNameHash(HProperty property);
const char*             PropertyGetName(HProperty property);
const char*             PropertyGetDesc(HProperty property);
ProfilePropertyType     PropertyGetType(HProperty property);
ProfilePropertyValue    PropertyGetValue(HProperty property);
ProfilePropertyValue    PropertyGetPrevValue(HProperty property);



#endif // DM_PROFILER_H
