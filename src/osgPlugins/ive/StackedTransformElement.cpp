/******************************************************************************
** Copyright (c) 2019 VT-MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/
#include "Exception.h"
#include "StackedTransformElement.h"
#include "Object.h"

using namespace ive;

void StackedTransformElement::write(DataOutputStream* out) {
   //VRV Patch to add support for type names being passed down.
   if (out->getVersion() >= VERSION_0047)
   {
      // Write StackedTransformElement's identification.
      out->writeInt(IVESTACKEDTRANSFORMELEMENT);

      // ** NOTE Template element let's write the object properties after
      // Write the StackedTransformElement
      out->writeStackedTransformElement(this);
      
      // If the osg class is inherited by any other class we should also write this to file.
      osg::Object*  obj = dynamic_cast<osg::Object*>(this);
      if (obj) {
         ((ive::Object*)(obj))->write(out);
      }
      else
      {
         out_THROW_EXCEPTION("StackedTransformElement::write(): Could not cast this osgAnimation::StackedTransformElement to an osg::Object.");
      }
   }
   //End VRV Patch
}

void StackedTransformElement::read(DataInputStream* in)
{
   OSG_WARN << "StackedTransformElement::read(): not implemented used static_read() instead" << std::endl;
}

osg::Object* StackedTransformElement::static_read(DataInputStream* in) {
   //VRV Patch to add support for type names being passed down.
   if (in->getVersion() >= VERSION_0047)
   {
      // Read StackedTransformElement's identification.
      int id = in->peekInt();
      if (id == IVESTACKEDTRANSFORMELEMENT) {
         // Code to read StackedTransformElement's properties.
         id = in->readInt();

         // ** NOTE Template element let's read the object properties after         
         // Read the StackedTransformElement name
         osg::Object* obj = in->readStackedTransformElement();

         // If the osg class is inherited by any other class we should also read this from file.
         if (obj) {
            ((ive::Object*)(obj))->read(in);
            return obj;
         }
         else
            OSG_WARN << "StackedTransformElement::read(): Could not cast this osgAnimation::StackedTransformElement to an osg::Object." << std::endl;

      }
      else {
         OSG_WARN << "StackedTransformElement::read(): Expected StackedTransformElement identification" << std::endl;
      }
   }
   return NULL;
   //End VRV Patch
}
