/******************************************************************************
** Copyright (c) 2019 VT-MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/
#include "Exception.h"
#include "Animation.h"
#include "Object.h"
#include <osgAnimation/BasicAnimationManager>

using namespace ive;

void Animation::write(DataOutputStream* out){
   if (out->getVersion() >= VERSION_0047)
   {
      // Write Animation's identification.
      out->writeInt(IVEANIMATION);

      // If the osg class is inherited by any other class we should also write this to file.
      osg::Object*  obj = dynamic_cast<osg::Object*>(this);
      if(obj){
         ((ive::Object*)(obj))->write(out);
      }
      else
      {
         out_THROW_EXCEPTION("Animation::write(): Could not cast this osgAnimation::Animation to an osg::Object.");
      }

      // Write out Animation's properties
      out->writeDouble(_duration);
      out->writeDouble(_originalDuration);
      out->writeFloat(_weight);
      out->writeDouble(_startTime);
      out->writeChar(_playmode);

      // Write channels
      const osgAnimation::Animation* animation = dynamic_cast<const osgAnimation::Animation*>(this);
      if (animation)
      {
         writeChannels(out, *animation);
      }
      else
      {
         out_THROW_EXCEPTION("Animation::write(): Could not cast this osgAnimation::Animation to an const osgAnimation::Animation.");
      }
   }
}

void Animation::read(DataInputStream* in){
   if (in->getVersion() >= VERSION_0047)
   {
      // Read Animation's identification.
      int id = in->peekInt();
      if (id == IVEANIMATION) {
         // Code to read Animation's properties.
         id = in->readInt();
         // If the osg class is inherited by any other class we should also read this from file.
         osg::Object*  obj = dynamic_cast<osg::Object*>(this);
         if (obj) {
            ((ive::Object*)(obj))->read(in);
         }
         else
         {
            in_THROW_EXCEPTION("Animation::read(): Could not cast this osgAnimation::Animation to an osg::Object.");
         }
         // Read animation props
         _duration = in->readDouble();
         _originalDuration = in->readDouble();
         _weight = in->readFloat();
         _startTime = in->readDouble();
         _playmode = (PlayMode)in->readChar();

         // Add animation manager and register the animation like in FBX 
         // It will later be de-register and re-register in Vantage
         osgAnimation::AnimationManagerBase* pAnimationManager = in->animationManager();
         if (!pAnimationManager) pAnimationManager = new osgAnimation::BasicAnimationManager;
         osgAnimation::Animation* anim = dynamic_cast<osgAnimation::Animation*>(this);
         const osgAnimation::AnimationList& animList = pAnimationManager->getAnimationList();
         bool found = false;
         for (size_t i = 0; i < animList.size(); ++i)
         {
            if (animList[i]->getName() == anim->getName())
            {
               found = true;
            }
         }
         // if this animation is not already register add it
         if (anim && found == false)
         {
            pAnimationManager->registerAnimation(anim);
         }

         // Read channels
         readChannels(in, *this);
      }
      else {
         in_THROW_EXCEPTION("Animation::read(): Expected Animation identification");
      }
   }
}

// 
// The code below is a modified copied 
// of osgWrappers\serializers\osgAnimation
//

void Animation::readChannel(DataInputStream* in, osgAnimation::Channel* ch)
{
   std::string name = in->readString();   
   std::string targetName = in->readString(); 
   ch->setName(name);
   ch->setTargetName(targetName);
}

template <typename ContainerType, typename ValueType>
void Animation::readContainer(DataInputStream* in, ContainerType* container)
{
   typedef typename ContainerType::KeyType KeyType;
   bool hasContainer = in->readBool();  
   if (hasContainer)
   {
      unsigned int size = in->readSize(); 
      for (unsigned int i = 0; i < size; ++i)
      {
         double time = 0.0;
         ValueType value;
         (*in) >> time >> value;
         container->push_back(KeyType(time, value));
      }
   }
}

template <typename ContainerType, typename ValueType, typename InternalValueType>
void Animation::readContainer2(DataInputStream* in, ContainerType* container)
{
   typedef typename ContainerType::KeyType KeyType;
   bool hasContainer = in->readBool(); 
   if (hasContainer)
   {
      unsigned int size = in->readSize(); 
      for (unsigned int i = 0; i < size; ++i)
      {
         double time = 0.0f;
         InternalValueType pos, ptIn, ptOut;
         (*in) >> time >> pos >> ptIn >> ptOut;
         container->push_back(KeyType(time, ValueType(pos, ptIn, ptOut)));
      }
   }
}

#define READ_CHANNEL_FUNC( NAME, CHANNEL, CONTAINER, VALUE ) \
    if ( type==#NAME ) { \
        CHANNEL* ch = new CHANNEL; \
        readChannel( in, ch ); \
        readContainer<CONTAINER, VALUE>( in, ch->getOrCreateSampler()->getOrCreateKeyframeContainer() ); \
        if ( ch ) ani.addChannel( ch ); \
        continue; \
    }

#define READ_CHANNEL_FUNC2( NAME, CHANNEL, CONTAINER, VALUE, INVALUE ) \
    if ( type==#NAME ) { \
        CHANNEL* ch = new CHANNEL; \
        readChannel( in, ch ); \
        readContainer2<CONTAINER, VALUE, INVALUE>( in, ch->getOrCreateSampler()->getOrCreateKeyframeContainer() ); \
        if ( ch ) ani.addChannel( ch ); \
        continue; \
    }

//// writing channel helpers
//
void Animation::writeChannel(DataOutputStream* out, osgAnimation::Channel* ch)
{
   out->writeString(ch->getName());      
   out->writeString(ch->getTargetName()); 
}

template <typename ContainerType>
void Animation::writeContainer(DataOutputStream* out, ContainerType* container)
{   
   out->writeBool(container != NULL); 
   if (container != NULL)
   {
      out->writeSize(container->size()); 
      for (unsigned int i = 0; i < container->size(); ++i)
      {
         (*out) << (*container)[i].getTime() << (*container)[i].getValue();// << std::endl;
      }
   }
}

template <typename ContainerType>
void Animation::writeContainer2(DataOutputStream* out, ContainerType* container)
{
   typedef typename ContainerType::KeyType KeyType;
   out->writeBool(container != NULL); 
   if (container != NULL)
   {
      out->writeSize(container->size()); 
      for (unsigned int i = 0; i < container->size(); ++i)
      {
         const KeyType& keyframe = (*container)[i];
         (*out) << keyframe.getTime() << keyframe.getValue().getPosition()
            << keyframe.getValue().getControlPointIn()
            << keyframe.getValue().getControlPointOut();// << std::endl;
      }
   }
}

#define WRITE_CHANNEL_FUNC( NAME, CHANNEL, CONTAINER ) \
    CHANNEL* ch_##NAME = dynamic_cast<CHANNEL*>(ch); \
    if ( ch_##NAME ) { \
        (*out) << std::string(#NAME);  \
        writeChannel( out, ch_##NAME ); \
        writeContainer<CONTAINER>( out, ch_##NAME ->getSamplerTyped()->getKeyframeContainerTyped() ); \
        continue; \
    }

#define WRITE_CHANNEL_FUNC2( NAME, CHANNEL, CONTAINER ) \
    CHANNEL* ch_##NAME = dynamic_cast<CHANNEL*>(ch); \
    if ( ch_##NAME ) { \
        (*out) << std::string(#NAME);  \
        writeChannel( out, ch_##NAME ); \
        writeContainer2<CONTAINER>( out, ch_##NAME ->getSamplerTyped()->getKeyframeContainerTyped() ); \
        continue; \
    }

// _channels
bool Animation::checkChannels(const osgAnimation::Animation& ani)
{
   return ani.getChannels().size() > 0;
}

bool Animation::readChannels(DataInputStream* in, osgAnimation::Animation& ani)
{
   unsigned int size = in->readSize(); 
   for (unsigned int i = 0; i < size; ++i)
   {
      std::string type = in->readString(); 

      READ_CHANNEL_FUNC(DoubleStepChannel, osgAnimation::DoubleStepChannel, osgAnimation::DoubleKeyframeContainer, double);
      READ_CHANNEL_FUNC(FloatStepChannel, osgAnimation::FloatStepChannel, osgAnimation::FloatKeyframeContainer, float);
      READ_CHANNEL_FUNC(Vec2StepChannel, osgAnimation::Vec2StepChannel, osgAnimation::Vec2KeyframeContainer, osg::Vec2);
      READ_CHANNEL_FUNC(Vec3StepChannel, osgAnimation::Vec3StepChannel, osgAnimation::Vec3KeyframeContainer, osg::Vec3);
      READ_CHANNEL_FUNC(Vec4StepChannel, osgAnimation::Vec4StepChannel, osgAnimation::Vec4KeyframeContainer, osg::Vec4);
      READ_CHANNEL_FUNC(QuatStepChannel, osgAnimation::QuatStepChannel, osgAnimation::QuatKeyframeContainer, osg::Quat);
      READ_CHANNEL_FUNC(DoubleLinearChannel, osgAnimation::DoubleLinearChannel, osgAnimation::DoubleKeyframeContainer, double);
      READ_CHANNEL_FUNC(FloatLinearChannel, osgAnimation::FloatLinearChannel, osgAnimation::FloatKeyframeContainer, float);
      READ_CHANNEL_FUNC(Vec2LinearChannel, osgAnimation::Vec2LinearChannel, osgAnimation::Vec2KeyframeContainer, osg::Vec2);
      READ_CHANNEL_FUNC(Vec3LinearChannel, osgAnimation::Vec3LinearChannel, osgAnimation::Vec3KeyframeContainer, osg::Vec3);
      READ_CHANNEL_FUNC(Vec4LinearChannel, osgAnimation::Vec4LinearChannel, osgAnimation::Vec4KeyframeContainer, osg::Vec4);
      READ_CHANNEL_FUNC(QuatSphericalLinearChannel, osgAnimation::QuatSphericalLinearChannel,
         osgAnimation::QuatKeyframeContainer, osg::Quat);
      READ_CHANNEL_FUNC(MatrixLinearChannel, osgAnimation::MatrixLinearChannel,
         osgAnimation::MatrixKeyframeContainer, osg::Matrix);
      READ_CHANNEL_FUNC2(FloatCubicBezierChannel, osgAnimation::FloatCubicBezierChannel,
         osgAnimation::FloatCubicBezierKeyframeContainer,
         osgAnimation::FloatCubicBezier, float);
      READ_CHANNEL_FUNC2(DoubleCubicBezierChannel, osgAnimation::DoubleCubicBezierChannel,
         osgAnimation::DoubleCubicBezierKeyframeContainer,
         osgAnimation::DoubleCubicBezier, double);
      READ_CHANNEL_FUNC2(Vec2CubicBezierChannel, osgAnimation::Vec2CubicBezierChannel,
         osgAnimation::Vec2CubicBezierKeyframeContainer,
         osgAnimation::Vec2CubicBezier, osg::Vec2);
      READ_CHANNEL_FUNC2(Vec3CubicBezierChannel, osgAnimation::Vec3CubicBezierChannel,
         osgAnimation::Vec3CubicBezierKeyframeContainer,
         osgAnimation::Vec3CubicBezier, osg::Vec3);
      READ_CHANNEL_FUNC2(Vec4CubicBezierChannel, osgAnimation::Vec4CubicBezierChannel,
         osgAnimation::Vec4CubicBezierKeyframeContainer,
         osgAnimation::Vec4CubicBezier, osg::Vec4);
   }
   return true;
}

bool  Animation::writeChannels(DataOutputStream* out, const osgAnimation::Animation& ani)
{
   const osgAnimation::ChannelList& channels = ani.getChannels();
   out->writeSize(channels.size()); 
   for (osgAnimation::ChannelList::const_iterator itr = channels.begin();
      itr != channels.end(); ++itr)
   {
      osgAnimation::Channel* ch = itr->get();
      WRITE_CHANNEL_FUNC(DoubleStepChannel, osgAnimation::DoubleStepChannel, osgAnimation::DoubleKeyframeContainer);
      WRITE_CHANNEL_FUNC(FloatStepChannel, osgAnimation::FloatStepChannel, osgAnimation::FloatKeyframeContainer);
      WRITE_CHANNEL_FUNC(Vec2StepChannel, osgAnimation::Vec2StepChannel, osgAnimation::Vec2KeyframeContainer);
      WRITE_CHANNEL_FUNC(Vec3StepChannel, osgAnimation::Vec3StepChannel, osgAnimation::Vec3KeyframeContainer);
      WRITE_CHANNEL_FUNC(Vec4StepChannel, osgAnimation::Vec4StepChannel, osgAnimation::Vec4KeyframeContainer);
      WRITE_CHANNEL_FUNC(QuatStepChannel, osgAnimation::QuatStepChannel, osgAnimation::QuatKeyframeContainer);
      WRITE_CHANNEL_FUNC(DoubleLinearChannel, osgAnimation::DoubleLinearChannel, osgAnimation::DoubleKeyframeContainer);
      WRITE_CHANNEL_FUNC(FloatLinearChannel, osgAnimation::FloatLinearChannel, osgAnimation::FloatKeyframeContainer);
      WRITE_CHANNEL_FUNC(Vec2LinearChannel, osgAnimation::Vec2LinearChannel, osgAnimation::Vec2KeyframeContainer);
      WRITE_CHANNEL_FUNC(Vec3LinearChannel, osgAnimation::Vec3LinearChannel, osgAnimation::Vec3KeyframeContainer);
      WRITE_CHANNEL_FUNC(Vec4LinearChannel, osgAnimation::Vec4LinearChannel, osgAnimation::Vec4KeyframeContainer);
      WRITE_CHANNEL_FUNC(QuatSphericalLinearChannel, osgAnimation::QuatSphericalLinearChannel,
         osgAnimation::QuatKeyframeContainer);
      WRITE_CHANNEL_FUNC(MatrixLinearChannel, osgAnimation::MatrixLinearChannel,
         osgAnimation::MatrixKeyframeContainer);
      WRITE_CHANNEL_FUNC2(FloatCubicBezierChannel, osgAnimation::FloatCubicBezierChannel,
         osgAnimation::FloatCubicBezierKeyframeContainer);
      WRITE_CHANNEL_FUNC2(DoubleCubicBezierChannel, osgAnimation::DoubleCubicBezierChannel,
         osgAnimation::DoubleCubicBezierKeyframeContainer);
      WRITE_CHANNEL_FUNC2(Vec2CubicBezierChannel, osgAnimation::Vec2CubicBezierChannel,
         osgAnimation::Vec2CubicBezierKeyframeContainer);
      WRITE_CHANNEL_FUNC2(Vec3CubicBezierChannel, osgAnimation::Vec3CubicBezierChannel,
         osgAnimation::Vec3CubicBezierKeyframeContainer);
      WRITE_CHANNEL_FUNC2(Vec4CubicBezierChannel, osgAnimation::Vec4CubicBezierChannel,
         osgAnimation::Vec4CubicBezierKeyframeContainer);
   }
   return true;
}
