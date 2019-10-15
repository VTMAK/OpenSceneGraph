/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield 
 *
 * This library is open source and may be redistributed and/or modified under  
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or 
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * OpenSceneGraph Public License for more details.
*/

//
// OpenFlight® loader for OpenSceneGraph
//
//  Copyright (C) 2005-2007  Brede Johansen
//

#include <osg/Notify>
#include "Registry.h"

// VRV_PATCH BEGIN
#ifdef WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif
// VRV_PATCH END

using namespace flt;

Registry::Registry()
{
}

Registry::~Registry()
{
}

static Registry* protoTypeRegistry;

Registry* flt::Registry::protoTypeInstance()
{
   if(!protoTypeRegistry)
   {
      protoTypeRegistry = new Registry;
   }
   return protoTypeRegistry;
}

flt::Registry::Registry( const Registry& other ) :_recordProtoMap(other._recordProtoMap)
,_externalReadQueue(other._externalReadQueue)
,_externalCacheMap(other._externalCacheMap)
,_textureCacheMap(other._textureCacheMap)
{
}

class FixedReadWriteMutex : public OpenThreads::ReadWriteMutex
{
public:

   virtual int upgradeLock()
   {
      OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_readCountMutex);
      
      return _readWriteMutex.lock();
   }

};

Registry* Registry::instance(bool initalizeOnly)
{
   // VRV_PATCH BEGIN
   typedef std::map<unsigned int, Registry*> RegisteryInstanceMap;
   static OpenThreads::Mutex _theTLSMutex;
   static RegisteryInstanceMap _theTLSMap;

   // Little heavier lock rather than read write
   OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_theTLSMutex);

   if(initalizeOnly) return 0;

   unsigned int currentThreadId = 0;
#ifdef WIN32
   // This can return NULL if not an OpenThreads thread so use platform way
   //OpenThreads::Thread* currentThread = OpenThreads::Thread::CurrentThread();
   currentThreadId = GetCurrentThreadId();
#else
   currentThreadId = pthread_self();
#endif // WINDOWS

   {
      RegisteryInstanceMap::iterator i = _theTLSMap.find(currentThreadId);
      if(i != _theTLSMap.end()) 
      {
         return i->second;
      }
   }

   Registry* newReg = new Registry(*protoTypeRegistry);
   _theTLSMap[currentThreadId] = newReg;
   // VRV_PATCH END
   return newReg;   
}

void Registry::addPrototype(int opcode, Record* prototype)
{
    if (prototype==0L)
    {
        OSG_WARN << "Not a record." << std::endl;
        return;
    }

    if (_recordProtoMap.find(opcode) != _recordProtoMap.end())
        OSG_WARN << "Registry already contains prototype for opcode " << opcode << "." << std::endl;

    _recordProtoMap[opcode] = prototype;
}

Record* Registry::getPrototype(int opcode)
{
    RecordProtoMap::iterator itr = _recordProtoMap.find(opcode);
    if (itr != _recordProtoMap.end())
        return (*itr).second.get();

    return NULL;
}
