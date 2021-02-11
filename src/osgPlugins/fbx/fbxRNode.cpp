#include <cassert>
#include <memory>
#include <sstream>
#include <string>
#include <locale>

#include "ReaderWriterFBX.h"

#include <osg/io_utils>
#include <osg/Notify>
#include <osg/MatrixTransform>
#include <osg/Material>
#include <osg/Texture2D>
#include <osg/Switch>
#include <osg/LOD>
#include <osg/Sequence>
#include <osg/Geometry>
#include <osg/CullFace>

#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>

#include <osgAnimation/AnimationManagerBase>
#include <osgAnimation/Bone>
#include <osgAnimation/Skeleton>
#include <osgAnimation/StackedMatrixElement>
#include <osgAnimation/StackedQuaternionElement>
#include <osgAnimation/StackedRotateAxisElement>
#include <osgAnimation/StackedScaleElement>
#include <osgAnimation/StackedTranslateElement>
#include <osgAnimation/UpdateBone>
#include <osgAnimation/AnimationMatrixTransform>

#include <osgSim/DOFTransform>
#include <osgSim/LightPointNode>

#if defined(_MSC_VER)
    #pragma warning( disable : 4505 )
    #pragma warning( default : 4996 )
#endif
#include <fbxsdk.h>

#include "fbxReader.h"
#include "fbxUtil.h"

static const std::string disExternalRef = "@dis external_reference";
static const std::string disSwitch = "@dis switch";
static const std::string disState = "@dis state";
// ArticulatedPart
static const std::string disArticulatedPart = "@dis articulated_part";
static const std::string disArticulatedPartTranslateMin = "translate_min";
static const std::string disArticulatedPartTranslateCurrent = "translate_current";
static const std::string disArticulatedPartTranslateMax = "translate_max";
static const std::string disArticulatedPartTranslateStep = "translate_step";
static const std::string disArticulatedPartRotateMin = "rotate_min";
static const std::string disArticulatedPartRotateCurrent = "rotate_current";
static const std::string disArticulatedPartRotateMax = "rotate_max";
static const std::string disArticulatedPartRotateStep = "rotate_step";
static const std::string disArticulatedPartScaleMin = "scale_min";
static const std::string disArticulatedPartScaleCurrent = "scale_current";
static const std::string disArticulatedPartScaleMax = "scale_max";
static const std::string disArticulatedPartScaleStep = "scale_step";
// End ArticulatedPart

// Flipbook Animation
static const std::string disFlipbookAnimation = "@dis flipbook_animation";
static const std::string dislastFrameTime = "lastframetime";
static const std::string disloopCount = "loopcount";
static const std::string disloopDuration = "loopduration";
static const std::string disSwing = "swing";
static const std::string disFlipbookAnimationType = "animationtype";
static const std::string disFlipbookFrame = "@dis frame";
// end of Flipbook Animation

// Lod parameters
static const std::string disLod = "@dis lod";
static const std::string disSwitchIn = "switch_in";
static const std::string disSwitchOut = "switch_out";
static const std::string disTransition = "transition";
static const std::string disSIG = "significant_size";
static const std::string disLodCenter = "lod_center";
// end of Lod parameters

// COPIED FROM osgSim/DOFTransform.cpp
static const unsigned int TRANSLATION_X_LIMIT_BIT = 0x80000000u >> 0;
static const unsigned int TRANSLATION_Y_LIMIT_BIT = 0x80000000u >> 1;
static const unsigned int TRANSLATION_Z_LIMIT_BIT = 0x80000000u >> 2;
static const unsigned int ROTATION_PITCH_LIMIT_BIT = 0x80000000u >> 3;
static const unsigned int ROTATION_ROLL_LIMIT_BIT = 0x80000000u >> 4;
static const unsigned int ROTATION_YAW_LIMIT_BIT = 0x80000000u >> 5;
static const unsigned int SCALE_X_LIMIT_BIT = 0x80000000u >> 6;
static const unsigned int SCALE_Y_LIMIT_BIT = 0x80000000u >> 7;
static const unsigned int SCALE_Z_LIMIT_BIT = 0x80000000u >> 8;

static const std::string stateNodeName = "maingroup_";

bool isAnimated(FbxProperty& prop, FbxScene& fbxScene)
{
    for (int i = 0; i < fbxScene.GetSrcObjectCount<FbxAnimStack>(); ++i)
    {
        FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(fbxScene.GetSrcObject<FbxAnimStack>(i));

        const int nbAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
        for (int j = 0; j < nbAnimLayers; j++)
        {
            FbxAnimLayer* pAnimLayer = pAnimStack->GetMember<FbxAnimLayer>(j);
            if (prop.GetCurveNode(pAnimLayer, false))
            {
                return true;
            }
        }
    }
    return false;
}

bool isAnimated(FbxProperty& prop, const char* channel, FbxScene& fbxScene)
{
    for (int i = 0; i < fbxScene.GetSrcObjectCount<FbxAnimStack>(); ++i)
    {
        FbxAnimStack* pAnimStack = FbxCast<FbxAnimStack>(fbxScene.GetSrcObject<FbxAnimStack>(i));

        const int nbAnimLayers = pAnimStack->GetMemberCount<FbxAnimLayer>();
        for (int j = 0; j < nbAnimLayers; j++)
        {
            FbxAnimLayer* pAnimLayer = pAnimStack->GetMember<FbxAnimLayer>(j);
            if (prop.GetCurve(pAnimLayer, channel, false))
            {
                return true;
            }
        }
    }
    return false;
}

osg::Quat makeQuat(const FbxDouble3& degrees, EFbxRotationOrder fbxRotOrder)
{
    double radiansX = osg::DegreesToRadians(degrees[0]);
    double radiansY = osg::DegreesToRadians(degrees[1]);
    double radiansZ = osg::DegreesToRadians(degrees[2]);

    switch (fbxRotOrder)
    {
    case eEulerXYZ:
        return osg::Quat(
            radiansX, osg::Vec3d(1,0,0),
            radiansY, osg::Vec3d(0,1,0),
            radiansZ, osg::Vec3d(0,0,1));
    case eEulerXZY:
        return osg::Quat(
            radiansX, osg::Vec3d(1,0,0),
            radiansZ, osg::Vec3d(0,0,1),
            radiansY, osg::Vec3d(0,1,0));
    case eEulerYZX:
        return osg::Quat(
            radiansY, osg::Vec3d(0,1,0),
            radiansZ, osg::Vec3d(0,0,1),
            radiansX, osg::Vec3d(1,0,0));
    case eEulerYXZ:
        return osg::Quat(
            radiansY, osg::Vec3d(0,1,0),
            radiansX, osg::Vec3d(1,0,0),
            radiansZ, osg::Vec3d(0,0,1));
    case eEulerZXY:
        return osg::Quat(
            radiansZ, osg::Vec3d(0,0,1),
            radiansX, osg::Vec3d(1,0,0),
            radiansY, osg::Vec3d(0,1,0));
    case eEulerZYX:
        return osg::Quat(
            radiansZ, osg::Vec3d(0,0,1),
            radiansY, osg::Vec3d(0,1,0),
            radiansX, osg::Vec3d(1,0,0));
    case eSphericXYZ:
        {
            //I don't know what eSPHERIC_XYZ means, so this is a complete guess.
            osg::Quat quat;
            quat.makeRotate(osg::Vec3d(1.0, 0.0, 0.0), osg::Vec3d(degrees[0], degrees[1], degrees[2]));
            return quat;
        }
    default:
        OSG_WARN << "Invalid FBX rotation mode." << std::endl;
        return osg::Quat();
    }
}

