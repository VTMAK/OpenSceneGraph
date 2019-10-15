/******************************************************************************
** Copyright (c) 2019 VT-MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/
#ifndef IVE_ANIMATIONMATRIXTRANSFORM
#define IVE_IVE_ANIMATIONMATRIXTRANSFORM 1

#include <osgAnimation/AnimationMatrixTransform>
#include "ReadWrite.h"

namespace ive{
class AnimationMatrixTransform : public osgAnimation::AnimationMatrixTransform, public ReadWrite {
public:
	void write(DataOutputStream* out);
	void read(DataInputStream* in);
};
}

#endif
