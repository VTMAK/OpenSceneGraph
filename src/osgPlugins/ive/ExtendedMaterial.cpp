/******************************************************************************
** Copyright (c) 2017 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file ExtendedMaterial.cxx 
//! \brief Contains ExtendedMaterial class. 
#include "Exception.h"
#include "ExtendedMaterial.h"
#include "Object.h"

using namespace ive;

void ExtendedMaterial::write(DataOutputStream* out){
   // Write ExtendedMaterial's identification.
   out->writeInt(IVEEXTENDEDMATERIAL);
   // If the osg class is inherited by any other class we should also write this to file.
   osg::Object*  obj = dynamic_cast<osg::Object*>(this);
   if (obj){
      ((ive::Object*)(obj))->write(out);
   }
   else
      out_THROW_EXCEPTION("ExtendedMaterial::write(): Could not cast this osg::ExtendedMaterial to an osg::Object.");

   for (int i = osg::ExtendedMaterial::AMBIENT_LAYER; i <= PHYSICAL_MAP_LAYER;++i)
   {
      out->writeInt(effectTextureIndexMap[i]);
      if (effectTextureIndexMap[i] >= 0)
      {
         out->writeString(effectTextureNameMap[i]);
      }
   }
}

void ExtendedMaterial::read(DataInputStream* in){
   // Read ExtendedMaterial's identification.
   int id = in->peekInt();
   if (id == IVEEXTENDEDMATERIAL){
      // Code to read ExtendedMaterial's properties.
      id = in->readInt();
      // If the osg class is inherited by any other class we should also read this from file.
      osg::Object*  obj = dynamic_cast<osg::Object*>(this);
      if (obj){
         ((ive::Object*)(obj))->read(in);
      }
      else
         in_THROW_EXCEPTION("ExtendedMaterial::read(): Could not cast this osg::ExtendedMaterial to an osg::Object.");

      for (int i = osg::ExtendedMaterial::AMBIENT_LAYER; i <= PHYSICAL_MAP_LAYER; ++i)
      {
         effectTextureIndexMap[i] = in->readInt();
         if (effectTextureIndexMap[i] >= 0)
         {
            effectTextureNameMap[i] = in->readString();
         }
      }

   }
   else{
      in_THROW_EXCEPTION("ExtendedMaterial::read(): Expected ExtendedMaterial identification.");
   }
}