void makeLocalMatrix(const FbxNode* pNode, osg::Matrix& m, const FbxString& appName)
{
    /*From http://area.autodesk.com/forum/autodesk-fbx/fbx-sdk/the-makeup-of-the-local-matrix-of-an-kfbxnode/

    Local Matrix = LclTranslation * RotationOffset * RotationPivot *
      PreRotation * LclRotation * PostRotation * RotationPivotInverse *
      ScalingOffset * ScalingPivot * LclScaling * ScalingPivotInverse

    LocalTranslation : translate (xform -query -translation)
    RotationOffset: translation compensates for the change in the rotate pivot point (xform -q -rotateTranslation)
    RotationPivot: current rotate pivot position (xform -q -rotatePivot)
    PreRotation : joint orientation(pre rotation)
    LocalRotation: rotate transform (xform -q -rotation & xform -q -rotateOrder)
    PostRotation : rotate axis (xform -q -rotateAxis)
    RotationPivotInverse: inverse of RotationPivot
    ScalingOffset: translation compensates for the change in the scale pivot point (xform -q -scaleTranslation)
    ScalingPivot: current scale pivot position (xform -q -scalePivot)
    LocalScaling: scale transform (xform -q -scale)
    ScalingPivotInverse: inverse of ScalingPivot
    */

    // When this flag is set to false, the RotationOrder, the Pre/Post rotation
    // values and the rotation limits should be ignored.
    bool rotationActive = pNode->RotationActive.Get();    
    EFbxRotationOrder fbxRotOrder = rotationActive ? pNode->RotationOrder.Get() : eEulerXYZ;
    
    FbxNodeAttribute::EType lAttributeType = FbxNodeAttribute::eUnknown;
    if (pNode->GetNodeAttribute())
    {
       lAttributeType = pNode->GetNodeAttribute()->GetAttributeType();
    }

    FbxDouble3 fbxLclPos = pNode->LclTranslation.Get();
    FbxDouble3 fbxRotOff = pNode->RotationOffset.Get();
    FbxDouble3 fbxRotPiv = pNode->RotationPivot.Get();
    FbxDouble3 fbxPreRot = pNode->PreRotation.Get();
    FbxDouble3 fbxLclRot = pNode->LclRotation.Get();
    FbxDouble3 fbxPostRot; // post rotation that we don't want to add in Vantage
    
    
    if ((lAttributeType == FbxNodeAttribute::eLight && appName == "Maya") ||
       (lAttributeType == FbxNodeAttribute::eLight && appName == "3ds Max"))
    {
       // Don't set the fbxPostRot for Maya and 3ds Max light 
    }
    else
    {
       fbxPostRot = pNode->PostRotation.Get();
    }
    FbxDouble3 fbxSclOff = pNode->ScalingOffset.Get();
    FbxDouble3 fbxSclPiv = pNode->ScalingPivot.Get();
    FbxDouble3 fbxLclScl = pNode->LclScaling.Get();

    m.makeTranslate(osg::Vec3d(
        fbxLclPos[0] + fbxRotOff[0] + fbxRotPiv[0],
        fbxLclPos[1] + fbxRotOff[1] + fbxRotPiv[1],
        fbxLclPos[2] + fbxRotOff[2] + fbxRotPiv[2]));
    if (rotationActive)
    {
        m.preMultRotate(
            makeQuat(fbxPostRot, eEulerXYZ) *
            makeQuat(fbxLclRot, fbxRotOrder) *
            makeQuat(fbxPreRot, eEulerXYZ));
    }
    else
    {
        m.preMultRotate(makeQuat(fbxLclRot, fbxRotOrder));
    }
    m.preMultTranslate(osg::Vec3d(
        fbxSclOff[0] + fbxSclPiv[0] - fbxRotPiv[0],
        fbxSclOff[1] + fbxSclPiv[1] - fbxRotPiv[1],
        fbxSclOff[2] + fbxSclPiv[2] - fbxRotPiv[2]));
    m.preMultScale(osg::Vec3d(fbxLclScl[0], fbxLclScl[1], fbxLclScl[2]));
    m.preMultTranslate(osg::Vec3d(
        -fbxSclPiv[0],
        -fbxSclPiv[1],
        -fbxSclPiv[2]));
}

void readTranslationElement(FbxPropertyT<FbxDouble3>& prop,
                            osgAnimation::UpdateMatrixTransform* pUpdate,
                            osg::Matrix& staticTransform,
                            FbxScene& fbxScene)
{
    FbxDouble3 fbxPropValue = prop.Get();
    osg::Vec3d val(
        fbxPropValue[0],
        fbxPropValue[1],
        fbxPropValue[2]);

    if (isAnimated(prop, fbxScene))
    {
        if (!staticTransform.isIdentity())
        {
            pUpdate->getStackedTransforms().push_back(new osgAnimation::StackedMatrixElement(staticTransform));
            staticTransform.makeIdentity();
        }
        pUpdate->getStackedTransforms().push_back(new osgAnimation::StackedTranslateElement("translate", val));
    }
    else
    {
        staticTransform.preMultTranslate(val);
    }
}

void getRotationOrder(EFbxRotationOrder fbxRotOrder, int order[/*3*/])
{
    switch (fbxRotOrder)
    {
    case eEulerXZY:
        order[0] = 0; order[1] = 2; order[2] = 1;
        break;
    case eEulerYZX:
        order[0] = 1; order[1] = 2; order[2] = 0;
        break;
    case eEulerYXZ:
        order[0] = 1; order[1] = 0; order[2] = 2;
        break;
    case eEulerZXY:
        order[0] = 2; order[1] = 0; order[2] = 1;
        break;
    case eEulerZYX:
        order[0] = 2; order[1] = 1; order[2] = 0;
        break;
    default:
        order[0] = 0; order[1] = 1; order[2] = 2;
    }
}

void readRotationElement(FbxPropertyT<FbxDouble3>& prop,
                         EFbxRotationOrder fbxRotOrder,
                         bool quatInterpolate,
                         osgAnimation::UpdateMatrixTransform* pUpdate,
                         osg::Matrix& staticTransform,
                         FbxScene& fbxScene)
{
    if (isAnimated(prop, fbxScene))
    {
        if (quatInterpolate)
        {
            if (!staticTransform.isIdentity())
            {
                pUpdate->getStackedTransforms().push_back(
                    new osgAnimation::StackedMatrixElement(staticTransform));
                staticTransform.makeIdentity();
            }
            pUpdate->getStackedTransforms().push_back(new osgAnimation::StackedQuaternionElement(
                "quaternion", makeQuat(prop.Get(), fbxRotOrder)));
        }
        else
        {
            const char* curveNames[3] = {FBXSDK_CURVENODE_COMPONENT_X, FBXSDK_CURVENODE_COMPONENT_Y, FBXSDK_CURVENODE_COMPONENT_Z};
            osg::Vec3 axes[3] = {osg::Vec3(1,0,0), osg::Vec3(0,1,0), osg::Vec3(0,0,1)};

            FbxDouble3 fbxPropValue = prop.Get();
            fbxPropValue[0] = osg::DegreesToRadians(fbxPropValue[0]);
            fbxPropValue[1] = osg::DegreesToRadians(fbxPropValue[1]);
            fbxPropValue[2] = osg::DegreesToRadians(fbxPropValue[2]);

            int order[3] = {0, 1, 2};
            getRotationOrder(fbxRotOrder, order);

            for (int i = 0; i < 3; ++i)
            {
                int j = order[2-i];
                if (isAnimated(prop, curveNames[j], fbxScene))
                {
                    if (!staticTransform.isIdentity())
                    {
                        pUpdate->getStackedTransforms().push_back(new osgAnimation::StackedMatrixElement(staticTransform));
                        staticTransform.makeIdentity();
                    }

                    pUpdate->getStackedTransforms().push_back(new osgAnimation::StackedRotateAxisElement(
                        std::string("rotate") + curveNames[j], axes[j], fbxPropValue[j]));
                }
                else
                {
                    staticTransform.preMultRotate(osg::Quat(fbxPropValue[j], axes[j]));
                }
            }
        }
    }
    else
    {
        staticTransform.preMultRotate(makeQuat(prop.Get(), fbxRotOrder));
    }
}

