#include "fbxMaterialToOsgStateSet.h"
#include <sstream>
#include <osg/TexMat>
#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

TextureDetails::TextureDetails():
    factor(1.0),
    scale(1.0, 1.0)
{
}

bool TextureDetails::transparent() const
{
    const osg::Image* image = texture.valid() ? texture->getImage() : 0;
    return image!=0 ? image->isImageTranslucent() : false;
}

void TextureDetails::assignTextureIfRequired(osg::StateSet* stateSet, unsigned int unit)
{
    if (!texture) return;

    stateSet->setTextureAttributeAndModes(unit, texture.get());
}

void TextureDetails::assignTexMatIfRequired(osg::StateSet* stateSet, unsigned int unit)
{
    if (scale.x() != 1.0 || scale.y() != 1.0)
    {
        // set UV scaling...
        stateSet->setTextureAttributeAndModes(unit, new osg::TexMat(osg::Matrix::scale(scale.x(), scale.y(), 1.0)), osg::StateAttribute::ON);
    }
}
#include <osg/ExtendedMaterial>

static osg::Texture::WrapMode convertWrap(FbxFileTexture::EWrapMode wrap)
{
    return wrap == FbxFileTexture::eRepeat ?
        osg::Texture2D::REPEAT : osg::Texture2D::CLAMP_TO_EDGE;
}

StateSetContent FbxMaterialToOsgStateSet::convert(const FbxSurfaceMaterial* pFbxMat)
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
#if 0 // ALPHAPIXEL MERGE
    result.diffuse = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse));
#else
    const FbxProperty lProperty = pFbxMat->FindProperty(FbxSurfaceMaterial::sDiffuse);
    if (lProperty.IsValid())
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
#endif

    // opacity map...
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

        result.opacity = selectTextureDetails(lOpacityProperty);
    }

    // reflection map...
#if 0 // ALPHAPIXEL MERGE
    result.reflection = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sReflection));

    // emissive map...
    result.emissive = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sEmissive));

    // ambient map...
    result.ambient = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sAmbient));

    // normal map...
    result.normalMap = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sNormalMap));

    // specular map...
    result.specular = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sSpecular));

    // shininess map...
    result.shininess = selectTextureDetails(pFbxMat->FindProperty(FbxSurfaceMaterial::sShininess));
