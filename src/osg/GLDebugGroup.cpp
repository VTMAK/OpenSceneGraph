/******************************************************************************
** Copyright (c) 2019 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

#include <osg/GLDebugGroup>
#include <osg/GLExtensions>

namespace osg
{
   bool GlDebuggingState::theDebuggingEnabled = false;

   bool GlDebuggingState::debuggingEnabled(void)
   {
      return theDebuggingEnabled;
   }
   void GlDebuggingState::setDebuggingEnabled(bool val)
   {
      theDebuggingEnabled = val;
   }

   GlScopedDebugGroup::GlScopedDebugGroup(const osg::GLExtensions* _ext, unsigned int id, const char* message) : ext(_ext)
   {
      if (GlDebuggingState::debuggingEnabled())
      {
         ext->glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, id, -1, message);
      }
   }
   GlScopedDebugGroup::~GlScopedDebugGroup()
   {
      if (GlDebuggingState::debuggingEnabled())
      {
         ext->glPopDebugGroup();
      }
   }

   GlDebugGroup::GlDebugGroup(unsigned int id, const char* message)
      : myId(id), myMessage(message)
   {

   }

   GlScopedDebugGroup::GlScopedDebugGroup()
   {

   }

   GlDebugGroup::GlDebugGroup()
      : myId(0)
   {

   }

   GlDebugGroup::~GlDebugGroup()
   {

   }

   void GlDebugGroup::setId(unsigned int id)
   {
      myId = id;
   }

   unsigned int GlDebugGroup::id(void) const
   {
      return myId;
   }


   void GlDebugGroup::setMessage(const char* message)
   {
      myMessage = message;
   }

   const std::string& GlDebugGroup::message(void) const
   {
      return myMessage;
   }

   void GlDebugGroup::start(const osg::GLExtensions* ext)
   {
      if (GlDebuggingState::debuggingEnabled())
      {
         ext->glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, myId, -1, myMessage.c_str());
      }

   }
   
   void GlDebugGroup::stop(const osg::GLExtensions* ext)
   {
      if (GlDebuggingState::debuggingEnabled())
      {
         ext->glPopDebugGroup();
      }
   }
}