void readScaleElement(FbxPropertyT<FbxDouble3>& prop,
                      osgAnimation::UpdateMatrixTransform* pUpdate,
                      osg::Matrix& staticTransform,
                      FbxScene& fbxScene)
{
    FbxDouble3 fbxPropValue = prop.Get();
    osg::Vec3d val(
        fbxPropValue[0],
        fbxPropValue[1],
        fbxPropValue[2]);

    if (isAnimated(prop, fbxScene))
    {
        if (!staticTransform.isIdentity())
        {
            pUpdate->getStackedTransforms().push_back(new osgAnimation::StackedMatrixElement(staticTransform));
            staticTransform.makeIdentity();
        }
        pUpdate->getStackedTransforms().push_back(new osgAnimation::StackedScaleElement("scale", val));
    }
    else
    {
        staticTransform.preMultScale(val);
    }
}

// TODO 
// FIX from https://github.com/openscenegraph/OpenSceneGraph/commit/9bc93fb18e91c6dfd06b3b85ac62059c9c7c4e22
// NOTE check if this can be replace by FbxNode::EvaluateLocal/GlobalTransform() when using 
// FbxAxisSystem::Max.ConvertScene(lScene)
void readUpdateMatrixTransform(osgAnimation::UpdateMatrixTransform* pUpdate, FbxNode* pNode, FbxScene& fbxScene)
{
    osg::Matrix staticTransform;

    readTranslationElement(pNode->LclTranslation, pUpdate, staticTransform, fbxScene);

    FbxDouble3 fbxRotOffset = pNode->RotationOffset.Get();
    FbxDouble3 fbxRotPiv = pNode->RotationPivot.Get();
    staticTransform.preMultTranslate(osg::Vec3d(
        fbxRotPiv[0] + fbxRotOffset[0],
        fbxRotPiv[1] + fbxRotOffset[1],
        fbxRotPiv[2] + fbxRotOffset[2]));

    // When this flag is set to false, the RotationOrder, the Pre/Post rotation
    // values and the rotation limits should be ignored.
    bool rotationActive = pNode->RotationActive.Get();

    EFbxRotationOrder fbxRotOrder = (rotationActive && pNode->RotationOrder.IsValid()) ?
        pNode->RotationOrder.Get() : eEulerXYZ;

    if (rotationActive)
    {
       staticTransform.preMultRotate(makeQuat(pNode->PreRotation.Get(), eEulerXYZ));
    }

    readRotationElement(pNode->LclRotation, fbxRotOrder,
        pNode->QuaternionInterpolate.IsValid() && pNode->QuaternionInterpolate.Get(),
        pUpdate, staticTransform, fbxScene);

    if (rotationActive)
    {
       staticTransform.preMultRotate(makeQuat(pNode->PostRotation.Get(), eEulerXYZ));
    }

    FbxDouble3 fbxSclOffset = pNode->ScalingOffset.Get();
    FbxDouble3 fbxSclPiv = pNode->ScalingPivot.Get();
    staticTransform.preMultTranslate(osg::Vec3d(
        fbxSclOffset[0] + fbxSclPiv[0] - fbxRotPiv[0],
        fbxSclOffset[1] + fbxSclPiv[1] - fbxRotPiv[1],
        fbxSclOffset[2] + fbxSclPiv[2] - fbxRotPiv[2]));

    readScaleElement(pNode->LclScaling, pUpdate, staticTransform, fbxScene);

    staticTransform.preMultTranslate(osg::Vec3d(
        -fbxSclPiv[0],
        -fbxSclPiv[1],
        -fbxSclPiv[2]));

    if (!staticTransform.isIdentity())
    {
        pUpdate->getStackedTransforms().push_back(new osgAnimation::StackedMatrixElement(staticTransform));
    }
}

// Get the value in "@dis key value"
std::string getDisValue(const std::string& pComment, size_t endPos)
{
   std::string temp;
   size_t pos = pComment.find(' ', endPos);
   if (pos != std::string::npos)
   {
      temp = pComment.substr(endPos, pos - endPos);
   }
   else
   {
      temp = pComment.substr(endPos, pComment.length() - endPos);
   }
   return temp;
}

// Get the value in "x,y,z" of float format
void getVecValue(const std::string& pComment, size_t endPos, osg::Vec3f& vec3)
{
   std::string temp;
   size_t pos = pComment.find(' ', endPos);
   if (pos != std::string::npos)
   {
      temp = pComment.substr(endPos, pos - endPos);
   }
   else
   {
      temp = pComment.substr(endPos, pComment.length() - endPos);
   }
   float x = 0.0f;
   float y = 0.0f; 
   float z = 0.0f; 
   sscanf(temp.c_str(), "%f,%f,%f", &x, &y, &z);
   vec3.set(x, y, z);
}

// we read x,y,z (pitch, roll, yaw) like in FLT but write
// (yaw, pitch, roll) in the vector to pass to the DOF
void getVecValueFromDegree(const std::string& pComment, size_t endPos, osg::Vec3f& vec3)
{
   std::string temp;
   size_t pos = pComment.find(' ', endPos);
   if (pos != std::string::npos)
   {
      temp = pComment.substr(endPos, pos - endPos);
   }
   else
   {
      temp = pComment.substr(endPos, pComment.length() - endPos);
   }
   float x = 0.0f;
   float y = 0.0f;
   float z = 0.0f;
   sscanf(temp.c_str(), "%f,%f,%f", &x, &y, &z);  // read (pitch, roll, yaw)
   x = osg::inDegrees(x);
   y = osg::inDegrees(y);
   z = osg::inDegrees(z);
   vec3.set(z, x, y); // save (yaw, pitch, roll) 
}


// format of the comment: "animation num_frames_per_second [ skip | noskip ] loopCount count lastFrameTime time
//                         loopDuration time swing animationType [forward|backward]"
// ex.  "animation 2 skip loopCount 3 lastFrameTime 11.5"
void readFlipBookAnimationParameters(std::string pComment, int& nbFrame, bool& skip, int& loopCount, float& lastFrameTime,
   float& loopDuration, bool& swing, bool& forwardAnim)
{
   std::transform(pComment.begin(), pComment.end(), pComment.begin(), ::tolower);
   std::string temp;

   // loopDuration [time]
   size_t loopDurationPos = pComment.find(disloopDuration);
   loopDuration = 0.0f;
   if (loopDurationPos != std::string::npos)
   {
      // search for a space after loopDuration
      size_t endPos = loopDurationPos + disloopDuration.size() + 1; // pos after space
      temp = getDisValue(pComment, endPos);
      loopDuration = atof(temp.c_str());
   }

   // swing
   size_t swingPos = pComment.find(disSwing);
   swing = false;
   if (swingPos != std::string::npos)
   {
      swing = true;
   }

   // animationType
   size_t disanimationTypePos = pComment.find(disFlipbookAnimationType);
   forwardAnim = true;
   if (disanimationTypePos != std::string::npos)
   {
      // search for a space after disanimationType
      size_t endPos = disanimationTypePos + disFlipbookAnimationType.size() + 1; // pos after space
      temp = getDisValue(pComment, endPos);
      std::transform(temp.begin(), temp.end(), temp.begin(), ::tolower);
      if (temp.find("backward") != std::string::npos)
      {
         forwardAnim = false;
      }
   }

   // lastFrameTime [time]
   size_t lastFrameTimePos = pComment.find(dislastFrameTime);
   lastFrameTime = 0.0f;
   if (lastFrameTimePos != std::string::npos)
   {
      // search for a space after lastFrameTime
      size_t endPos = lastFrameTimePos+ dislastFrameTime.size() + 1; // pos after space
      temp = getDisValue(pComment, endPos);
      lastFrameTime = atof(temp.c_str());
   }

   // loopCount
   size_t loopCountPos = pComment.find(disloopCount);
   loopCount = -1;
   if (loopCountPos != std::string::npos)
   {
      size_t endPos = loopCountPos+ disloopCount.size() + 1; // pos after space
      temp = getDisValue(pComment, endPos);
      loopCount = atoi(temp.c_str());
   }

   // animation [num_frames_per_second]
   size_t animationPos = pComment.find(disFlipbookAnimationType);
   if (animationPos != std::string::npos)
   {
      // search for a space after num_frames_per_second
      size_t endPos = disFlipbookAnimationType.size() + 1; // pos after space
      temp = getDisValue(pComment, endPos);
      nbFrame = atoi(temp.c_str());

      skip = true;  // Use skip as default
      if (pComment.find("noskip") != std::string::npos)
      {
         skip = false;
      }
   }
}

