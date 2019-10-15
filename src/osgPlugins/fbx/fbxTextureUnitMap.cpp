/******************************************************************************
** Copyright(c) 2018 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file fbxTextureUnitMap.cpp
//! \brief class to hold the texture unit used the FBX loader
//! \ingroup osgdb_fbx

#include "fbxTextureUnitMap.h"

textureUnit::textureUnit(const std::string& name, unsigned int index) :
   myName(name),
   myIndex(index)
{ 

}

textureUnit::textureUnit(const textureUnit& unit2) :
   myName(unit2.myName),
   myIndex(unit2.myIndex)
{

}

textureUnit::~textureUnit()
{

}

unsigned int textureUnit::index() const
{
   return myIndex;
}

std::string textureUnit::name() const
{
   return myName;
}


textureUnitMap::textureUnitMap() :
   myGlobalIndex(0)
{

}

textureUnitMap::~textureUnitMap()
{

}

textureUnit textureUnitMap::getOrCreate(const std::string& textureUnitName)
{
   TexureUnitMapType::const_iterator it = myMap.find(textureUnitName);
   if (it != myMap.end())
   {
      // found it return a copy
      return it->second;
   }
   else
   {
      // create a new (diffuse should always be the first added by the user)
      textureUnit temp(textureUnitName, myGlobalIndex);
      myMap.insert(std::pair<std::string, textureUnit>(textureUnitName, temp));
      myGlobalIndex++;
      //avoid the reserve texture units from
      //DtOsgEffectTextureController::installEffectTexture (7-23)
      if (myGlobalIndex > 6)
      {
         myGlobalIndex = 24;
      }
      return temp;
   }
}