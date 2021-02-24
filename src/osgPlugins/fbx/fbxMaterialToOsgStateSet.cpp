#include "fbxMaterialToOsgStateSet.h"
#include "fbxUtil.h"
#include <sstream>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include <osg/ExtendedMaterial>

#include <boost/filesystem.hpp>
#include <boost/tokenizer.hpp>
#include <boost/token_functions.hpp>

static const std::string vrvAppDataDir = "vrvAppDataDir=";

static osg::Texture::WrapMode convertWrap(FbxFileTexture::EWrapMode wrap)
{
    return wrap == FbxFileTexture::eRepeat ?
        osg::Texture2D::REPEAT : osg::Texture2D::CLAMP_TO_EDGE;
}

StateSetContent
FbxMaterialToOsgStateSet::convert(const FbxSurfaceMaterial* pFbxMat)
{
    FbxMaterialMap::const_iterator it = _fbxMaterialMap.find(pFbxMat);
    if (it != _fbxMaterialMap.end())
        return it->second;

    osg::ref_ptr<osg::Material> pOsgMat = new osg::Material;
    pOsgMat->setName(pFbxMat->GetName());
    StateSetContent result;

    result.material = pOsgMat;
    result.extendedmaterial = new osg::ExtendedMaterial(); // create an extended material
    FbxString shadingModel = pFbxMat->ShadingModel.Get();

    const FbxSurfaceLambert* pFbxLambert = FbxCast<FbxSurfaceLambert>(pFbxMat);

    // **************** TODO OTHER PROPERTY THAT COULD BE ADDED
    //static const char* sShadingModel;
    //static const char* sMultiLayer;
    //static const char* sDisplacementColor;
    //static const char* sVectorDisplacementColor;    
    //***************************

    // diffuse map...
    const FbxProperty lProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse);
    if (lProperty.IsValid())
    {
       //FbxSurfaceMaterial::sMultiLayer
       int layeredTextureCount = lProperty.GetSrcObjectCount<FbxLayeredTexture>();
       if (layeredTextureCount > 0)
       {
          for (int j = 0; j < layeredTextureCount; j++)
          {
             FbxLayeredTexture* layered_texture = FbxCast<FbxLayeredTexture>(lProperty.GetSrcObject<FbxLayeredTexture>(j));
             if (layered_texture)
             {
                int lcount = layered_texture->GetSrcObjectCount<FbxTexture>();
                for (int k = 0; k < lcount; k++)
                {
                   FbxFileTexture* texture2 = FbxCast<FbxFileTexture>(layered_texture->GetSrcObject<FbxFileTexture>(k));

                   result.diffuseLayerTexture.push_back(fbxTextureToOsgTexture(texture2));
                   result.diffuseLayerChannel.push_back(std::string(texture2->UVSet.Get()));
                   result.diffuseLayerScaleU.push_back(texture2->GetScaleU());
                   result.diffuseLayerScaleV.push_back(texture2->GetScaleV());

                   FbxLayeredTexture::EBlendMode BlendMode;
                   osg::ref_ptr<osg::TexEnv> texenv = new osg::TexEnv;

                   if (layered_texture->GetTextureBlendMode(k, BlendMode))
                   {
                      switch (BlendMode)
                      {
                      case FbxLayeredTexture::EBlendMode::eModulate:
                         texenv->setMode(osg::TexEnv::MODULATE);
                         break;
                      //case AttrData::TEXENV_BLEND:
                      //     texenv->setMode(osg::TexEnv::BLEND);  // No equivalent in FBX
                      //     break;
                     case FbxLayeredTexture::EBlendMode::eTranslucent:
                        texenv->setMode(osg::TexEnv::DECAL);
                        break;
                     case FbxLayeredTexture::EBlendMode::eNormal:
                        texenv->setMode(osg::TexEnv::REPLACE);
                        break;
                     case FbxLayeredTexture::EBlendMode::eAdditive:
                        texenv->setMode(osg::TexEnv::ADD);
                        break;
                      }
                   }
                   //stateset->setTextureAttribute(0, texenv);
                   result.diffuseLayerEnv.push_back(texenv);
                }
             }
          }
       }
       else
       {
          int lNbTex = lProperty.GetSrcObjectCount<FbxFileTexture>();
          for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
          {
             FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
             if (lTexture)
             {
                result.diffuseTexture = fbxTextureToOsgTexture(lTexture);
                result.diffuseChannel = lTexture->UVSet.Get();
                result.diffuseScaleU = lTexture->GetScaleU();
                result.diffuseScaleV = lTexture->GetScaleV();
             }
             //For now only allow 1 texture
             break;
          }
       }
    }

    double transparencyColorFactor = 1.0;
    bool useTransparencyColorFactor = false;
    
    // opacity map...
    const FbxProperty lOpacityProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sTransparentColor);
    if (lOpacityProperty.IsValid())
    {
        FbxDouble3 transparentColor = lOpacityProperty.Get<FbxDouble3>();
        // If transparent color is defined set the transparentFactor to gray scale value of transparentColor
        if (transparentColor[0] < 1.0 || transparentColor[1] < 1.0 || transparentColor[2] < 1.0) {
            transparencyColorFactor = transparentColor[0]*0.30 + transparentColor[1]*0.59 + transparentColor[2]*0.11;
            useTransparencyColorFactor = true;
        }         

        int lNbTex = lOpacityProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
        {
            FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lOpacityProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
            if (lTexture)
            {
                // TODO: if texture image does NOT have an alpha channel, should it be added?

                result.opacityTexture = fbxTextureToOsgTexture(lTexture);
                result.opacityChannel = lTexture->UVSet.Get();
                result.opacityScaleU = lTexture->GetScaleU();
                result.opacityScaleV = lTexture->GetScaleV();
            }
            //For now only allow 1 texture
            break;
        }
    }

    // reflection map...
    const FbxProperty lReflectionProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sReflection);
    if (lReflectionProperty.IsValid())
    {
        int lNbTex = lReflectionProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
        {
            FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lReflectionProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
            if (lTexture)
            {
                // support only spherical reflection maps...
                if (FbxFileTexture::eUMT_ENVIRONMENT == lTexture->CurrentMappingType.Get() || 
                    FbxFileTexture::eUMT_UV == lTexture->CurrentMappingType.Get() )
                {
                    result.reflectionTexture = fbxTextureToOsgTexture(lTexture);
                    result.reflectionChannel = lTexture->UVSet.Get();
                    result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::REFLECTION_BASE_LAYER, lTexture->GetFileName());
                }
            }
            //For now only allow 1 texture
            break;
        }
    }

    // emissive map...
    const FbxProperty lEmissiveProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sEmissive);
    if (lEmissiveProperty.IsValid())
    {
        int lNbTex = lEmissiveProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
        {
            FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lEmissiveProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
            if (lTexture)
            {
                result.emissiveTexture = fbxTextureToOsgTexture(lTexture);
                result.emissiveChannel = lTexture->UVSet.Get();
                result.emissiveScaleU = lTexture->GetScaleU();
                result.emissiveScaleV = lTexture->GetScaleV();
                result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::LIGHT_MAP_LAYER, lTexture->GetFileName());
            }
            //For now only allow 1 texture
            break;
        }
    }

    // ambient map...
    const FbxProperty lAmbientProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sAmbient);
    if (lAmbientProperty.IsValid())
    {
        int lNbTex = lAmbientProperty.GetSrcObjectCount<FbxFileTexture>();
        for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
        {
            FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lAmbientProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
            if (lTexture)
            {
                result.ambientTexture = fbxTextureToOsgTexture(lTexture);
                result.ambientChannel = lTexture->UVSet.Get();
                result.ambientScaleU = lTexture->GetScaleU();
                result.ambientScaleV = lTexture->GetScaleV();
                // Check if we need this (this was used in the FLT plugin)
                //result.extendedmaterial->setAmbientFrontAndBack(ambient);
                result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::AMBIENT_LAYER, lTexture->GetFileName());
            }
            //For now only allow 1 texture
            break;
        }
    }

    // normal map...
    const FbxProperty lNormalProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sNormalMap);
    if (lNormalProperty.IsValid())
    {
       int lNbTex = lNormalProperty.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lNormalProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             result.normalTexture = fbxTextureToOsgTexture(lTexture);
             result.normalChannel = lTexture->UVSet.Get();
             result.normalScaleU = lTexture->GetScaleU();
             result.normalScaleV = lTexture->GetScaleV();

             result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::NORMAL_MAP_LAYER, lTexture->GetFileName());
          }
          //For now only allow 1 texture
          break;
       }
    }

    // bump map...
    const FbxProperty lBumpProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sBump);
    if (lBumpProperty.IsValid())
    {
       int lNbTex = lBumpProperty.GetSrcObjectCount<FbxFileTexture>();
       if (lNbTex > 0)
       {
          OSG_WARN << "Bump Map effect texture not currently supported by Vantage" << std::endl;
       }

       //for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       //{
       //   FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lBumpProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
       //   if (lTexture)
       //   {
       //      result.bumpTexture = fbxTextureToOsgTexture(lTexture);
       //      result.bumpChannel = lTexture->UVSet.Get();
       //      result.bumpScaleU = lTexture->GetScaleU();
       //      result.bumpScaleV = lTexture->GetScaleV();

       //      result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::BUMP_MAP_LAYER, lTexture->GetFileName());
       //   }
       //   //For now only allow 1 texture
       //   break;
       //}
    }

    // shininess map...
    const FbxProperty lShininessProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sShininess);
    if (lShininessProperty.IsValid())
    {
       int lNbTex = lShininessProperty.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lShininessProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             result.glossTexture = fbxTextureToOsgTexture(lTexture);
             result.glossChannel = lTexture->UVSet.Get();
             result.glossScaleU = lTexture->GetScaleU();
             result.glossScaleV = lTexture->GetScaleV();
             // this map to "Specular Power" in Vantage GUI
             result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::GLOSS_MAP_LAYER, lTexture->GetFileName());
          }
          //For now only allow 1 texture
          break;
       }
    }

    // *******************************************************************************
    // For now we want to pick sSpecular color over sSpecularFactor (level) if both exist
    // we will output a message if both exist
    // TODO if we have many cases with both we should eventually combine them
    // ********************************************************************************

    // Specular factor texture ...
    bool asSpecularFactor = false;
    const FbxProperty lSpecularFactorProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sSpecularFactor);
    if (lSpecularFactorProperty.IsValid())
    {
       int lNbTex = lSpecularFactorProperty.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lSpecularFactorProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             asSpecularFactor = true;
             result.specularTexture = fbxTextureToOsgTexture(lTexture);
             result.specularChannel = lTexture->UVSet.Get();
             result.specularScaleU = lTexture->GetScaleU();
             result.specularScaleV = lTexture->GetScaleV();
             // Check if we need this 
             //result.extendedmaterial->setShininessFrontAndBack(shininess);
             //result.extendedmaterial->setSpecularFrontAndBack(specular);
             result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::SPECULAR_LAYER, lTexture->GetFileName());
          }
          //For now only allow 1 texture
          break;
       }
    }

    // Specular map...
    const FbxProperty lSpecularProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sSpecular);
    if (lSpecularProperty.IsValid())
    {
       int lNbTex = lSpecularProperty.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lSpecularProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             result.specularTexture = fbxTextureToOsgTexture(lTexture);
             result.specularChannel = lTexture->UVSet.Get();
             result.specularScaleU = lTexture->GetScaleU();
             result.specularScaleV = lTexture->GetScaleV();
             // Check if we need this (this was used in the FLT plugin)
             //result.extendedmaterial->setShininessFrontAndBack(shininess);
             //result.extendedmaterial->setSpecularFrontAndBack(specular);
             result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::SPECULAR_LAYER, lTexture->GetFileName());

             if (asSpecularFactor)
             {
                // If we hit that warning often we should eventually add the code to combine them
                OSG_WARN << "Specular texture and Specular Factor texture found. Only the specular texture will be used." << std::endl;
             }
          }
          //For now only allow 1 texture
          break;
       }
    }

    // VANTAGE specific implementation
    // PBR texture will overwrite normal textures if present

    // "3dsMax|main|base_color_map"  act as a diffuse texture
    const FbxProperty lProperty3dsMaxBase = pFbxMat->FindPropertyHierarchical("3dsMax|main|base_color_map");
    if (lProperty3dsMaxBase.IsValid())
    {
       int lNbTex = lProperty3dsMaxBase.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {        
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lProperty3dsMaxBase.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             // Check if we have a normal texture in FileName or RelativeFilename of the FBTexture class
             result.diffuseTexture = fbxTextureToOsgTexture(lTexture);
             if (result.diffuseTexture == NULL)
             {
                // We did not find the texture name check PBR material for texture
                std::string textureFileName;
                result.diffuseTexture = fbxPBRTextureToOsgTexture(lTexture, textureFileName);
                if (result.diffuseTexture == NULL)
                {
                   OSG_WARN << "Texture file not found for material '3dsMax|main|base_color_map'" << std::endl;
                }
             }
             result.diffuseChannel = lTexture->UVSet.Get();
             result.diffuseScaleU = lTexture->GetScaleU();
             result.diffuseScaleV = lTexture->GetScaleV();
          }
       }
    }

    // "3dsMax|main|metalness_map"
    std::string metalTexureFileName;
    const FbxProperty lProperty3dsMaxMetal= pFbxMat->FindPropertyHierarchical("3dsMax|main|metalness_map");
    if (lProperty3dsMaxMetal.IsValid())
    {
       int lNbTex = lProperty3dsMaxMetal.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lProperty3dsMaxMetal.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             result.metalTexture = fbxTextureToOsgTexture(lTexture);
             metalTexureFileName = lTexture->GetFileName();
             if (result.metalTexture == NULL)
             {
                // We did not find the texture name check PBR material for texture
                result.metalTexture = fbxPBRTextureToOsgTexture(lTexture, metalTexureFileName);
                if (result.metalTexture == NULL)
                {
                   OSG_WARN << "Texture file not found for material '3dsMax|main|metalness_map'" << std::endl;
                }
             }
             result.metalChannel = lTexture->UVSet.Get();
             result.metalScaleU = lTexture->GetScaleU();
             result.metalScaleV = lTexture->GetScaleV();

             // This is done in code below
             //result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::METAL_MAP_LAYER, metalTexureFileName); 
          }
       }
    }

    // "3dsMax|main|roughness_map" act as gloss (FbxSurfaceMaterial::sShininess)
    std::string roughnessTexureFileName;
    const FbxProperty lProperty3dsMaxRough = pFbxMat->FindPropertyHierarchical("3dsMax|main|roughness_map");
    if (lProperty3dsMaxRough.IsValid())
    {
       int lNbTex = lProperty3dsMaxRough.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lProperty3dsMaxRough.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             result.glossTexture = fbxTextureToOsgTexture(lTexture);
             roughnessTexureFileName = lTexture->GetFileName();
             if (result.glossTexture == NULL)
             {
                // We did not find the texture name check PBR material for texture
                result.glossTexture = fbxPBRTextureToOsgTexture(lTexture, roughnessTexureFileName);
                if (result.glossTexture == NULL)
                {
                   OSG_WARN << "Texture file not found for material '3dsMax|main|roughness_map'" << std::endl;
                }
             }
             result.glossChannel = lTexture->UVSet.Get();
             result.glossScaleU = lTexture->GetScaleU();
             result.glossScaleV = lTexture->GetScaleV();
             // This is done in code below
             // this map to "Specular Power" in Vantage GUI
             //result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::GLOSS_MAP_LAYER, roughnessTexureFileName); 
          }
       }
    }

    // "3dsMax|main|ao_map" act as FbxSurfaceMaterial::sAmbient
    std::string aoTexureFileName;
    const FbxProperty lProperty3dsMaxAo = pFbxMat->FindPropertyHierarchical("3dsMax|main|ao_map");
    if (lProperty3dsMaxAo.IsValid())
    {
       int lNbTex = lProperty3dsMaxAo.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lProperty3dsMaxAo.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             result.ambientTexture = fbxTextureToOsgTexture(lTexture);
             aoTexureFileName = lTexture->GetFileName();
             if (result.ambientTexture == NULL)
             {
                // We did not find the texture name check PBR material for texture
                result.ambientTexture = fbxPBRTextureToOsgTexture(lTexture, aoTexureFileName);
                if (result.ambientTexture == NULL)
                {
                   OSG_WARN << "Texture file not found for material '3dsMax|main|ao_map'" << std::endl;
                }
             }
             result.ambientChannel = lTexture->UVSet.Get();
             result.ambientScaleU = lTexture->GetScaleU();
             result.ambientScaleV = lTexture->GetScaleV();
             // This is done below
             //result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::AMBIENT_LAYER, aoTexureFileName); 
          }
       }
    }

    // Check PBR for composite material texture
    if ((!metalTexureFileName.empty() && !roughnessTexureFileName.empty() && !aoTexureFileName.empty()) &&
       (metalTexureFileName == roughnessTexureFileName && metalTexureFileName == aoTexureFileName))
    {
      // To have the composite material, we need the 3 filenames not to be empty and 
      // to point to the same file (and texture)
      result.compTexture = result.metalTexture;  
      // we can use one of the 3 channel and UV, should be the same
      result.compChannel = result.metalChannel;
      result.compScaleU = result.metalScaleU;
      result.compScaleV = result.metalScaleV;
      result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::METAL_GLOSS_AO_LAYER, metalTexureFileName);

       // Reset the other values 
       result.metalTexture = NULL;
       result.metalChannel = "";
       result.metalScaleU = 1.0;
       result.metalScaleV = 1.0;
       result.glossTexture = NULL;
       result.glossChannel = "";
       result.glossScaleU = 1.0;
       result.glossScaleV = 1.0;
       result.ambientTexture = NULL;
       result.ambientChannel = "";
       result.ambientScaleU = 1.0;
       result.ambientScaleV = 1.0;
    }
    else
    {
       // It's not a composite material assign the extended texture to each
       if (!metalTexureFileName.empty())
       {
          result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::METAL_MAP_LAYER, metalTexureFileName); 
       }
       if (!roughnessTexureFileName.empty())
       {
         result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::GLOSS_MAP_LAYER, roughnessTexureFileName); 
       }
       if (!aoTexureFileName.empty())
       {
          result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::AMBIENT_LAYER, aoTexureFileName); 
       }
    }

    // "3dsMax|main|norm_map" act as normal map
    const FbxProperty lProperty3dsMaxNormal = pFbxMat->FindPropertyHierarchical("3dsMax|main|norm_map");
    if (lProperty3dsMaxNormal.IsValid())
    {
       int lNbTex = lProperty3dsMaxNormal.GetSrcObjectCount<FbxFileTexture>();
       for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
       {
          FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lProperty3dsMaxNormal.GetSrcObject<FbxFileTexture>(lTextureIndex));
          if (lTexture)
          {
             result.normalTexture = fbxTextureToOsgTexture(lTexture);
             std::string texureFileName = lTexture->GetFileName();
             if (result.normalTexture == NULL)
             {
                // We did not find the texture name check PBR material for texture
                result.normalTexture = fbxPBRTextureToOsgTexture(lTexture, texureFileName);
                if (result.normalTexture == NULL)
                {
                   OSG_WARN << "Texture file not found for material '3dsMax|main|norm_map'" << std::endl;
                }
             }

             result.normalChannel = lTexture->UVSet.Get();
             result.normalScaleU = lTexture->GetScaleU();
             result.normalScaleV = lTexture->GetScaleV();

             result.extendedmaterial->setEffectTextureName(osg::ExtendedMaterial::NORMAL_MAP_LAYER, texureFileName);
          }
       }
    }

    if (pFbxLambert)
    {
        FbxDouble3 color = pFbxLambert->Diffuse.Get();
        double factor = pFbxLambert->DiffuseFactor.Get();
        double transparencyFactor = useTransparencyColorFactor ? transparencyColorFactor : pFbxLambert->TransparencyFactor.Get();
        pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(
            static_cast<float>(color[0] * factor),
            static_cast<float>(color[1] * factor),
            static_cast<float>(color[2] * factor),
            static_cast<float>(1.0 - transparencyFactor)));

        color = pFbxLambert->Ambient.Get();
        factor = pFbxLambert->AmbientFactor.Get();
        pOsgMat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(
            static_cast<float>(color[0] * factor),
            static_cast<float>(color[1] * factor),
            static_cast<float>(color[2] * factor),
            1.0f));

        color = pFbxLambert->Emissive.Get();
        factor = pFbxLambert->EmissiveFactor.Get();
        pOsgMat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(
            static_cast<float>(color[0] * factor),
            static_cast<float>(color[1] * factor),
            static_cast<float>(color[2] * factor),
            1.0f));

        
        // get maps factors...
        result.diffuseFactor = pFbxLambert->DiffuseFactor.Get();

        if (const FbxSurfacePhong* pFbxPhong = FbxCast<FbxSurfacePhong>(pFbxLambert))
        {
            color = pFbxPhong->Specular.Get();
            factor = pFbxPhong->SpecularFactor.Get();
            pOsgMat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4(
                static_cast<float>(color[0] * factor),
                static_cast<float>(color[1] * factor),
                static_cast<float>(color[2] * factor),
                1.0f));
            // Since Maya and 3D studio Max stores their glossiness values in exponential format (2^(log2(x)) 
            // We need to linearize to values between 0-100 and then scale to values between 0-128.
            // Glossiness values above 100 will result in shininess larger than 128.0 and will be clamped
            double shininess = (64.0 * log (pFbxPhong->Shininess.Get())) / (5.0 * log(2.0));
            pOsgMat->setShininess(osg::Material::FRONT_AND_BACK,
                static_cast<float>(shininess));

            // get maps factors...
            result.reflectionFactor = pFbxPhong->ReflectionFactor.Get();
            // get more factors here...
        }
    }

    if (_lightmapTextures)
    {
        // if using an emission map then adjust material properties accordingly...
        if (result.emissiveTexture)
        {
            osg::Vec4 diffuse = pOsgMat->getDiffuse(osg::Material::FRONT_AND_BACK);
            pOsgMat->setEmission(osg::Material::FRONT_AND_BACK, diffuse);
            pOsgMat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0,0,0,diffuse.a()));
            pOsgMat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0,0,0,diffuse.a()));
        }
    }

    _fbxMaterialMap.insert(FbxMaterialMap::value_type(pFbxMat, result));
    return result;
}

