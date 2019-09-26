/******************************************************************************
** Copyright (c) 2019 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file fbxMatrixTransformVisitor.h
//! \brief Parse the scene graph of the Fbx Model to accumulate matrix tranforms
//!        and apply to the DOF putmatrix
//! \ingroup fbx
#pragma once

#include <osg/NodeVisitor>
#include <osg/MatrixTransform>

#include <osgSim/DOFTransform>


class fbxMatrixTransformVisitor : public osg::NodeVisitor
{
public:
   fbxMatrixTransformVisitor();
   virtual ~fbxMatrixTransformVisitor();

   //! traverse node
   virtual void apply(osg::Node& node);

   //! visit all the matrix transform to retrieve the 
   //! matrix and add them to a stack to be use by the 
   //! dof put matrix
   virtual void apply(osg::MatrixTransform& mt);

   //! apply the current matrix stack to the put matrix
   //! of the dof
   virtual void apply(osg::Transform& tr);
   virtual void apply(osgSim::DOFTransform& dof);

protected:
   //! keep a matrix stack to be used by the dof put matrix
   std::vector<osg::Matrix> _matrixStack;

};
