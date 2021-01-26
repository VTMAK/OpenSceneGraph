#include <osg/LightSource>

#include <osgDB/ReadFile>

#if defined(_MSC_VER)
#pragma warning( disable : 4505 )
#pragma warning( default : 4996 )
#endif
#include <fbxsdk.h>
#include "fbxReader.h"
#include <osgSim/LightPointNode>
#include <osgSim/LightPoint>
#include "fbxUtil.h"

static const std::string disLightpoint = "@dis lightpoint";

void replaceAll(std::string& str, const std::string& from, const std::string& to) 
{
   if (from.empty())
      return;
   size_t start_pos = 0;
   while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
   }
}

osgDB::ReaderWriter::ReadResult OsgFbxReader::readFbxLightPoint(FbxNode* pNode, const std::string& lightType)
{
   // lightType string follow the CDB type used by Vantage and found in 
   // Lights_DefaultValues_3_2.csv   
   osgSim::LightPointNode* osgSimLightPointNode = new osgSim::LightPointNode();
   osgSim::LightPoint* osgSimLightPoint = new osgSim::LightPoint();
   osgSimLightPoint->_typeName = lightType;
   osgSimLightPoint->_sector = new osgSim::DirectionalSector(osg::Vec3(0,0,-1), 0, 0, 0);
   osgSimLightPoint->_position = osg::Vec3(0, 0, 0);

   osgSimLightPointNode->addLightPoint(*osgSimLightPoint);
   return osgDB::ReaderWriter::ReadResult(osgSimLightPointNode);
}