osg::ref_ptr<osg::Texture2D>
FbxMaterialToOsgStateSet::fbxPBRTextureToOsgTexture(const FbxFileTexture* fbx, std::string& texureFileName)
{
   osg::ref_ptr<osg::Image> pImage = NULL;
   const FbxProperty lSourceMapPtr = fbx->FindPropertyHierarchical("3dsMax|parameters|sourceMap");
   if (lSourceMapPtr.IsValid())
   {
      FbxFileTexture* lMapTexture = FbxCast<FbxFileTexture>(lSourceMapPtr.GetSrcObject<FbxFileTexture>());
      if (lMapTexture)
      {
         const FbxProperty lPropertyFilename = lMapTexture->FindPropertyHierarchical("3dsMax|OSLBitmap2|Filename");
         if (lPropertyFilename.IsValid())
         {
            FbxString fname = lPropertyFilename.Get<FbxString>();
            std::string fileName = fname.Buffer();
            std::string simpleFileName = fileName;
            texureFileName = simpleFileName;
            ImageMap::iterator it = _imageMap.find(simpleFileName);
            if (it != _imageMap.end())
               return it->second;

            bool found = osgDB::fileExists(fileName);
            // check for existence
            if (!found)
            {               
               std::string fileName3 = osgDB::concatPaths(_dir, osgDB::getSimpleFileName(fileName));
               
               if (osgDB::fileExists(fileName3))
               {
                  found = true;
                  fileName = fileName3;
               }
               if (!found)
               {
                  if (!osgDB::isAbsolutePath(fileName))
                  {
                     fileName3 = osgDB::concatPaths(_dir, fileName);
                     if (osgDB::fileExists(fileName3))
                     {
                        found = true;
                        fileName = fileName3;
                     }
                  }
                  // If not found search Vantage extra textures path
                  if (found == false)
                  {
                     found = findFileInTexturePath(fileName);
                  }
               }
            }

            if ((pImage = osgDB::readImageFile(fileName)) && found == true)
            {
               osg::ref_ptr<osg::Texture2D> pOsgTex = new osg::Texture2D;
               pOsgTex->setImage(pImage.get());
               pOsgTex->setWrap(osg::Texture2D::WRAP_S, convertWrap(fbx->GetWrapModeU()));
               pOsgTex->setWrap(osg::Texture2D::WRAP_T, convertWrap(fbx->GetWrapModeV()));
               _imageMap.insert(std::make_pair(simpleFileName, pOsgTex.get()));
               return pOsgTex;
            }
         }
      }
   }

   return NULL;
}