// format of the comment: @dis articulated_part xxx translate_min x,y,z .... 
// NOTE all angle are in degrees
// ex.  @dis articulated_part 4096 
//           translate_min x,y,z translate_current x,y,z translate_max x,y,z translate_step x,y,z 
//           rotate_min x,y,z rotate_current x,y,z rotate_max x,y,z rotate_step x,y,z
//           scale_min x,y,z scale_current x,y,z scale_max x,y,z  scale_step x,y,z
unsigned long readArticulatedPartParameters(std::string pComment, int& partNumber, osg::Vec3f& translate_min, osg::Vec3f& translate_current,
   osg::Vec3f& translate_max, osg::Vec3f& translate_step, osg::Vec3f& rotate_min, osg::Vec3f& rotate_current,
   osg::Vec3f& rotate_max, osg::Vec3f& rotate_step, osg::Vec3f& scale_min, osg::Vec3f& scale_current,
   osg::Vec3f& scale_max, osg::Vec3f& scale_step)
{
   unsigned long limitationFlag = 0;
   std::transform(pComment.begin(), pComment.end(), pComment.begin(), ::tolower);
   std::string temp;

   // @dis articulated_part xxx
   size_t partPos = pComment.find(disArticulatedPart);
   partNumber = 0;
   if (partPos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = partPos + disArticulatedPart.size() + 1; // pos after space
      temp = getDisValue(pComment, endPos);
      partNumber = atoi(temp.c_str());
   }

   // translate_min x,y,z
   size_t Pos = pComment.find(disArticulatedPartTranslateMin);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartTranslateMin.size() + 1; // pos after space
      getVecValue(pComment, endPos, translate_min);
   }

   // translate_current x,y,z
   Pos = pComment.find(disArticulatedPartTranslateCurrent);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartTranslateCurrent.size() + 1; // pos after space
      getVecValue(pComment, endPos, translate_current);
   }

   // translate_max x,y,z
   Pos = pComment.find(disArticulatedPartTranslateMax);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartTranslateMax.size() + 1; // pos after space
      getVecValue(pComment, endPos, translate_max);
   }

   // translate_step x,y,z
   Pos = pComment.find(disArticulatedPartTranslateStep);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartTranslateStep.size() + 1; // pos after space
      getVecValue(pComment, endPos, translate_step);
   }

   // rotate_min 
   // we read x,y,z (pitch, roll, yaw) like in FLT but write
   // (yaw, pitch, roll) in the vector
   Pos = pComment.find(disArticulatedPartRotateMin);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartRotateMin.size() + 1; // pos after space
      getVecValueFromDegree(pComment, endPos, rotate_min);
   }

   // rotate_current 
   // we read x,y,z (pitch, roll, yaw) like in FLT but write
   // (yaw, pitch, roll) in the vector
   Pos = pComment.find(disArticulatedPartRotateCurrent);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartRotateCurrent.size() + 1; // pos after space
      getVecValueFromDegree(pComment, endPos, rotate_current);
   }
   
   // rotate_max 
   // we read x,y,z (pitch, roll, yaw) like in FLT but write
   // (yaw, pitch, roll) in the vector
   Pos = pComment.find(disArticulatedPartRotateMax);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartRotateMax.size() + 1; // pos after space
      getVecValueFromDegree(pComment, endPos, rotate_max);
   }

   // rotate_step 
   // we read x,y,z (pitch, roll, yaw) like in FLT but write
   // (yaw, pitch, roll) in the vector
   Pos = pComment.find(disArticulatedPartRotateStep);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartRotateStep.size() + 1; // pos after space
      getVecValueFromDegree(pComment, endPos, rotate_step);
   }

   // scale_min x,y,z
   Pos = pComment.find(disArticulatedPartScaleMin);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartScaleMin.size() + 1; // pos after space
      getVecValue(pComment, endPos, scale_min);
   }

   // scale_current x,y,z
   Pos = pComment.find(disArticulatedPartScaleCurrent);
   // Init current scale to 1.0
   scale_current.set(1.0, 1.0, 1.0);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartScaleCurrent.size() + 1; // pos after space
      getVecValue(pComment, endPos, scale_current);
   }

   // scale_max x,y,z
   Pos = pComment.find(disArticulatedPartScaleMax);
   // Init max scale to 1.0
   scale_max.set(1.0, 1.0, 1.0);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartScaleMax.size() + 1; // pos after space
      getVecValue(pComment, endPos, scale_max);
   }

   // scale_step x,y,z
   Pos = pComment.find(disArticulatedPartScaleStep);
   if (Pos != std::string::npos)
   {
      // search for a space after 
      size_t endPos = Pos + disArticulatedPartScaleStep.size() + 1; // pos after space
      getVecValue(pComment, endPos, scale_step);
   }

   // Set the constraint flag (

   // Translation 
   if (!osg::equivalent(translate_max.x() - translate_min.x(), 0.0f))  limitationFlag |= TRANSLATION_X_LIMIT_BIT;
   if (!osg::equivalent(translate_max.y() - translate_min.y(), 0.0f))  limitationFlag |= TRANSLATION_Y_LIMIT_BIT;
   if (!osg::equivalent(translate_max.z() - translate_min.z(), 0.0f))  limitationFlag |= TRANSLATION_Z_LIMIT_BIT;


   // Rotation
   // Note at that point x=yaw, y=pitch, z=roll
   if (!osg::equivalent(rotate_max.x() - rotate_min.x(), 0.0f))  limitationFlag |= ROTATION_YAW_LIMIT_BIT;
   if (!osg::equivalent(rotate_max.y() - rotate_min.y(), 0.0f))  limitationFlag |= ROTATION_PITCH_LIMIT_BIT;
   if (!osg::equivalent(rotate_max.z() - rotate_min.z(), 0.0f))  limitationFlag |= ROTATION_ROLL_LIMIT_BIT;

   // Scale
   if (!osg::equivalent(scale_max.x() - scale_min.x(), 0.0f))  limitationFlag |= SCALE_X_LIMIT_BIT;
   if (!osg::equivalent(scale_max.y() - scale_min.y(), 0.0f))  limitationFlag |= SCALE_Y_LIMIT_BIT;
   if (!osg::equivalent(scale_max.z() - scale_min.z(), 0.0f))  limitationFlag |= SCALE_Z_LIMIT_BIT;

   return limitationFlag;
}

bool containStateName(FbxNode* pNode, const std::map<std::string, std::string>& stateNodeMap)
{
   // Search for "maingroup_xxx" in the name to see if we need to add the state (normal/destroy)
   std::string nodeName = pNode->GetName();
   if (nodeName.find(stateNodeName) != std::string::npos)
   {
      return true;
   }
   // loop on all normal/destroy state node name from the CVS file
   for (std::map<std::string, std::string>::const_iterator it = stateNodeMap.begin(); it != stateNodeMap.end(); ++it)
   {
      // look for NORMAL state node name
      size_t pos = nodeName.find(it->first);
      if (pos != std::string::npos)
      {
         return true;
      }
      // look for DESTROY state node name
      pos = nodeName.find(it->second);
      if (pos != std::string::npos)
      {
         return true;
      }
   }
   return false;
}

