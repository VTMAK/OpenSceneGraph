/******************************************************************************
** Copyright (c) 2019 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file AnimationMatrixTransform.cpp
//! \brief Contains A Matrix transform with an animation reference

#include <osgAnimation/AnimationMatrixTransform>

using namespace osgAnimation;

AnimationMatrixTransform::AnimationMatrixTransform():
   MatrixTransform()
{

}

AnimationMatrixTransform::AnimationMatrixTransform(const AnimationMatrixTransform& amt, const osg::CopyOp& copyop)
   :osg::MatrixTransform(amt, copyop)
{
   const osgAnimation::AnimationMatrixTransform* animMt
      = dynamic_cast<const osgAnimation::AnimationMatrixTransform*>(&amt);
   if (animMt)
   {
      _animation = animMt->getAnimation();
   }
}

AnimationMatrixTransform::AnimationMatrixTransform(const osg::Matrix& matrix)
   :osg::MatrixTransform(matrix)
{
   
}


AnimationMatrixTransform::~AnimationMatrixTransform()
{
   _animation.release();
}


void AnimationMatrixTransform::setAnimation(Animation* animation)
{
   _animation = animation;
}

Animation* AnimationMatrixTransform::getAnimation() const
{
   return _animation.get();
}