osg::ref_ptr<osg::Texture2D>
FbxMaterialToOsgStateSet::fbxTextureToOsgTexture(const FbxFileTexture* fbx)
{
   // check if the file exist and warn the user if not found
   std::string fileName = fbx->GetFileName();
   std::string fileName2 = fbx->GetRelativeFileName();
   if (fileName.empty() && fileName2.empty())
   {
      return NULL;
   }

   osg::ref_ptr<osg::Image> pImage = NULL;
   bool found = false;
   ImageMap::iterator it = _imageMap.find(fbx->GetFileName());
   if (it != _imageMap.end())
      return it->second;

    if (osgDB::fileExists(fileName))
    {
       found = true;
    }
    else
    {
       if (!osgDB::isAbsolutePath(fileName))
       {
          std::string fileName3 = osgDB::concatPaths(_dir, fileName);
          if (osgDB::fileExists(fileName3))
          {
             found = true;
             fileName = fileName3;
          }
       }
       if (!found)
       {
          std::string fileName3 = osgDB::concatPaths(_dir, fbx->GetRelativeFileName());
          if (osgDB::fileExists(fileName3))
          {
             found = true;
             fileName = fileName3;
          }
       }
       // If not found search Vantage extra textures path
       if (!found)
       {
          found = findFileInTexturePath(fileName);
       }
    }
    
    if ((pImage = osgDB::readImageFile(fileName)) && found == true)
    {
        osg::ref_ptr<osg::Texture2D> pOsgTex = new osg::Texture2D;
        pOsgTex->setImage(pImage.get());
        pOsgTex->setWrap(osg::Texture2D::WRAP_S, convertWrap(fbx->GetWrapModeU()));
        pOsgTex->setWrap(osg::Texture2D::WRAP_T, convertWrap(fbx->GetWrapModeV()));
        _imageMap.insert(std::make_pair(fbx->GetFileName(), pOsgTex.get()));
        return pOsgTex;
    }
    return NULL;
}

