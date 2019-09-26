/******************************************************************************
** Copyright(c) 2018 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file fbxUtil.cpp
//! \brief Utility class with various functions
//! \ingroup osgdb_fbx


#include "fbxUtil.h"
#include <osg/Switch>
#include <osg/Geode>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>

fbxUtil::fbxUtil()
{

}

fbxUtil::~fbxUtil()
{

}

bool fbxUtil::getCommentProperty(FbxNode* pNode, FbxString& pComment)
{
   FbxProperty lComment = pNode->FindProperty("UDP3DSMAX", false);
   if (lComment.IsValid())
   {
      pComment = lComment.Get<FbxString>();
      return true;
   }
   else
   {
      pComment = "";
      return false;
   }
}

bool fbxUtil::getColorProperty(FbxGeometry* pGeometry, FbxColor& pColor)
{
   // Look if there is a color property in the geometry
   FbxProperty lColorProperty = pGeometry->FindProperty("Color", false);
   if (lColorProperty.IsValid())
   {
      if (lColorProperty.GetPropertyDataType() == FbxColor3DT)
      {
         pColor = lColorProperty.Get<FbxColor>();
         return true;
      }
   }
   return false;
}

/// Returns true if the given node is a basic root group with no special information.
/// Used in conjunction with UseFbxRoot option.
/// Identity transforms are considered as basic root nodes.
bool fbxUtil::isBasicRootNode(const osg::Node& node)
{
   const osg::Group* osgGroup = node.asGroup();
   if (!osgGroup || node.asGeode())        // WriterNodeVisitor handles Geodes the "old way" (= Derived from Node, not Group as for now). Geodes may be considered "basic root nodes" when WriterNodeVisitor will be adapted.
   {
      // Geodes & such are not basic root nodes
      return false;
   }

   // Test if we've got an empty transform (= a group!)
   const osg::Transform* transform = osgGroup->asTransform();
   if (transform)
   {
      if (const osg::MatrixTransform* matrixTransform = transform->asMatrixTransform())
      {
         if (!matrixTransform->getMatrix().isIdentity())
         {
            // Non-identity matrix transform
            return false;
         }
      }
      else if (const osg::PositionAttitudeTransform* pat = transform->asPositionAttitudeTransform())
      {
         if (pat->getPosition() != osg::Vec3d() ||
            pat->getAttitude() != osg::Quat() ||
            pat->getScale() != osg::Vec3d(1.0f, 1.0f, 1.0f) ||
            pat->getPivotPoint() != osg::Vec3d())
         {
            // Non-identity position attribute transform
            return false;
         }
      }
      else
      {
         // Other transform (not identity or not predefined type)
         return false;
      }
   }

   // Test the presence of a non-empty stateset
   if (node.getStateSet())
   {
      osg::ref_ptr<osg::StateSet> emptyStateSet = new osg::StateSet;
      if (node.getStateSet()->compare(*emptyStateSet, true) != 0)
      {
         return false;
      }
   }

   return true;
}

//Some files don't correctly mark their skeleton nodes, so this function infers
//them from the nodes that skin deformers linked to.
void fbxUtil::findLinkedFbxSkeletonNodes(FbxNode* pNode, std::set<const FbxNode*>& fbxSkeletons)
{
   if (const FbxGeometry* pMesh = FbxCast<FbxGeometry>(pNode->GetNodeAttribute()))
   {
      for (int i = 0; i < pMesh->GetDeformerCount(FbxDeformer::eSkin); ++i)
      {
         const FbxSkin* pSkin = (const FbxSkin*)pMesh->GetDeformer(i, FbxDeformer::eSkin);

         for (int j = 0; j < pSkin->GetClusterCount(); ++j)
         {
            const FbxNode* pSkeleton = pSkin->GetCluster(j)->GetLink();
            fbxSkeletons.insert(pSkeleton);
         }
      }
   }

   for (int i = 0; i < pNode->GetChildCount(); ++i)
   {
      findLinkedFbxSkeletonNodes(pNode->GetChild(i), fbxSkeletons);
   }
}

void fbxUtil::resolveBindMatrices(
   osg::Node& root,
   const BindMatrixMap& boneBindMatrices,
   const std::map<FbxNode*, osg::Node*>& nodeMap)
{
   std::set<std::string> nodeNames;
   for (std::map<FbxNode*, osg::Node*>::const_iterator it = nodeMap.begin(); it != nodeMap.end(); ++it)
   {
      nodeNames.insert(it->second->getName());
   }

   for (BindMatrixMap::const_iterator it = boneBindMatrices.begin();
      it != boneBindMatrices.end();)
   {
      FbxNode* const fbxBone = it->first.first;
      std::map<FbxNode*, osg::Node*>::const_iterator nodeIt = nodeMap.find(fbxBone);
      if (nodeIt != nodeMap.end())
      {
         const osg::Matrix bindMatrix = it->second;
         osgAnimation::Bone& osgBone = dynamic_cast<osgAnimation::Bone&>(*nodeIt->second);
         osgBone.setInvBindMatrixInSkeletonSpace(bindMatrix);

         ++it;
         for (; it != boneBindMatrices.end() && it->first.first == fbxBone; ++it)
         {
            if (it->second != bindMatrix)
            {
               std::string name;
               for (int i = 0;; ++i)
               {
                  std::stringstream ss;
                  ss << osgBone.getName() << '_' << i;
                  name = ss.str();
                  if (nodeNames.insert(name).second)
                  {
                     break;
                  }
               }
               osgAnimation::Bone* newBone = new osgAnimation::Bone(name);
               newBone->setDefaultUpdateCallback();
               newBone->setInvBindMatrixInSkeletonSpace(it->second);
               osgBone.addChild(newBone);

               osgAnimation::RigGeometry* pRigGeometry = it->first.second;

               osgAnimation::VertexInfluenceMap* vertexInfluences = pRigGeometry->getInfluenceMap();

               osgAnimation::VertexInfluenceMap::iterator vimIt = vertexInfluences->find(osgBone.getName());
               if (vimIt != vertexInfluences->end())
               {
                  osgAnimation::VertexInfluence vi;
                  vi.swap(vimIt->second);
                  vertexInfluences->erase(vimIt);
                  osgAnimation::VertexInfluence& vi2 = (*vertexInfluences)[name];
                  vi.swap(vi2);
                  vi2.setName(name);
               }
               else
               {
                  OSG_WARN << "No vertex influences found for \"" << osgBone.getName() << "\"" << std::endl;
               }
            }
         }
      }
      else
      {
         OSG_WARN << "No bone found for \"" << fbxBone->GetName() << "\"" << std::endl;
         ++it;
      }
   }
}

void fbxUtil::minMaxSwitchDistance(const std::string& nodeName, int& lod, float& min, float& max)
{
   // The nodename should end with "_LODx", this should be check before this call
   lod = atoi(&nodeName[nodeName.length()-1]);
   //pLod->setCenter(center*document.unitScale());
   min = 0.0;
   max = 10000.0;
   // Temp map
   switch (lod)
   {
   case 0:
      min = 0.0;
      max = 200.0;
      break;
   case 1:
      min = 200.0;
      max = 400.0;
      break;
   case 2:
      min = 400.0;
      max = 600.0;
      break;
   case 3:
      min = 600.0;
      max = 800.0;
      break;
   case 4:
      min = 800.0;
      max = 2000.0;
      break;
   case 5:
      min = 2000.0;
      max = 10000.0;
      break;
   default:
      OSG_WARN << "FBX should probably only have 5 lod! This is LOD " << lod << std::endl;
      min = 10000.0;
      max = 20000.0;
      break;
   }
}
