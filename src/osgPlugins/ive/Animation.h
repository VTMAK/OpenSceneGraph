/******************************************************************************
** Copyright (c) 2019 VT-MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/
#ifndef IVE_ANIMATION
#define IVE_IVE_ANIMATION 1

#include <osgAnimation/Animation>
#include <osgAnimation/Channel>
#include "ReadWrite.h"

namespace ive{
class Animation : public osgAnimation::Animation, public ReadWrite {
public:
	void write(DataOutputStream* out);
	void read(DataInputStream* in);

protected:
   //!
   void readChannel(DataInputStream* in, osgAnimation::Channel* ch);
   //!
   void writeChannel(DataOutputStream* out, osgAnimation::Channel* ch);
   //!
   template <typename ContainerType, typename ValueType>
   void readContainer(DataInputStream* in, ContainerType* container);
   //!
   template <typename ContainerType, typename ValueType, typename InternalValueType>
   void readContainer2(DataInputStream* in, ContainerType* container);
   //!
   template <typename ContainerType>
   void writeContainer(DataOutputStream* out, ContainerType* container);
   //!
   template <typename ContainerType>
   void writeContainer2(DataOutputStream* out, ContainerType* container);
   //!
   bool checkChannels(const osgAnimation::Animation& ani);
   //!
   bool readChannels(DataInputStream* in, osgAnimation::Animation& ani);
   //!
   bool  writeChannels(DataOutputStream* out, const osgAnimation::Animation& ani);
};
}

#endif