bool FbxMaterialToOsgStateSet::findFileInTexturePath(std::string& fileName)
{
   bool found = false;
   // If not found search Vantage extra textures path
   for (TexturePathList::const_iterator iter = _texturePathList.begin(); iter != _texturePathList.end(); ++iter)
   {
      boost::filesystem::path filepath;
      if (osgDB::isAbsolutePath(*iter))
      {
         filepath = *iter;
      }
      else
      {
         filepath = fbxUtil::resolve(boost::filesystem::path(*iter), boost::filesystem::path(_dir));
      }
      std::string fileName4 = osgDB::concatPaths(filepath.string(), osgDB::getSimpleFileName(fileName));
      if (osgDB::fileExists(fileName4))
      {
         found = true;
         fileName = fileName4;
         break;
      }
   }
   return found;
}

void FbxMaterialToOsgStateSet::checkInvertTransparency()
{
    int zeroAlpha = 0, oneAlpha = 0;
    for (FbxMaterialMap::const_iterator it = _fbxMaterialMap.begin(); it != _fbxMaterialMap.end(); ++it)
    {
        const osg::Material* pMaterial = it->second.material.get();
        float alpha = pMaterial->getDiffuse(osg::Material::FRONT).a();
        if (alpha > 0.999f)
        {
            ++oneAlpha;
        }
        else if (alpha < 0.001f)
        {
            ++zeroAlpha;
        }
    }

    if (zeroAlpha > oneAlpha)
    {
        //Transparency values seem to be back to front so invert them.

        for (FbxMaterialMap::const_iterator it = _fbxMaterialMap.begin(); it != _fbxMaterialMap.end(); ++it)
        {
            osg::Material* pMaterial = it->second.material.get();
            osg::Vec4 diffuse = pMaterial->getDiffuse(osg::Material::FRONT);
            diffuse.a() = 1.0f - diffuse.a();
            pMaterial->setDiffuse(osg::Material::FRONT_AND_BACK, diffuse);
        }
    }
}