osg::Group* createGroupNode(FbxManager& pSdkManager, FbxNode* pNode,
    const std::string& animName, osgAnimation::Animation* animation, const osg::Matrix& localMatrix, bool bNeedSkeleton,
    std::map<FbxNode*, osg::Node*>& nodeMap, FbxScene& fbxScene, osg::NodeList& children, bool& hasDof,
    const std::map<std::string, std::string>& stateNodeMap)
{
    FbxString pComment;
    fbxUtil::getCommentProperty(pNode, pComment);
    // Search for "maingroup_xxx" in the name to see if we need to add the state (normal/destroy)
    bool foundStateName = containStateName(pNode, stateNodeMap);

    if (bNeedSkeleton)
    {
        return addBone(pNode, animName, fbxScene, nodeMap);
    }
    else if (pComment.Find(disSwitch.c_str()) != -1 && pComment.Find(disSwitchIn.c_str()) == -1 && pComment.Find(disSwitchOut.c_str()) == -1)
    {
       return addSwitch(pNode, pComment);
    }
    else if (pComment.Find(disState.c_str()) != -1 || foundStateName)
    {
       return addState(pNode, pComment, foundStateName, stateNodeMap);
    }    
    else if (pComment.Find(disArticulatedPart.c_str()) != -1 )
    {
       return addArticulatedPart(pNode, pComment, localMatrix, hasDof, fbxScene);
    }
    else if (pComment.Find(disFlipbookAnimation.c_str()) != -1)
    {
       return addFlipBookAnimation(pNode, pComment, children);
    }
    else
    {
        bool bAnimated = !animName.empty();
        if (!bAnimated && localMatrix.isIdentity())
        {
           return addGroup(pNode, pComment);
        }
        return addTransform(pNode, pComment, localMatrix, fbxScene, animName, animation, bAnimated);
    }
}

osgDB::ReaderWriter::ReadResult OsgFbxReader::readFbxNode(
    FbxNode* pNode,
    bool& bIsBone, int& nLightCount,
    textureUnitMap& textureMap,
    const FbxString& appName)
{

   // Add comment from Node Name if needed
   addCommentFromNodeName(pNode);

   if (FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute())
    {
        FbxNodeAttribute::EType attrType = lNodeAttribute->GetAttributeType();
        switch (attrType)
        {
        case FbxNodeAttribute::eNurbs:
        case FbxNodeAttribute::ePatch:
        case FbxNodeAttribute::eNurbsCurve:
        case FbxNodeAttribute::eNurbsSurface:
            {
                FbxGeometryConverter lConverter(&pSdkManager);
#if FBXSDK_VERSION_MAJOR < 2014
                if (!lConverter.TriangulateInPlace(pNode))
#else
                if (!lConverter.Triangulate(lNodeAttribute,true,false))
#endif
                {
                    OSG_WARN << "Unable to triangulate FBX NURBS " << pNode->GetName() << std::endl;
                }
            }
            break;
        default:
            break;
        }
    }

    bIsBone = false;
    bool bCreateSkeleton = false;

    FbxNodeAttribute::EType lAttributeType = FbxNodeAttribute::eUnknown;
    if (pNode->GetNodeAttribute())
    {
        lAttributeType = pNode->GetNodeAttribute()->GetAttributeType();
        if (lAttributeType == FbxNodeAttribute::eSkeleton)
        {
            bIsBone = true;
        }
    }

    if (!bIsBone && fbxSkeletons.find(pNode) != fbxSkeletons.end())
    {
        bIsBone = true;
    }

    unsigned nMaterials = pNode->GetMaterialCount();
    std::vector<StateSetContent > stateSetList;

    for (unsigned i = 0; i < nMaterials; ++i)
    {
        FbxSurfaceMaterial* fbxMaterial = pNode->GetMaterial(i);
        assert(fbxMaterial);
        stateSetList.push_back(fbxMaterialToOsgStateSet.convert(fbxMaterial));
    }

    osg::NodeList skeletal, children;
    int nChildCount = pNode->GetChildCount();
    for (int i = 0; i < nChildCount; ++i)
    {
        FbxNode* pChildNode = pNode->GetChild(i);
        if (pChildNode->GetParent() != pNode)
        {
            //workaround for bug that occurs in some files exported from Blender
            continue;
        }

        bool bChildIsBone = false;
        osgDB::ReaderWriter::ReadResult childResult = readFbxNode(
            pChildNode, bChildIsBone, nLightCount, textureMap, appName);
        if (childResult.error())
        {
            return childResult;
        }
        else if (osg::Node* osgChild = childResult.getNode())
        {
            if (bChildIsBone)
            {
                if (!bIsBone) bCreateSkeleton = true;
                skeletal.push_back(osgChild);
            }
            else
            {
               if (addDamageSwitch(osgChild, children) == false)
               {
                  children.push_back(osgChild);
               }
            }
        }
    }

    // IMPORTANT NOTE : for now we are disabling reading the animation key frame
    // This cause a problem in VRF (defect 67974 "Placement of entities near another FBX entity"
    osgAnimation::Animation* animation = NULL;
    std::string animName;
    // NOTE animName = the target name of the animation
    //std::string animName = readFbxAnimation(pNode, pNode->GetName(), animation); 

    osg::Matrix localMatrix;
    makeLocalMatrix(pNode, localMatrix, appName);
    bool bLocalMatrixIdentity = localMatrix.isIdentity();
    // NOTE in theory we should be able to use the call below instead 
    // of doing makeLocalMatrix. This would be in conjunction with 
    // FbxAxisSystem::Max.ConvertScene(pScene); but it does not seem to work
    //FbxAMatrix lGlobal, lLocal;
    //lGlobal = pNode->EvaluateGlobalTransform();
    //lLocal = pNode->EvaluateLocalTransform();

    osg::ref_ptr<osg::Group> osgGroup;

    bool bEmpty = children.empty() && !bIsBone;
    std::string debugTypeString;
    FbxString pComment;
    fbxUtil::getCommentProperty(pNode, pComment);
    switch (lAttributeType)
    {
    case FbxNodeAttribute::eNull:
       {
         // The vantage comment for this type of nodes are read in 
         // createGroupNode()
          debugTypeString = "eNull";
          if (pNode->GetNodeAttribute())
          {
             // If we find an external reference command load the other FBX model
             if (pComment.Find(disExternalRef.c_str()) != -1)
             {
                bool bAnimated = !animName.empty();
                osg::ref_ptr<osg::MatrixTransform> mt = addTransform(pNode, pComment, localMatrix, fbxScene, animName, animation, bAnimated);
                osgDB::ReaderWriter::ReadResult Xref = addExternalReference(pComment, localMatrix, options, currentFilePath);
                mt->addChild(Xref.getNode());
                return osgDB::ReaderWriter::ReadResult(mt.get());
             }
          }
       }
       break;
    case FbxNodeAttribute::eMesh:
        {
            debugTypeString = "eMesh";
            size_t bindMatrixCount = boneBindMatrices.size();
            osgDB::ReaderWriter::ReadResult meshRes = readFbxMesh(pNode, stateSetList, textureMap);
            if (meshRes.error())
            {
                return meshRes;
            }
            else if (osg::Node* node = meshRes.getNode())
            {
                bEmpty = false;

                if (bindMatrixCount != boneBindMatrices.size())
                {
                    //The mesh is skinned therefore the bind matrix will handle all transformations.
                    localMatrix.makeIdentity();
                    bLocalMatrixIdentity = true;
                }

                if (animName.empty() &&
                    children.empty() &&
                    skeletal.empty() &&
                    (pComment.Find("@dis") == -1) &&    // If we find a Mak comment we should not return here
                    bLocalMatrixIdentity)
                {
                    return osgDB::ReaderWriter::ReadResult(node);
                }
                children.insert(children.begin(), node);
            }
        }
        break;
    case FbxNodeAttribute::eCamera:
    case FbxNodeAttribute::eLight:
        {
            debugTypeString = "eLight";
            osgDB::ReaderWriter::ReadResult res =
                lAttributeType == FbxNodeAttribute::eCamera ?
                readFbxCamera(pNode) : readFbxLight(pNode, nLightCount);
            if (res.error())
            {
                return res;
            }
            else if (osg::Group* resGroup = dynamic_cast<osg::Group*>(res.getObject()))
            {
                bEmpty = false;
                if (animName.empty() &&
                    bLocalMatrixIdentity)
                {
                    osgGroup = resGroup;
                }
                else
                {
                    children.insert(children.begin(), resGroup);
                }
            }
            else if (osgSim::LightPointNode* lpn = dynamic_cast<osgSim::LightPointNode*>(res.getObject()))
            {
               bEmpty = false;
               children.insert(children.begin(), lpn);
            }
        }
        break;
    default:
        debugTypeString = "Other";
        break;
    }
    
    bool hasDof = false;
    if (!osgGroup)
    {
       osgGroup = createGroupNode(pSdkManager, pNode, animName, animation, localMatrix, 
          bIsBone, nodeMap, fbxScene, children, hasDof, _nodeNameStateMap);
    }
    osg::Group* pAddChildrenTo = osgGroup.get();
    if (hasDof)
    {
       // if we had a DOF add the children to the DOF instead
       pAddChildrenTo = (osg::Group*)osgGroup.get()->getChild(0);
    }

    if (bCreateSkeleton)
    {
        osgAnimation::Skeleton* osgSkeleton = getSkeleton(pNode, fbxSkeletons, skeletonMap);
        osgSkeleton->setDefaultUpdateCallback();
        pAddChildrenTo->addChild(osgSkeleton);
        pAddChildrenTo = osgSkeleton;
    }

    for (osg::NodeList::iterator it = skeletal.begin(); it != skeletal.end(); ++it)
    {
        pAddChildrenTo->addChild(it->get());
    }

    // Follow unity naming convention standard for FBX 
    // https://docs.unity3d.com/Manual/LevelOfDetail.html
    // If we find a group with name that start with LODx put it 
    // under a LOD node
    //    Group  
    //     Lod
    //  --------------
    //  LOD0   LOD1 ...
    //
    bool addLod = false;
    int childId = 0;
    osg::ref_ptr<osg::LOD> pLod;

    // Add all the children under a Group/Lod/Switch/Dof
    for (osg::NodeList::iterator it = children.begin(); it != children.end(); ++it)
    {
       // Get the group name 
       osg::Node* childNode = it->get();
       std::string nodeName = childNode->getName();
       std::string nodeComment;
       if (childNode->getNumDescriptions() > 0)
       {
          nodeComment = childNode->getDescription(0);
       }
       
       // check if it finish with "_LODx" or comment contain "@dis lod"
       // we also need to check if it start with "LODx"
       if (returnEndNodeName(nodeName) == "_LOD" || nodeComment.find(disLod.c_str()) != std::string::npos ||
          nodeName.substr(0,3) == "LOD")
       {
          readLodInformation(childNode, pAddChildrenTo, nodeName, nodeComment, addLod, childId, pLod);
       }
       else if (nodeComment.find(disFlipbookFrame.c_str()) != std::string::npos)
       {
          addFlipbookAnimationFrame(it, pAddChildrenTo, pComment);
       }
       else
       {
          //std::string name = childNode->getName();
          //std::cout << name << ":" << localMatrix(3, 0) << "," << localMatrix(3, 1) << "," << localMatrix(3, 2) << std::endl;
          pAddChildrenTo->addChild(childNode);
       }
    }

    return osgDB::ReaderWriter::ReadResult(osgGroup.get());
}

