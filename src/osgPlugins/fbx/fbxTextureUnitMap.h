/******************************************************************************
** Copyright(c) 2018 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file fbxTextureUnitMap.h
//! \brief class to hold the texture unit used the FBX loader
//! \ingroup osgdb_fbx
#pragma once


#include <string>
#include <map>

//! \brief hold the texture unit to pack them
//! and avoid the reserve texture units from 
//! DtOsgEffectTextureController::installEffectTexture (7-23)
class textureUnit
{

public:

   //! CTOR
   textureUnit(const std::string& name, unsigned int index);

   //! Copy CTOR
   textureUnit(const textureUnit& unit2);
   
   //! DTOR
   virtual ~textureUnit();

   //! return the texture unit (index)
   unsigned int index() const;

   //! return the name of the texture unit
   std::string name() const;

protected:

   //! Name of the texture unit
   std::string myName;

   //! Index of the texture unit (0 should be diffuse all the time)
   unsigned int myIndex;
};

//! \brief hold a map of the texture unit used
class textureUnitMap
{
public:

   //! CTOR
   textureUnitMap();

   //! DTOR
   virtual ~textureUnitMap();

   //! find a texture unit in the map (create if it does not exist)
   textureUnit getOrCreate(const std::string& textureUnitName);


protected:

   // index of the next texture unit to be added
   unsigned int myGlobalIndex;

   // define for the current texture unit map
   typedef std::map<std::string, textureUnit> TexureUnitMapType;

   // texture unit map
   TexureUnitMapType myMap;

};

