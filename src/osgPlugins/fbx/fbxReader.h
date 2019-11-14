/******************************************************************************
** Copyright(c) 2019 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

#ifndef FBXRANIMATION_H
#define FBXRANIMATION_H

#include <fbxsdk.h>
#include <osgDB/ReaderWriter>
#include <osgAnimation/BasicAnimationManager>
#include <osgSim/MultiSwitch>
#include <osgSim/DOFTransform>

#include "fbxMaterialToOsgStateSet.h"
#include "fbxUtil.h"
#include "fbxTextureUnitMap.h"

namespace osgAnimation
{
    class AnimationManagerBase;
    class RigGeometry;
    class Skeleton;
    class Animation;
}

class OsgFbxReader
{
public:
    FbxManager& pSdkManager;
    FbxScene& fbxScene;
    osg::ref_ptr<osgAnimation::AnimationManagerBase> pAnimationManager;
    FbxMaterialToOsgStateSet& fbxMaterialToOsgStateSet;
    std::map<FbxNode*, osg::Node*> nodeMap;
    BindMatrixMap boneBindMatrices;
    const std::set<const FbxNode*>& fbxSkeletons;
    std::map<FbxNode*, osgAnimation::Skeleton*> skeletonMap;
    const osgDB::Options& options;
    const std::string& currentFilePath;
    bool lightmapTextures, tessellatePolygons;

    enum AuthoringTool
    {
        UNKNOWN,
        OPENSCENEGRAPH,
        AUTODESK_3DSTUDIO_MAX
    } authoringTool;

    OsgFbxReader(
        FbxManager& pSdkManager1,
        FbxScene& fbxScene1,
        FbxMaterialToOsgStateSet& fbxMaterialToOsgStateSet1,
        const std::set<const FbxNode*>& fbxSkeletons1,
        const osgDB::Options& options1,
        AuthoringTool authoringTool1,
        bool lightmapTextures1,
        bool tessellatePolygons1,
        const std::string& currentFilePath1)
        : pSdkManager(pSdkManager1),
        fbxScene(fbxScene1),
        fbxMaterialToOsgStateSet(fbxMaterialToOsgStateSet1),
        fbxSkeletons(fbxSkeletons1),
        options(options1),
        lightmapTextures(lightmapTextures1),
        tessellatePolygons(tessellatePolygons1),
        authoringTool(authoringTool1),
        currentFilePath(currentFilePath1)
    {
       readNodeMapCSVfile();
    }

    osgDB::ReaderWriter::ReadResult readFbxNode(
        FbxNode*, bool& bIsBone, int& nLightCount,
       textureUnitMap& textureMap, const FbxString& appName);

    std::string readFbxAnimation(
        FbxNode*, const char* targetName, osgAnimation::Animation*& animation);

    osgDB::ReaderWriter::ReadResult readFbxCamera(
        FbxNode* pNode);
    
    //! Read the FBX light and convert to OSG
    osgDB::ReaderWriter::ReadResult readFbxLight(
        FbxNode* pNode, int& nLightCount);

    osgDB::ReaderWriter::ReadResult readMesh(
        FbxNode* pNode, FbxMesh* fbxMesh,
        std::vector<StateSetContent>& stateSetList,
        const char* szName,
        textureUnitMap& textureMap);

    osgDB::ReaderWriter::ReadResult readFbxMesh(
        FbxNode* pNode,
        std::vector<StateSetContent>&,
        textureUnitMap& textureMap);

protected:

    //! Read the Light but based on the node comment create
    //! create a Vantage light point
    osgDB::ReaderWriter::ReadResult readFbxLightPoint(
       FbxNode* pNode, const std::string& lightType);

    //! Read the FBX light and create a dynamic light
    osgDB::ReaderWriter::ReadResult readFbxDynamicLight(
       FbxNode* pNode, int& nLightCount);

    //! Read the @dis frame information from the node comment
    //! and add the child node to the group
    void addFlipbookAnimationFrame(osg::NodeList::iterator& itNodeList,
       osg::Group* pGroup, const FbxString& pComment);

    //! Read the _LOD node name and @dis comment from the node 
    //! to add the child node to the group
    void readLodInformation(osg::Node* childNode, osg::Group* pGroup, 
       const std::string& pNodeName, const std::string& nodeComment,
       bool& pAddLod, int& pChildId, osg::ref_ptr<osg::LOD>& pLod);

    //! Find if the LOD contain different comments use for LOD settings
    //! @dis lod 1
    //! @dis switch_in 50
    //! @dis switch_out 0
    //! @dis transition 0
    //! @dis significant_size 0
    //! @dis lod_center 0, 0, 0
    bool findLodComments(const std::string& pComment, int& cLod,
       float& switch_in, float& switch_out, float& transition,
       float& significant_size, osg::BoundingSphere::vec_type& lod_center);

    //! Get the next token position from a parameter string
    size_t getTokenPosition(const std::string& strcomment, size_t offset);

    //! Get the next x,y,z position from a parameter string
    size_t getXYZPosition(const std::string& strcomment, size_t offset);

    //! Return the 5 last character of the node name
    std::string returnEndNodeName(const std::string& nodeName);

    //! Special case, check if we need to add a damage state node parent because it's missing)
    bool addDamageSwitch(osg::Node* osgChild, osg::NodeList& children);

    //! Read the Vantage Map Node CSV 
    void readNodeMapCSVfile();
    
    //! Add a comment from the node map if needed
    void addCommentFromNodeName(FbxNode* pNode);

    //! Retrieve a @dis comment based on the node name (this is filled using the Vantage FBX_NodeNameMap.csv file)
    std::string getCommentFromNodeName(const std::string& nodeName);

    //! Check if the nodename is called "maingroup_" or "xxx" from the "Node" field
    //! in FBX_NodeNameMap.csv with a "type=DamageState" 
    size_t findNormalStateNode(const std::string& nodeName);

    //! Check if the nodename is called "maingroup_destroyed" or "xxx" from the "Comment" field
    //! in FBX_NodeNameMap.csv with a "type=DamageState" 
    size_t findDamageStateNode(const std::string& nodeName);

    //! Flag to know if the mapping file was read
    static bool CSVMapFileRead;

    //! map that keep the @dis comment based on node name
    typedef std::map<std::string, std::string> NodeNameToCommentMap;
    NodeNameToCommentMap _nodeNameMap;
    //! map of normal/damage state node name
    NodeNameToCommentMap _nodeNameStateMap;
};

osgAnimation::Skeleton* getSkeleton(FbxNode*,
    const std::set<const FbxNode*>& fbxSkeletons,
    std::map<FbxNode*, osgAnimation::Skeleton*>&);

//! Add Switch to the group 
osgSim::MultiSwitch* addSwitch(FbxNode* pNode, const FbxString& pComment);

//! Add DIS state
osg::Group* addState(FbxNode* pNode, const FbxString& pComment, bool foundStateName, const std::map<std::string, std::string>& stateNodeMap);

//! Add articulation part 
osg::MatrixTransform* addArticulatedPart(FbxNode* pNode, const FbxString& pComment, const osg::Matrix& localMatrix, bool& hasDof,
   FbxScene& fbxScene);

//! Add FlipBook animation 
osg::Sequence* addFlipBookAnimation(FbxNode* pNode, const FbxString& pComment, osg::NodeList& children);

//! Add Bone 
osgAnimation::Bone* addBone(FbxNode* pNode, const std::string& animName, FbxScene& fbxScene, std::map<FbxNode*, osg::Node*>& nodeMap);

//! Add Group
osg::Group* addGroup(FbxNode* pNode, const FbxString& pComment);

//! Add Transform
osg::MatrixTransform* addTransform(FbxNode* pNode, const FbxString& pComment,
   const osg::Matrix& localMatrix, FbxScene& fbxScene, const std::string& animName, osgAnimation::Animation* animation, bool bAnimated);

//! Add an external reference (read another FBX model an add it to current one)
osgDB::ReaderWriter::ReadResult addExternalReference(const FbxString& pComment, const osg::Matrix& localMatrix, 
   const osgDB::Options& options, const std::string& currentModelPath);

#endif