osgAnimation::Skeleton* getSkeleton(FbxNode* fbxNode,
    const std::set<const FbxNode*>& fbxSkeletons,
    std::map<FbxNode*, osgAnimation::Skeleton*>& skeletonMap)
{
    //Find the first non-skeleton ancestor of the node.
    while (fbxNode &&
        ((fbxNode->GetNodeAttribute() &&
        fbxNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) ||
        fbxSkeletons.find(fbxNode) != fbxSkeletons.end()))
    {
        fbxNode = fbxNode->GetParent();
    }

    std::map<FbxNode*, osgAnimation::Skeleton*>::const_iterator it = skeletonMap.find(fbxNode);
    if (it == skeletonMap.end())
    {
        osgAnimation::Skeleton* skel = new osgAnimation::Skeleton;
        skel->setDefaultUpdateCallback();
        skeletonMap.insert(std::pair<FbxNode*, osgAnimation::Skeleton*>(fbxNode, skel));
        return skel;
    }
    else
    {
        return it->second;
    }
}

void OsgFbxReader::addFlipbookAnimationFrame(osg::NodeList::iterator& itNodeList,
   osg::Group* pGroup, const FbxString& pComment)
{
   bool inserted = false;
   
   // get the dis frame number of the current child we are looping on
   unsigned int nbDesc = itNodeList->get()->getNumDescriptions();
   unsigned int frameNum = 0;
   if (nbDesc)
   {
      std::string childComment = itNodeList->get()->getDescription(0);
      // jump after "@dis frame " to read the frame number xx
      frameNum = atoi(childComment.c_str() + disFlipbookFrame.size() + 1);
   }
   else
   {
      OSG_WARN << "Node under the @dis animation group is missing @dis frame xx comment." << std::endl;
   }

   // Loop on the animation child to know where to insert it
   // because in the FBX the order is not guarantied
   for (unsigned int i = 0; i < pGroup->getNumChildren(); i++)
   {
      osg::Node* childNode = pGroup->getChild(i);
      nbDesc = childNode->getNumDescriptions();
      if (nbDesc > 0)
      {
         unsigned int ChildframeNum = atoi(childNode->getDescription(0).c_str() + disFlipbookFrame.size() + 1);
         if (frameNum < ChildframeNum)
         {
            pGroup->insertChild(i, itNodeList->get());
            inserted = true;
            break;
         }
      }
   }

   if (!inserted)
   {
      pGroup->addChild(itNodeList->get());
   }
}

void OsgFbxReader::readLodInformation(osg::Node* childNode, osg::Group* pGroup, 
   const std::string& pNodeName,const std::string& nodeComment,
   bool& pAddLod, int& pChildId, osg::ref_ptr<osg::LOD>& pLod)
{
   int lod;
   if (pAddLod == false)
   {
      // if it's the first we hit, create a LOD node
      pLod = new osg::LOD;
      pAddLod = true;
      pGroup->addChild(pLod);
   }

   // Check if we have Vantage comment on the node
   // lod / switch_in / switch_out / transition / significant_size / lod_center
   int cLod = -1;
   float switch_in = -1.0f;
   float switch_out = -1.0f;
   float transition = -1.0f; // TODO add to OSG if needed
   float significant_size = -1.0f; // TODO add to OSG (for CDB also)
   osg::BoundingSphere::vec_type lod_center;   

   // Get default values
   fbxUtil::minMaxSwitchDistance(pNodeName, lod, switch_out, switch_in);
   bool hasLodCenter = findLodComments(nodeComment, cLod, switch_in, switch_out, transition, significant_size, lod_center);
   if (lod != cLod)
   {
      // Use default values and node name for _LODx
      cLod = lod;
   }
   if (cLod >= 0)
   {
       // We found values in the comment
      pLod->addChild(childNode, switch_out, switch_in);
      pChildId++;
   }
   else
   {
      OSG_WARN << "LOD child not added, could not find the value LOD index" << std::endl;
   }

   if (hasLodCenter && pLod.get()!=NULL)
   {
      pLod->setCenter(lod_center);
   }
}

