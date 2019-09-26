/******************************************************************************
** Copyright (c) 2019 VT-MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/
#include "Exception.h"
#include "AnimationMatrixTransform.h"
#include "Animation.h"
#include "MatrixTransform.h"
#include "UpdateMatrixTransform.h"

using namespace ive;

void AnimationMatrixTransform::write(DataOutputStream* out){
   //VRV Patch to add support for type names being passed down.
   if (out->getVersion() >= VERSION_0047)
   {
      // Write AnimationMatrixTransform's identification.
      out->writeInt(IVEANIMATIONMATRIXTRANSFORM);

      // If the osg class is inherited by any other class we should also write this to file.
      osg::MatrixTransform*  mt = dynamic_cast<osg::MatrixTransform*>(this);
      if (mt) {
         ((ive::MatrixTransform*)(mt))->write(out);
      }
      else
      {
         out_THROW_EXCEPTION("AnimationMatrixTransform::write(): Could not cast this osgAnimation::AnimationMatrixTransform to an osg::MatrixTransform.");
      }
      // Write the animation.
      osgAnimation::Animation* animation = _animation.get();
      if (animation)
      {
         // ONLY WRITE EACH ANIMATION ONCE !!
         std::vector<osgAnimation::Animation*>::const_iterator iter;
         bool found = false;
         for (iter = out->animationList.begin(); iter != out->animationList.end(); ++iter)
         {
            if (animation == (*iter))
            {
               found = true;
               break;
            }
         }
         // Write the name of the animation
         out->writeString(animation->getName());
         // Save object name (used by animation)
         out->writeString(this->getName());
         if (!found)
         {
            // Write a bool that say if animation is written here
            out->writeBool(true);
            ((ive::Animation*)(animation))->write(out);
            out->animationList.push_back(animation);
         }
         else
         {
            // Write a bool that say that the animation is not written here
            out->writeBool(false);
         }
         // Write the update matrix
         osgAnimation::UpdateMatrixTransform* pUpdateMT = dynamic_cast<osgAnimation::UpdateMatrixTransform*>(this->getUpdateCallback());
         if (pUpdateMT)
         {
            ((ive::UpdateMatrixTransform*)(pUpdateMT))->write(out);
         }
         else
         {
            out_THROW_EXCEPTION("AnimationMatrixTransform::write(): Could not cast this osg::CallbackNode to an osgAnimation::UpdateMatrixTransform.");
         }
      }
   }
   //End VRV Patch
}

void AnimationMatrixTransform::read(DataInputStream* in){
   //VRV Patch to add support for type names being passed down.
   if (in->getVersion() >= VERSION_0047)
   {
      // Read AnimationMatrixTransform's identification.
      int id = in->peekInt();
      if(id == IVEANIMATIONMATRIXTRANSFORM){
         // Code to read AnimationMatrixTransform's properties.
         id = in->readInt();
         // If the osg class is inherited by any other class we should also read this from file.
         osg::MatrixTransform*  mt = dynamic_cast<osg::MatrixTransform*>(this);
         if(mt){
            ((ive::MatrixTransform*)(mt))->read(in);
         }
         else
            in_THROW_EXCEPTION("AnimationMatrixTransform::read(): Could not cast this osgAnimation::AnimationMatrixTransform to an osg::MatrixTransform.");


         // Read the name of the animation
         std::string animationName = in->readString();
         // Read name of the object to animate
         std::string animationObject = in->readString();

         // Write a bool that say that the animation is not written here
         bool readAnimation = in->readBool();

         if (readAnimation)
         {
            osgAnimation::Animation* animation = new osgAnimation::Animation();
            ((ive::Animation*)(animation))->read(in);
            setAnimation(animation);
            // Add the animation to the list to be reused 
            in->animationList.push_back(animation);
         }
         else
         {
            // try to get the animation from the vector 
            std::vector<osgAnimation::Animation*>::const_iterator iter;
            bool found = false;
            for (iter = in->animationList.begin(); iter != in->animationList.end(); ++iter)
            {
               if ((*iter)->getName() == animationName)
               {
                  found = true;
                  setAnimation(*iter);
               }
            }
            if (!found)
            {
               in_THROW_EXCEPTION("AnimationMatrixTransform::read(): Unable to found animation.");
            }
         }
         // read Stack matrix transform
         osgAnimation::UpdateMatrixTransform* pUpdateMT = new osgAnimation::UpdateMatrixTransform(animationObject);
         ((ive::UpdateMatrixTransform*)(pUpdateMT))->read(in);
         setUpdateCallback(pUpdateMT);
      }
      else{
         in_THROW_EXCEPTION("AnimationMatrixTransform::read(): Expected AnimationMatrixTransform identification");
      }
   }
   //End VRV Patch
}
