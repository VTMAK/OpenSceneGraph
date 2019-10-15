/******************************************************************************
** Copyright (c) 2019 VT-MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/
#ifndef IVE_UPDATEMATRIXTRANSFORM
#define IVE_UPDATEATRIXTRANSFORM 1

#include <osgAnimation/UpdateMatrixTransform>
#include "ReadWrite.h"

namespace ive {
   class UpdateMatrixTransform : public osgAnimation::UpdateMatrixTransform, public ReadWrite {
   public:
      void write(DataOutputStream* out);
      void read(DataInputStream* in);
   protected:
      bool readStackedTransforms(DataInputStream* in, osgAnimation::UpdateMatrixTransform& obj);
      bool writeStackedTransforms(DataOutputStream* out, const osgAnimation::UpdateMatrixTransform& obj);
   };
}

#endif

