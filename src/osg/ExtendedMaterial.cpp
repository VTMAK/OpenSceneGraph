/******************************************************************************
** Copyright (c) 2021 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file ExtendedMaterial.cxx 
//! \brief Contains osg::ExtendedMaterial class. 

#include <osg/ExtendedMaterial>
using namespace osg;

ExtendedMaterial::ExtendedMaterial()
   : Material()
   , used(false)
   , shadeModel(ExtendedMaterial::PER_FRAG_PHONG)
   , alphaQuality(ExtendedMaterial::MULTISAMPLE_TRANSPARENCY)
   , lightMapMaximumIntensity(0)
   , shadowMapMaximumIntensity(0)
   , globalAlpha(1)
{
   for (int i = AMBIENT_LAYER; i <= PHYSICAL_MAP_LAYER; ++i)
   {
      effectTextureIndexMap[i] = -1;
      effectTextureNameMap[i] = -1;
   }
}

ExtendedMaterial::~ExtendedMaterial()
{
   effectTextureIndexMap.clear();
   effectTextureNameMap.clear();
}

ExtendedMaterial& ExtendedMaterial::operator = (const ExtendedMaterial& rhs)
{
   if (&rhs == this) return *this;

   effectTextureIndexMap = rhs.effectTextureIndexMap;
   effectTextureNameMap = rhs.effectTextureNameMap;
   used = rhs.used;
   globalAlpha = rhs.globalAlpha;
   shadeModel = rhs.shadeModel;
   alphaQuality = rhs.alphaQuality;
   lightMapMaximumIntensity = rhs.lightMapMaximumIntensity;
   shadowMapMaximumIntensity = rhs.shadowMapMaximumIntensity;
   reflectionTintColor = rhs.reflectionTintColor;
   return *this;
}

void ExtendedMaterial::apply(State& state) const
{
   //This will override if there is already a osg::Materail in the stateset,
   //Since the ExtendedMaterial type is after Material type
   Material::apply(state);
}

const ExtendedMaterial::EffectTextureIndexMap& ExtendedMaterial::getEffectTextureIndexMap() const
{
   return effectTextureIndexMap;
}

void ExtendedMaterial::setEffectTextureIndex(EffectTextureLayer layer, int index)
{
   effectTextureIndexMap[layer] = index;
}

void ExtendedMaterial::setEffectTextureName(EffectTextureLayer layer, const std::string& name)
{
   effectTextureNameMap[layer] = name;
}

bool ExtendedMaterial::isEffectTexture(const std::string& name, EffectTextureLayer& layer)
{
   if (effectTextureNameMap.size() == 0) 
   {
      return false;
   }
   for (int i = osg::ExtendedMaterial::AMBIENT_LAYER; i <= osg::ExtendedMaterial::METAL_GLOSS_AO_LAYER; ++i)
   {
      //Diffuse texture should never be treated as effect texture since it's actually the base texture
      if (i == osg::ExtendedMaterial::DIFFUSE_LAYER)
      {
         continue;
      }

      EffectTextureNameMap::iterator iter = effectTextureNameMap.find(i);
      if (iter!= effectTextureNameMap.end() && checkMatch(iter->second, name))
      {
         layer = (osg::ExtendedMaterial::EffectTextureLayer)i;
         return true;
      }
   }
   return false;
}

void ExtendedMaterial::setUsed(bool _used)
{
   used = _used;
}

bool ExtendedMaterial::getUsed() const
{
   return used;
}

void ExtendedMaterial::setShadedModel(ShadeModel _shadeModel)
{
   shadeModel = _shadeModel;
}

ExtendedMaterial::ShadeModel ExtendedMaterial::getShadedModel() const
{
   return shadeModel;
}

void ExtendedMaterial::setGlobalAlpha(float _globalAlpha)
{
   globalAlpha = _globalAlpha;
}

float ExtendedMaterial::getGlobalAlpha() const
{
   return globalAlpha;
}

void ExtendedMaterial::setAmbientFrontAndBack(const osg::Vec3f& v)
{
   setAmbient(Material::FRONT_AND_BACK, osg::Vec4f(v, globalAlpha));
}

void ExtendedMaterial::setDiffuseFrontAndBack(const osg::Vec3f& v)
{
   setDiffuse(Material::FRONT_AND_BACK, osg::Vec4f(v, globalAlpha));
}

void ExtendedMaterial::setSpecularFrontAndBack(const osg::Vec3f& v)
{
   setSpecular(Material::FRONT_AND_BACK, osg::Vec4f(v, globalAlpha));
}

void ExtendedMaterial::setShininessFrontAndBack(float shininess)
{
   if (shininess >= 0.0f)
   {
      setShininess(osg::Material::FRONT_AND_BACK, shininess);
   }
   else
   {
      OSG_INFO << "Warning: OpenFlight shininess value out of range: " << shininess << std::endl;
   }
}

void ExtendedMaterial::setEmissiveFrontAndBack(const osg::Vec3f& v)
{
   setEmission(Material::FRONT_AND_BACK, osg::Vec4f(v, globalAlpha));
}

void ExtendedMaterial::setAlphaFrontAndBack(float alpha)
{
   setAlpha(Material::FRONT_AND_BACK, alpha);
}

void ExtendedMaterial::setAlphaQuality(ExtendedMaterial::AlphaQuality quality)
{
   alphaQuality = quality;
}

void ExtendedMaterial::setLightMapMaximumIntensity(float intensity)
{
   lightMapMaximumIntensity = intensity;
}

void ExtendedMaterial::setShadowMapMaximumIntensity(float intensity)
{
   shadowMapMaximumIntensity = intensity;
}

void ExtendedMaterial::setReflectionTintColor(const osg::Vec3f& v)
{
   reflectionTintColor = v;
}

bool ExtendedMaterial::checkMatch(const std::string& effectname, const std::string& textureName)
{
   //if two names are identical, return true
   if (effectname == textureName)
   {
      return true;
   }
   //If the simple file name(/a/b/c.Ext => c.Ext) are identical, return true
   else if (getSimpleFileName(effectname) == getSimpleFileName(textureName))
   {
      return true;
   }
   //If it's a meif file, we need take out the extension of the original format before do the comparison.
   //For example, compare BuildingsRange220OneUnitWhite_png.meif and BuildingsRange220OneUnitWhite.png
   else if (getNameLessExtension(effectname) == getNameLessExtension(textureName))
   {
      return true;
   }

   return false;
}

std::string ExtendedMaterial::getSimpleFileName(const std::string& fileName)
{
   
   //get simple file name, Ex: /a/b/c.Ext => c.Ext
   std::string::size_type slash = fileName.find_last_of("/\\");
   if (slash == std::string::npos)
   {
      return fileName;
   }

   return std::string(fileName.begin() + slash + 1, fileName.end());

}

std::string ExtendedMaterial::getLowerCaseExtension(const std::string& fileName)
{
   std::string extension;
   std::string::size_type dot = fileName.find_last_of('.');
   std::string::size_type slash = fileName.find_last_of("/\\");
   if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
   {
      extension = std::string("");
   }
   else
   {
      extension = std::string(fileName.begin() + dot + 1, fileName.end());
   }

   std::string lowcase_str(extension);
   for (std::string::iterator itr = lowcase_str.begin();
      itr != lowcase_str.end();
      ++itr)
   {
      *itr = tolower(*itr);
   }
   return lowcase_str;
}

std::string ExtendedMaterial::getNameLessExtension(const std::string& textureName)
{
   std::string simpleFileName = getSimpleFileName(textureName);
   std::string ext = getLowerCaseExtension(textureName);

   if (ext == "meif")
   {
      if (simpleFileName.find_last_of("_") != simpleFileName.npos)
      {
         size_t pos = simpleFileName.find_last_of("_");
         simpleFileName = simpleFileName.substr(0, pos);
      }
   }
   else
   {
      std::string::size_type dot = simpleFileName.find_last_of('.');
      std::string::size_type slash = simpleFileName.find_last_of("/\\");        // Finds forward slash *or* back slash
      if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
      {

      }
      else
      {
         simpleFileName = std::string(simpleFileName.begin(), simpleFileName.begin() + dot);
      }
   }


   return simpleFileName;
}