osgDB::ReaderWriter::ReadResult OsgFbxReader::readFbxDynamicLight(FbxNode* pNode, int& nLightCount)
{
   const FbxLight* fbxLight = FbxCast<FbxLight>(pNode->GetNodeAttribute());

   if (!fbxLight)
   {
      return osgDB::ReaderWriter::ReadResult::ERROR_IN_READING_FILE;
   }

   osg::Light* osgLight = new osg::Light;
   osgLight->setLightNum(nLightCount++);

   osg::LightSource* osgLightSource = new osg::LightSource;
   osgLightSource->setLight(osgLight);

   FbxLight::EType fbxLightType = fbxLight->LightType.IsValid() ?
      fbxLight->LightType.Get() : FbxLight::ePoint;

   osgLight->setPosition(osg::Vec4(0, 0, 0, fbxLightType != FbxLight::eDirectional));
   bool VantageSpotLight = false;
   double startFarAttenuation = 1.0;
   double endFarAttenuation = 1.0;
   if (fbxLight->EnableFarAttenuation.Get())
   {
      startFarAttenuation = fbxLight->FarAttenuationStart.Get();
      endFarAttenuation = fbxLight->FarAttenuationEnd.Get();
   }

   if (fbxLightType == FbxLight::eSpot)
   {
      if (fbxLight->EnableFarAttenuation.Get())
      {
         // Special case in Vantage when we want to use start/end far attenuation.
         // OSG linear and quadratic values are actually use to store start and end distances.
         // Therefore, the range is the end distance
         VantageSpotLight = true;
         osgLight->setConstantAttenuation(1.0f);
         osgLight->setLinearAttenuation(startFarAttenuation);
         osgLight->setQuadraticAttenuation(endFarAttenuation);

         // Vantage, this will be used directly DtOsgLightManager::registerLight()
         // so we send the outer angle directly in the SpotCutoff()
         // and the different in SpotExponent()         
         // Seem like the 3DMax is using 4 time the angles of Creator
         float outerAngle = fbxLight->OuterAngle.Get()/4.0; 
         float exponent = outerAngle - (fbxLight->InnerAngle.Get()/4.0);
         osgLight->setSpotCutoff(outerAngle);  
         osgLight->setSpotExponent(exponent);
      }
      else
      {
         double coneAngle = fbxLight->OuterAngle.Get();
         double hotSpot = fbxLight->InnerAngle.Get();
         const float MIN_HOTSPOT = 0.467532f;

         osgLight->setSpotCutoff(static_cast<float>(coneAngle));

         //Approximate the hotspot using the GL light exponent.
         // This formula maps a hotspot of 180 to exponent 0 (uniform light
         // distribution) and a hotspot of 45 to exponent 1 (effective light
         // intensity is attenuated by the cosine of the angle between the
         // direction of the light and the direction from the light to the vertex
         // being lighted). A hotspot close to 0 maps to exponent 128 (maximum).
         float exponent = (180.0f / (std::max)(static_cast<float>(hotSpot),
            MIN_HOTSPOT) - 1.0f) / 3.0f;
         osgLight->setSpotExponent(exponent);
      }
   }
   else if (fbxLightType == FbxLight::ePoint)
   {
      osgLight->setConstantAttenuation(1.0f);
      osgLight->setLinearAttenuation(startFarAttenuation);
      osgLight->setQuadraticAttenuation(endFarAttenuation);
   }

   if (VantageSpotLight == false && fbxLight->DecayType.IsValid() &&
      fbxLight->DecayStart.IsValid())
   {
      double fbxDecayStart = fbxLight->DecayStart.Get();
      
      switch (fbxLight->DecayType.Get())
      {
      case FbxLight::eNone:
         break;
      case FbxLight::eLinear:
         osgLight->setLinearAttenuation(fbxDecayStart);
         break;
      case FbxLight::eQuadratic:
      case FbxLight::eCubic:
         osgLight->setQuadraticAttenuation(fbxDecayStart);
         break;
      }
   }

   osg::Vec3f osgDiffuseSpecular(1.0f, 1.0f, 1.0f);
   osg::Vec3f osgAmbient(0.0f, 0.0f, 0.0f);
   if (fbxLight->Color.IsValid())
   {
      FbxDouble3 fbxColor = fbxLight->Color.Get();
      osgDiffuseSpecular.set(
         static_cast<float>(fbxColor[0]),
         static_cast<float>(fbxColor[1]),
         static_cast<float>(fbxColor[2]));
   }
   if (fbxLight->Intensity.IsValid())
   {
      osgDiffuseSpecular *= static_cast<float>(fbxLight->Intensity.Get()) * 0.06f; // This multiplier match Openflight Spot light in Vantage
   }
   if (fbxLight->ShadowColor.IsValid())
   {
      FbxDouble3 fbxShadowColor = fbxLight->ShadowColor.Get();
      osgAmbient.set(
         static_cast<float>(fbxShadowColor[0]),
         static_cast<float>(fbxShadowColor[1]),
         static_cast<float>(fbxShadowColor[2]));
   }

   osgLight->setDiffuse(osg::Vec4f(osgDiffuseSpecular, 1.0f));
   osgLight->setSpecular(osg::Vec4f(osgDiffuseSpecular, 1.0f));
   osgLight->setAmbient(osg::Vec4f(osgAmbient, 1.0f));

   return osgDB::ReaderWriter::ReadResult(osgLightSource);
}

// trim space in a string 
std::string trim(const std::string& str)
{
   size_t first = str.find_first_not_of(' ');
   if (std::string::npos == first)
   {
      return str;
   }
   size_t last = str.find_last_not_of(' ');
   return str.substr(first, (last - first + 1));
}

osgDB::ReaderWriter::ReadResult OsgFbxReader::readFbxLight(FbxNode* pNode, int& nLightCount)
{

    // get the comment on the light point
    // if "@dis lightPoint ..." is present it's a light point that is using the
    // CDB \ Vantage comment style light point
    // and not a FBX dynamic light
    FbxString pComment;
    fbxUtil::getCommentProperty(pNode, pComment);
    std::string strComment = fbxUtil::removeReturn(pComment);
    // replace forward slash with back slash (Maya removes the backslashes)
    // but the light point table is all back slashes
    std::replace(strComment.begin(), strComment.end(), '/', '\\');
    size_t lightPointPos = strComment.find(disLightpoint);
    if (lightPointPos != std::string::npos)
    {
       // CDB/Vantage style light point
       std::string lightType = strComment.substr(disLightpoint.length(), strComment.length() - disLightpoint.length());
       // trim spaces (pre/post)
       lightType = trim(lightType);
       return readFbxLightPoint(pNode, lightType);
    }
    else
    {
       // FBX dynamic light point
       return readFbxDynamicLight(pNode, nLightCount);
    }
}