//! example
//! @dis lod 1
//! switch_in 50
//! switch_out 0
//! transition 0
//! significant_size 0
//! lod_center 0, 0, 0
bool OsgFbxReader::findLodComments(const std::string& pComment, int& cLod,
   float& switch_in, float& switch_out, float& transition,
   float& significant_size, osg::BoundingSphere::vec_type& lod_center)
{
   bool hasLodCenter = false;
   // Check if the @dis lod is present (mandatory for the others)
   int pos = pComment.find(disLod.c_str());
   if (pos != std::string::npos)
   {
      std::string strcomment = pComment;
      // Check for the "@dis lod"
      int offset = pos + disLod.length() + 1;
      size_t nexttoken = getTokenPosition(strcomment, offset);
      std::string tempValue = strcomment.substr(offset, nexttoken - offset);
      cLod = atoi(tempValue.c_str());

      // check for the "switch_in"
      pos = pComment.find(disSwitchIn.c_str());
      if (pos != std::string::npos)
      {
         offset = pos + disSwitchIn.length() + 1;
         nexttoken = getTokenPosition(strcomment, offset);
         tempValue = strcomment.substr(offset, nexttoken - offset);
         switch_in = atof(tempValue.c_str());
      }

      // check for the "switch_out"
      pos = pComment.find(disSwitchOut.c_str());
      if (pos != std::string::npos)
      {
         offset = pos + disSwitchOut.length() + 1;
         nexttoken = getTokenPosition(strcomment, offset);
         tempValue = strcomment.substr(offset, nexttoken - offset);
         switch_out = atof(tempValue.c_str());
      }
      if ((switch_in > -1.0f && switch_out < 0.0f) || (switch_in < 0.0f && switch_out > -1.0f))
      {
         OSG_WARN << "@dis switch_in or @dis switch_out value is missing, both are needed." << std::endl;
      }

      // check for the "transition"
      pos = pComment.find(disTransition.c_str());
      if (pos != std::string::npos)
      {
         offset = pos + disTransition.length() + 1;
         nexttoken = getTokenPosition(strcomment, offset);
         tempValue = strcomment.substr(offset, nexttoken - offset);
         transition = atof(tempValue.c_str());
      }

      // check for the "significant_size"
      pos = pComment.find(disSIG.c_str());
      if (pos != std::string::npos)
      {
         offset = pos + disSIG.length() + 1;
         nexttoken = getTokenPosition(strcomment, offset);
         tempValue = strcomment.substr(offset, nexttoken - offset);
         significant_size = atof(tempValue.c_str());
      }

      // check for the "lod_center"
      pos = pComment.find(disLodCenter.c_str());
      if (pos != std::string::npos)
      {
         offset = pos + disLodCenter.length() + 1;
         nexttoken = getTokenPosition(strcomment, offset);
         tempValue = strcomment.substr(offset, nexttoken - offset); // 1,2,3
         // X
         size_t posx = getXYZPosition(tempValue, 0);
         std::string xyzValue = tempValue.substr(0, posx);
         float x = atof(xyzValue.c_str());
         // Y
         posx++;
         size_t posy = getXYZPosition(tempValue, posx);
         xyzValue = tempValue.substr(posx, posy - posx);
         float y = atof(xyzValue.c_str());
         // Z
         posy++;
         size_t posz = getXYZPosition(tempValue, posy);
         xyzValue = tempValue.substr(posy, posz - posy);
         float z = atof(xyzValue.c_str());

         lod_center.set(x, y, z);
         hasLodCenter = true;
      }
   }
   return hasLodCenter;
}

size_t OsgFbxReader::getTokenPosition(const std::string& strcomment, size_t offset)
{
   size_t nexttoken = strcomment.find('@', offset);
   if (nexttoken == std::string::npos)
   {
      return strcomment.length();
   }
   else
   {
      return nexttoken;
   }
}

size_t OsgFbxReader::getXYZPosition(const std::string& strcomment, size_t offset)
{
   size_t nexttoken = strcomment.find(',', offset);
   if (nexttoken == std::string::npos)
   {
      return strcomment.length();
   }
   else
   {
      return nexttoken;
   }
}

std::string OsgFbxReader::returnEndNodeName(const std::string& nodeName)
{
   std::string endNodeName = "";
   if (nodeName.length() > 5)
   {
      endNodeName = nodeName.substr(nodeName.length() - 5, 4);
   }
   return endNodeName;
}

osgSim::MultiSwitch* addSwitch(FbxNode* pNode, const FbxString& pComment)
{
   // If a valid vantage switch comment is found it will add a switch
   osgSim::MultiSwitch* pSwitch = new osgSim::MultiSwitch;
   pSwitch->setName(pNode->GetName());
   pSwitch->addDescription(fbxUtil::removeReturn(pComment));
   pSwitch->setValue(0, 0, true);
   return pSwitch;
}

static const std::string destroyed = "_destroyed_";
bool isNodeNameDestroyState(const std::string& nodeName, const std::map<std::string, std::string>& stateNodeMap)
{
   if (nodeName.find(destroyed) != std::string::npos)
   {
      return true;
   }
   // loop on all destroy state node name from the CVS file
   for (std::map<std::string, std::string>::const_iterator it = stateNodeMap.begin(); it != stateNodeMap.end(); ++it)
   {
      // look for DESTROY state node name
      if (nodeName.find(it->second) != std::string::npos)
      {
         return true;
      }
   }
   return false;
}

osg::Group* addState(FbxNode* pNode, const FbxString& pComment, bool foundStateName, const std::map<std::string, std::string>& stateNodeMap)
{
   // If a valid vantage state comment is found it will add a group with comment
   osg::Group* pGroup = new osg::Group;
   std::string nodeName = pNode->GetName();
   pGroup->setName(nodeName.c_str());
   // if the nodename contain maingroup_ in it we need to
   // add the proper state comment and check if we need to add a parent switch node
   if (foundStateName)
   {
      if (isNodeNameDestroyState(nodeName, stateNodeMap))
      {
         // destroyed state
         pGroup->addDescription("@dis state 3");
      }
      else
      {
         // normal state
         pGroup->addDescription("@dis state 0,1,2");
      }
   }
   else
   {
      pGroup->addDescription(fbxUtil::removeReturn(pComment));
   }
   return pGroup;
}

