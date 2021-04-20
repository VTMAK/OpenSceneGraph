/******************************************************************************
** Copyright (c) 2021 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file fbxMatrixTransformVisitor.cxx
//! \brief Contains fbxMatrixTransformVisitor class.

#include "fbxMatrixTransformVisitor.h"
#include <osg/LOD>


   fbxMatrixTransformVisitor::fbxMatrixTransformVisitor()
      : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
   {

   }

   fbxMatrixTransformVisitor::~fbxMatrixTransformVisitor()
   {

   }

   void fbxMatrixTransformVisitor::apply(osg::Node& node)
   {
      osg::NodeVisitor::apply(node);
   }

   void fbxMatrixTransformVisitor::apply(osg::MatrixTransform& mt)
   {
      bool pushed = false;
      if (typeid(mt) == typeid(osg::MatrixTransform))
      {
         osg::Matrix matrix = mt.getMatrix();
         if (!matrix.isIdentity())
         {
            _matrixStack.push_back(matrix);
            pushed = true;
         }
      }
      traverse(mt);

      // pop matrix off of stack
      if (pushed)
      {
         _matrixStack.pop_back();
      }
   }

   void fbxMatrixTransformVisitor::apply(osg::Transform& tr)
   {
      osgSim::DOFTransform* dof = dynamic_cast<osgSim::DOFTransform*>(&tr);
      if (dof)
      {
         apply(*dof);
      }
      else
      {
         traverse(tr);
      }
   }

   void fbxMatrixTransformVisitor::apply(osgSim::DOFTransform& dof)
   {
      if (typeid(dof) == typeid(osgSim::DOFTransform))
      {
         if (!_matrixStack.empty())
         {
            osg::Matrix putMatrix;
            putMatrix.makeIdentity();
            for (size_t i = 0; i < _matrixStack.size(); ++i)
            {
               // multiply the matrix from the stack
               putMatrix = _matrixStack[i] * putMatrix;
            }
            putMatrix = dof.getPutMatrix() * putMatrix;

            // add the result in the put matrix
            dof.setInversePutMatrix(putMatrix);
            dof.setPutMatrix(osg::Matrix::inverse(putMatrix));
         }
      }
      traverse(dof);
   }

