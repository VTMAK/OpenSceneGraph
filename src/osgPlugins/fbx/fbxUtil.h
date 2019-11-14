/******************************************************************************
** Copyright(c) 2018 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file fbxUtil.h
//! \brief Utility class with various functions
//! \ingroup osgdb_fbx

#pragma once
#include <sstream>
#include <fbxsdk.h>
#include <osg/Node>
#include <osgAnimation/AnimationManagerBase>
#include <osgAnimation/Bone>
#include <osgAnimation/RigGeometry>
#include <osgAnimation/Skeleton>
#include <osgAnimation/VertexInfluence>
#include <boost/filesystem.hpp>
#include <map>

typedef std::map<std::pair<FbxNode*, osgAnimation::RigGeometry*>, osg::Matrix> BindMatrixMap;

class fbxUtil
{
public:
   //! \brief CTOR
   fbxUtil();
   //! \brief DTOR
   virtual ~fbxUtil();

   //! \brief retrieve a comment from a FbxNode
   static bool getCommentProperty(FbxNode* pNode, FbxString& pComment);

   //! \brief retrieve the color from a geometry
   static bool getColorProperty(FbxGeometry* pGeometry, FbxColor& pColor);

   //! Returns true if the given node is a basic root group with no special information.
   //! Used in conjunction with UseFbxRoot option.
   //! Identity transforms are considered as basic root nodes.
   static bool isBasicRootNode(const osg::Node& node);

   //!Some files don't correctly mark their skeleton nodes, so this function infers
   //!them from the nodes that skin deformers linked to.
   static void findLinkedFbxSkeletonNodes(FbxNode* pNode, std::set<const FbxNode*>& fbxSkeletons);

   //! 
   static void resolveBindMatrices( osg::Node& root,
      const BindMatrixMap& boneBindMatrices,
      const std::map<FbxNode*, osg::Node*>& nodeMap);

   //! Return the min/max switch distance based on the node name
   static void minMaxSwitchDistance(const std::string& nodeName, int& lod, float& min, float& max);

   // Remove \n and \r from the string
   static std::string removeReturn(const FbxString& pComment);
   static std::string removeReturn(const std::string& pComment);

   //! Remove \" 
   static void removeQuote(std::string& pComment);

   //! Remove spaces ' '
   static void removeSpaces(std::string& pComment);

   //! Resolve a relative path
   static boost::filesystem::path resolve(
      const boost::filesystem::path& p,
      const boost::filesystem::path& base);

   //! Resolve the path (from relative to absolute)
   static boost::filesystem::path resolve(const boost::filesystem::path& p);

protected:

   //! return an absolute path
   static boost::filesystem::path absolute(const boost::filesystem::path& p, const boost::filesystem::path& base);

};