bool FbxMaterialToOsgStateSet::CSVTexturePathFileRead = false; // Init once when the DLL is loaded
FbxMaterialToOsgStateSet::TexturePathList   FbxMaterialToOsgStateSet::_texturePathList; // Static only filled once
void FbxMaterialToOsgStateSet::readTexturePathCSVfile()
{
   if (CSVTexturePathFileRead == false)
   {
      std::string optionString = _options->getOptionString();
      std::size_t pos = optionString.find(vrvAppDataDir);
      if (pos != std::string::npos)
      {
         std::size_t endpos = optionString.find(" ", pos);
         if (pos == std::string::npos)
         {
            // no other param, use the end position for the length
            endpos = optionString.length();
         }
         pos = pos + vrvAppDataDir.length();
         std::string csvFile = optionString.substr(pos, endpos - pos);
         csvFile += "/importConfig/FBX_TexturePath.csv";

         FILE* file = fopen(csvFile.c_str(), "r");
         if (file != NULL)
         {
            char charline[256];
            bool firstLineRead = false;
            while (fgets(charline, sizeof(charline), file))
            {
               if (firstLineRead)
               {
                  std::string line = charline;
                  boost::char_separator<char> sep(",", "", boost::keep_empty_tokens);

                  boost::tokenizer<boost::char_separator<char> >
                     tokenList(line, sep);
                  boost::tokenizer<boost::char_separator<char> >::iterator curAttrIter =
                     tokenList.begin();
                  boost::tokenizer<boost::char_separator<char> >::iterator endAttrIter =
                     tokenList.end();

                  if (curAttrIter != endAttrIter)
                  {
                     // Read Texture Path
                     std::string texturePathStr = fbxUtil::removeReturn(*curAttrIter);
                     ++curAttrIter;

                     // insert in the list
                     _texturePathList.push_back(texturePathStr);
                  }
               }
               else
               {
                  firstLineRead = true;
               }
            }
         }
         fclose(file);
      }
      CSVTexturePathFileRead = true;
   }
}