#else
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
#endif

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
        if (result.diffuse.valid()) result.diffuse->factor = pFbxLambert->DiffuseFactor.Get();
        if (result.emissive.valid()) result.emissive->factor = pFbxLambert->EmissiveFactor.Get();
        if (result.ambient.valid()) result.ambient->factor = pFbxLambert->AmbientFactor.Get();
        if (result.normalMap.valid()) result.normalMap->factor = pFbxLambert->BumpFactor.Get();

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
            if (result.reflection.valid()) result.reflection->factor = pFbxPhong->ReflectionFactor.Get();
            if (result.specular.valid()) result.specular->factor = pFbxPhong->SpecularFactor.Get();
            // get more factors here...
        }
    }

    if (_lightmapTextures)
    {
        // if using an emission map then adjust material properties accordingly...
        if (result.emissive.valid())
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

osg::ref_ptr<osg::Texture2D> FbxMaterialToOsgStateSet::fbxTextureToOsgTexture(const FbxFileTexture* fbx)
{
    // Try to find image in cache
	ImageMap::iterator it = _imageMap.find(fbx->GetFileName());
    if (it != _imageMap.end())
        return it->second;
#if 0 // ALPHAPIXEL MERGE   

	// Try to locate valid filename
	std::string filename = "";

	// Warning: fbx->GetRelativeFileName() is relative TO EXECUTION DIR
	//          fbx->GetFileName() is as stored initially in the FBX
	if (osgDB::fileExists(osgDB::concatPaths(_dir, fbx->GetFileName()))) // First try "export dir/name"
	{
		filename = osgDB::concatPaths(_dir, fbx->GetFileName());
	} 
	else if (osgDB::fileExists(fbx->GetFileName())) // Then try "name" (if absolute)
	{
		filename = fbx->GetFileName();
	} 
	else if (osgDB::fileExists(osgDB::concatPaths(_dir, fbx->GetRelativeFileName()))) // Else try  "current dir + relative filename"
	{
		filename = osgDB::concatPaths(_dir, fbx->GetRelativeFileName());
	} 
	else if (osgDB::fileExists(osgDB::concatPaths(_dir, osgDB::getSimpleFileName(fbx->GetFileName())))) // Else try "current dir + simple filename"
	{
		filename = osgDB::concatPaths(_dir, osgDB::getSimpleFileName(fbx->GetFileName()));
	}
	else 
	{
		OSG_WARN << "Could not find valid file for " << fbx->GetFileName() << std::endl;
		return NULL;
	}
	
	osg::ref_ptr<osg::Image> pImage = osgDB::readRefImageFile(filename, _options);
	if (pImage.valid())  
#else      
    osg::ref_ptr<osg::Image> pImage = NULL;

    // check if the file exist and warn the user if not found
    std::string fileName = fbx->GetFileName();
    std::string fileName2 = fbx->GetRelativeFileName();
    bool found = false;
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
    }
    
    if ((pImage = osgDB::readImageFile(fileName)) && found == true)
#endif
    {
        osg::ref_ptr<osg::Texture2D> pOsgTex = new osg::Texture2D;
        pOsgTex->setImage(pImage.get());
        pOsgTex->setWrap(osg::Texture2D::WRAP_S, convertWrap(fbx->GetWrapModeU()));
        pOsgTex->setWrap(osg::Texture2D::WRAP_T, convertWrap(fbx->GetWrapModeV()));
        _imageMap.insert(std::make_pair(fbx->GetFileName(), pOsgTex.get()));
        return pOsgTex;
    }
    else
    {
       if (fileName != fileName2)
       {
          OSG_WARN << "File not found [" << fileName << "] or [" << fileName2 << "]" << std::endl;
       }
       else
       {
          OSG_WARN << "File not found [" << fileName << "]" << std::endl;
       }
       return NULL;
    }
}

FbxFileTexture* FbxMaterialToOsgStateSet::selectFbxFileTexture(const FbxProperty& lProperty)
{
    if (lProperty.IsValid())
    {
        // check if layered textures are used...
        int layeredTextureCount = lProperty.GetSrcObjectCount<FbxLayeredTexture>();
        if (layeredTextureCount)
        {
            // find the first valud FileTexture
            for (int layeredTextureIndex = 0; layeredTextureIndex<layeredTextureCount; ++layeredTextureIndex)
            {
                FbxLayeredTexture* layered_texture = FbxCast<FbxLayeredTexture>(lProperty.GetSrcObject<FbxLayeredTexture>(layeredTextureIndex));
                int lNbTex = layered_texture->GetSrcObjectCount<FbxFileTexture>();
                for (int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
                {
                    FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(layered_texture->GetSrcObject<FbxFileTexture>(lTextureIndex));
                    if (lTexture) return lTexture;
                }
            }
        }
        else
        {
            // find the first valud FileTexture
            int lNbTex = lProperty.GetSrcObjectCount<FbxFileTexture>();
            for(int lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++)
            {
                FbxFileTexture* lTexture = FbxCast<FbxFileTexture>(lProperty.GetSrcObject<FbxFileTexture>(lTextureIndex));
                if (lTexture) return lTexture;
            }
        }
    }
    return 0;
}

TextureDetails* FbxMaterialToOsgStateSet::selectTextureDetails(const FbxProperty& lProperty)
{
    if (lProperty.IsValid())
    {
        FbxFileTexture* fileTexture = selectFbxFileTexture(lProperty);
        if (fileTexture)
        {
            TextureDetails* textureDetails = new TextureDetails();
            textureDetails->texture = fbxTextureToOsgTexture(fileTexture);
            textureDetails->channel = fileTexture->UVSet.Get();
            textureDetails->scale.set(fileTexture->GetScaleU(), fileTexture->GetScaleV());
            return textureDetails;
        }
    }
    return 0;
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