osg::MatrixTransform* addArticulatedPart(FbxNode* pNode, const FbxString& pComment, const osg::Matrix& localMatrix, bool& hasDof,
   FbxScene& fbxScene)
{
   std::string strComment = fbxUtil::replaceReturnWithSpaces(pComment);
   int partNumber = 0;
   osg::Vec3f translate_min, translate_current, translate_max, translate_step,
      rotate_min, rotate_current, rotate_max, rotate_step,
      scale_min, scale_current, scale_max, scale_step;
   // Read all the parameters
   unsigned long limitationFlag = readArticulatedPartParameters(strComment, partNumber, 
      translate_min, translate_current, translate_max, translate_step,
      rotate_min, rotate_current, rotate_max, rotate_step,
      scale_min, scale_current, scale_max, scale_step);

   // If a valid vantage articulated part comment is found it will add a matrix and DOF
   // under it with the comment
   osg::MatrixTransform* pTransform = new osg::MatrixTransform(localMatrix);
   pTransform->setDataVariance(osg::Object::STATIC);
   osgSim::DOFTransform* pDOF = new osgSim::DOFTransform();
   pDOF->setDataVariance(osg::Object::DYNAMIC);
   pDOF->setName(pNode->GetName());
   pDOF->addDescription(strComment);

   //translations:
   pDOF->setMinTranslate(translate_min);
   pDOF->setMaxTranslate(translate_max);
   pDOF->setCurrentTranslate(translate_current);
   pDOF->setIncrementTranslate(translate_step);

   //rotations:
   pDOF->setMinHPR(rotate_min);
   pDOF->setMaxHPR(rotate_max);
   pDOF->setCurrentHPR(rotate_current);
   pDOF->setIncrementHPR(rotate_step);

   //scales:
   pDOF->setMinScale(scale_min);
   pDOF->setMaxScale(scale_max);
   pDOF->setCurrentScale(scale_current);
   pDOF->setIncrementScale(scale_step);

   // PUT MATRIX based on the pivot point
   // The section below was found based on trial and error
   // not sure it's a 100% accurate
   osg::Matrix pivotMatrixTrans;
   pivotMatrixTrans.makeIdentity();
   FbxDouble3 fbxRotPiv = pNode->RotationPivot.Get();
   FbxDouble3 fbxLclRot = pNode->LclRotation.Get();
   pivotMatrixTrans.setTrans(fbxRotPiv[0], fbxRotPiv[1], fbxRotPiv[2]);

   // Possible rotation of axis
   osg::Matrix pivotRotationMatrix;
   pivotRotationMatrix.makeIdentity();
   bool rotationActive = pNode->RotationActive.Get();
   EFbxRotationOrder fbxRotOrder = rotationActive ? pNode->RotationOrder.Get() : eEulerXYZ;
   if (rotationActive)
   {
      pivotRotationMatrix.preMultRotate(makeQuat(fbxLclRot, fbxRotOrder));
   }
   osg::Vec4 temp(fbxRotPiv[0], fbxRotPiv[1], fbxRotPiv[2], 0.0);
   temp = temp * pivotRotationMatrix;
   pivotMatrixTrans.setTrans(temp[0], temp[1], temp[2]);

   pDOF->setPutMatrix(pivotMatrixTrans);
   pDOF->setInversePutMatrix(osg::Matrix::inverse(pivotMatrixTrans));
   // End PUT MATRIX

   // Set the limitation flag if some parameters were found in the comments
   pDOF->setLimitationFlags(limitationFlag);
   
   hasDof = true;
   pTransform->addChild(pDOF);
   return pTransform;
}

osgDB::ReaderWriter::ReadResult addExternalReference(const FbxString& pComment, const osg::Matrix& localMatrix,
   const osgDB::Options& options, const std::string& currentModelPath)
{
   size_t offset = disExternalRef.length() + 1;
   // remove return in string
   std::string filename = fbxUtil::removeReturn(pComment);
   filename = filename.substr(offset, filename.length() - offset);
   // remove quote around filename
   fbxUtil::removeQuote(filename);
   // check for existing path
   std::string filepath = osgDB::getFilePath(filename);
   if (filepath.empty())
   {
      // if no path exist add the parent path
      filename = currentModelPath + osgDB::getNativePathSeparator() + filename;
   }

   ReaderWriterFBX FbxReader;
   return FbxReader.readNode(filename, &options);
}

osg::Sequence* addFlipBookAnimation(FbxNode* pNode, const FbxString& pComment, osg::NodeList& children)
{
   // flip book animation
   int nbFrame = 0;
   bool skip = true;  // Used in  DtArticulatedModelParser::parseAnimationDeclaration()
   int loopCount = 0;  // Also used in  DtArticulatedModelParser::parseAnimationDeclaration()
   float lastFrameTime = 0.0f; // Used in DtArticulatedModelParser::parseAnimationDeclaration()
   float loopDuration = 0.0f;
   bool swing = false;
   bool forwardAnim = true;
   std::string comment = pComment.Buffer();
   readFlipBookAnimationParameters(comment, nbFrame, skip, loopCount, lastFrameTime, loopDuration, swing, forwardAnim);

   // format of the comment: "animation num_frames_per_second [ skip | noskip ] loopCount count lastFrameTime time"
   // ex.  "animation 2 skip loopCount 3 lastFrameTime 11.5"
   osg::Sequence* pSequence = new osg::Sequence;
   pSequence->setName(pNode->GetName());
   pSequence->addDescription(fbxUtil::removeReturn(pComment));

   // Regardless of forwards or backwards, animation could have swing bit set.
   osg::Sequence::LoopMode loopMode = (swing) ?
      osg::Sequence::SWING : osg::Sequence::LOOP;

   if (forwardAnim)
      pSequence->setInterval(loopMode, 0, -1);
   else
      pSequence->setInterval(loopMode, -1, 0);

   float frameDuration;
   if (loopDuration == 0.0f)
   {
      frameDuration = 0.1f;     // 10Hz
   }
   else
   {
      frameDuration = loopDuration / float(children.size());
   }

   for (unsigned int i = 0; i < children.size(); i++)
   {
      pSequence->setTime(i, frameDuration);
   }

   //// Set number of repetitions.
   if (loopCount > 0)
      pSequence->setDuration(1.0f, loopCount);
   else
      pSequence->setDuration(1.0f);        // Run continuously

   pSequence->setMode(osg::Sequence::START);
   return pSequence;
}

osgAnimation::Bone* addBone(FbxNode* pNode, const std::string& animName, FbxScene& fbxScene, std::map<FbxNode*, osg::Node*>& nodeMap)
{
   osgAnimation::Bone* osgBone = new osgAnimation::Bone;
   osgBone->setDataVariance(osg::Object::DYNAMIC);
   osgBone->setName(pNode->GetName());
   osgAnimation::UpdateBone* pUpdate = new osgAnimation::UpdateBone(animName);
   readUpdateMatrixTransform(pUpdate, pNode, fbxScene);
   osgBone->setUpdateCallback(pUpdate);

   nodeMap.insert(std::pair<FbxNode*, osg::Node*>(pNode, osgBone));

   return osgBone;
}

osg::Group* addGroup(FbxNode* pNode, const FbxString& pComment)
{
   osg::Group* pGroup = new osg::Group;
   pGroup->setName(pNode->GetName());
   if (!pComment.IsEmpty())
   {
      pGroup->addDescription(fbxUtil::removeReturn(pComment));
   }
   return pGroup;
}

osg::MatrixTransform* addTransform(FbxNode* pNode, const FbxString& pComment, 
   const osg::Matrix& localMatrix, FbxScene& fbxScene, const std::string& animName, osgAnimation::Animation* animation, bool bAnimated)
{
   if (bAnimated)
   {
      osgAnimation::AnimationMatrixTransform* pAnimTransform = new osgAnimation::AnimationMatrixTransform();
      pAnimTransform->setDataVariance(osg::Object::DYNAMIC);
      pAnimTransform->setName(pNode->GetName());
      osgAnimation::UpdateMatrixTransform* pUpdate = new osgAnimation::UpdateMatrixTransform(animName);
      readUpdateMatrixTransform(pUpdate, pNode, fbxScene);
      pAnimTransform->setUpdateCallback(pUpdate);
      pAnimTransform->setAnimation(animation);
      // if we have a dis frame or annotation comment add it to the matrix transform and not the geode
      if (!pComment.IsEmpty())
      {
         pAnimTransform->addDescription(fbxUtil::removeReturn(pComment));
      }
      return pAnimTransform;
   }
   else
   {
      osg::MatrixTransform* pTransform = new osg::MatrixTransform(localMatrix);
      pTransform->setName(pNode->GetName());
      pTransform->setDataVariance(osg::Object::STATIC);
      // if we have a dis frame or annotation comment add it to the matrix transform and not the geode
      if (!pComment.IsEmpty())
      {
         pTransform->addDescription(fbxUtil::removeReturn(pComment));
      }
      return pTransform;
   }
}