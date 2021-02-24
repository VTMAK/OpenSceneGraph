#ifndef FBXMATERIALTOOSGSTATESET_H
#define FBXMATERIALTOOSGSTATESET_H

#include <map>
#include <memory>
#include <osg/Material>
#include <osg/StateSet>
#include <osgDB/Options>
#include <osg/Texture2D>
#include <osg/TexEnv>
#include <osg/ExtendedMaterial>

#include <vector>
#include <string>

#if defined(_MSC_VER)
#pragma warning( disable : 4505 )
#pragma warning( default : 4996 )
#endif
#include <fbxsdk.h>

typedef std::map<int, std::string> multiLayerMap;

//The only things we need to create a new StateSet are texture and materials. So we store that in a pair.
//We Don't store directly in stateSet because getOrCreateStateSet function set some parameters.
const std::string kDIFFUSE_TEXTURE_UNIT = "DIFFUSE_TEXTURE_UNIT";
const std::string kOPACITY_TEXTURE_UNIT = "OPACITY_TEXTURE_UNIT";
const std::string kREFLECTION_TEXTURE_UNIT = "REFLECTION_TEXTURE_UNIT";
const std::string kEMISSIVE_TEXTURE_UNIT = "EMISSIVE_TEXTURE_UNIT";
const std::string kAMBIENT_TEXTURE_UNIT = "AMBIENT_TEXTURE_UNIT";
const std::string kNORMAL_TEXTURE_UNIT = "NORMAL_TEXTURE_UNIT";
const std::string kBUMP_TEXTURE_UNIT = "BUMP_TEXTURE_UNIT";
const std::string kSPECULAR_TEXTURE_UNIT = "SPECULAR_TEXTURE_UNIT";
const std::string kGLOSS_TEXTURE_UNIT = "GLOSS_TEXTURE_UNIT";
const std::string kMETAL_TEXTURE_UNIT = "METAL_TEXTURE_UNIT";
const std::string kCOMP_TEXTURE_UNIT = "METAL_GLOSS_AO_TEXTURE_UNIT";

struct StateSetContent
{
    StateSetContent()
        : diffuseFactor(1.0),
        reflectionFactor(1.0),
        emissiveFactor(1.0),
        ambientFactor(1.0),
        normalFactor(1.0),
        diffuseScaleU(1.0),
        diffuseScaleV(1.0),
        opacityScaleU(1.0),
        opacityScaleV(1.0),
        emissiveScaleU(1.0),
        emissiveScaleV(1.0),
        ambientScaleU(1.0),
        ambientScaleV(1.0),
        normalScaleU(1.0),
        normalScaleV(1.0),
        bumpScaleU(1.0),
        bumpScaleV(1.0),
        specularScaleU(1.0),
        specularScaleV(1.0),
        glossScaleU(1.0),
        glossScaleV(1.0),
        metalScaleU(1.0),
        metalScaleV(1.0)
    {
    }

    osg::ref_ptr<osg::Material> material;

    // MultiLayer texture unit map
    multiLayerMap diffuseLayerTextureMap;

    // textures objects...
    std::vector<osg::ref_ptr<osg::Texture2D> > diffuseLayerTexture;
    osg::ref_ptr<osg::Texture2D> diffuseTexture;
    osg::ref_ptr<osg::Texture2D> opacityTexture;
    osg::ref_ptr<osg::Texture2D> reflectionTexture;
    osg::ref_ptr<osg::Texture2D> emissiveTexture;
    osg::ref_ptr<osg::Texture2D> ambientTexture;
    // more textures types here...
    osg::ref_ptr<osg::Texture2D> normalTexture;
    osg::ref_ptr<osg::Texture2D> bumpTexture;
    osg::ref_ptr<osg::Texture2D> specularTexture;
    osg::ref_ptr<osg::Texture2D> glossTexture; // shininess 
    osg::ref_ptr<osg::Texture2D> metalTexture; // metalness 
    osg::ref_ptr<osg::Texture2D> compTexture; // metalness - gloss - ambient occlusion composite

    // textures maps channels names...
    std::vector<std::string> diffuseLayerChannel;
    std::string diffuseChannel;
    std::string opacityChannel;
    std::string reflectionChannel;
    std::string emissiveChannel;
    std::string ambientChannel;
    // more channels names here...
    std::string normalChannel;
    std::string bumpChannel;
    std::string specularChannel;
    std::string glossChannel;
    std::string metalChannel;
    std::string compChannel;

    // combining factors...
    double diffuseFactor;
    double reflectionFactor;
    double emissiveFactor;
    double ambientFactor;
    // more combining factors here...
    double normalFactor;

    std::vector<double> diffuseLayerScaleU;
    std::vector<double> diffuseLayerScaleV;
    double diffuseScaleU;
    double diffuseScaleV;
    double opacityScaleU;
    double opacityScaleV;
    double emissiveScaleU;
    double emissiveScaleV;
    double ambientScaleU;
    double ambientScaleV;
    double normalScaleU;
    double normalScaleV;
    double bumpScaleU;
    double bumpScaleV;
    double specularScaleU;
    double specularScaleV;
    double glossScaleU;
    double glossScaleV;
    double metalScaleU;
    double metalScaleV;
    double compScaleU;
    double compScaleV;

    std::vector<osg::ref_ptr<osg::TexEnv>> diffuseLayerEnv;

    osg::ref_ptr<osg::ExtendedMaterial> extendedmaterial;
};

//We use the pointers set by the importer to not duplicate materials and textures.
typedef std::map<const FbxSurfaceMaterial *, StateSetContent> FbxMaterialMap;

//This map is used to not load the same image more than 1 time.
typedef std::map<std::string, osg::Texture2D *> ImageMap;

class FbxMaterialToOsgStateSet
{
public:
    //Convert a FbxSurfaceMaterial to a osgMaterial and an osgTexture.
    StateSetContent convert(const FbxSurfaceMaterial* pFbxMat);

    //dir is the directory where fbx is stored (for relative path).
    FbxMaterialToOsgStateSet(const std::string& dir, const osgDB::Options* options, bool lightmapTextures) :
        _options(options),
        _dir(dir),
        _lightmapTextures(lightmapTextures)
    {
       readTexturePathCSVfile();
    }

    void checkInvertTransparency();

protected:

   //! Read the Vantage Texture path CSV 
   void readTexturePathCSVfile();

private:
    //Convert a texture fbx to an osg texture.
    osg::ref_ptr<osg::Texture2D> fbxTextureToOsgTexture(const FbxFileTexture* pOsgTex);
    osg::ref_ptr<osg::Texture2D> fbxPBRTextureToOsgTexture(const FbxFileTexture* pOsgTex, std::string& texureFileName);
    bool findFileInTexturePath(std::string& fileName);
    FbxMaterialMap       _fbxMaterialMap;
    ImageMap              _imageMap;
    const osgDB::Options* _options;
    const std::string     _dir;
    bool                  _lightmapTextures;

    //! Flag to know if the texture path file was read
    static bool CSVTexturePathFileRead;

    // additional texture path list pass by Vantage 
    typedef std::vector<std::string> TexturePathList;
    static TexturePathList       _texturePathList;

};

#endif //FBXMATERIALTOOSGSTATESET_H
