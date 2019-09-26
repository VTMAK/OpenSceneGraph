/******************************************************************************
** Copyright (c) 2019 VT-MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/
#include "Exception.h"
#include "UpdateMatrixTransform.h"
#include "Object.h"
#include "StackedTransformElement.h"

using namespace ive;

void UpdateMatrixTransform::write(DataOutputStream* out) {
   //VRV Patch to add support for type names being passed down.
   if (out->getVersion() >= VERSION_0047)
   {
      // Write UpdateMatrixTransform's identification.
      out->writeInt(IVEUPDATEMATRIXTRANSFORM);

      // If the osg class is inherited by any other class we should also write this to file.
      osg::Object*  obj = dynamic_cast<osg::Object*>(this);
      if (obj) {
         ((ive::Object*)(obj))->write(out);
      }
      else
      {
         out_THROW_EXCEPTION("UpdateMatrixTransform::write(): Could not cast this osgAnimation::UpdateMatrixTransform to an osg::Object.");
      }

      // Write the AnimationUpdateCallback<> name
      AnimationUpdateCallback<osg::NodeCallback>* auc = dynamic_cast<AnimationUpdateCallback<osg::NodeCallback>*>(this);
      if (auc)
      {
         out->writeString(auc->getName());
      }

      // Write the stack
      writeStackedTransforms(out, *this);
   }
   //End VRV Patch
}

void UpdateMatrixTransform::read(DataInputStream* in) {
   //VRV Patch to add support for type names being passed down.
   if (in->getVersion() >= VERSION_0047)
   {
      // Read UpdateMatrixTransform's identification.
      int id = in->peekInt();
      if (id == IVEUPDATEMATRIXTRANSFORM) {
         // Code to read UpdateMatrixTransform's properties.
         id = in->readInt();

         // If the osg class is inherited by any other class we should also read this from file.
         osg::Object* obj = dynamic_cast<osg::Object*>(this);
         if (obj) {
            ((ive::Object*)(obj))->read(in);
         }
         else
            in_THROW_EXCEPTION("UpdateMatrixTransform::read(): Could not cast this osgAnimation::UpdateMatrixTransform to an osg::Object.");

         // Write the AnimationUpdateCallback<> name
         AnimationUpdateCallback<osg::NodeCallback>* auc = dynamic_cast<AnimationUpdateCallback<osg::NodeCallback>*>(this);
         if (auc)
         {
            auc->setName(in->readString());
         }

         // Read the stack
         readStackedTransforms(in, *this);
      }
      else {
         in_THROW_EXCEPTION("UpdateMatrixTransform::read(): Expected UpdateMatrixTransform identification");
      }
   }
   //End VRV Patch
}

// 
// The code below is a modified copied 
// of osgWrappers\serializers\osgUpdateMatrixTransform
//

bool UpdateMatrixTransform::readStackedTransforms(DataInputStream* in, osgAnimation::UpdateMatrixTransform& obj)
{
   osgAnimation::StackedTransform& transform = obj.getStackedTransforms();
   osg::Matrixd mt;
   (*in) >> mt;
   transform.setMatrix(mt);
   unsigned int size = in->readSize(); 
   for (unsigned int i = 0; i < size; ++i)
   {
      osgAnimation::StackedTransformElement* element =
         dynamic_cast<osgAnimation::StackedTransformElement*>(ive::StackedTransformElement::static_read(in));

      if (element) transform.push_back(element);
   }
   return true;
}

bool UpdateMatrixTransform::writeStackedTransforms(DataOutputStream* out, const osgAnimation::UpdateMatrixTransform& obj)
{
   const osgAnimation::StackedTransform& transform = obj.getStackedTransforms();
   const osg::Matrixd& mt = transform.getMatrix();
   (*out) << mt;
   out->writeSize(transform.size()); 
   for (osgAnimation::StackedTransform::const_iterator itr = transform.begin();
      itr != transform.end(); ++itr)
   {
      ((ive::StackedTransformElement*)(itr->get()))->write(out);
   }
   return true;
}