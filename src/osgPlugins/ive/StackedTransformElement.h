/******************************************************************************
** Copyright (c) 2019 VT-MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/
#ifndef IVE_STACKEDTRANSFORMELEMENT
#define IVE_STACKEDTRANSFORMELEMENT 1

#include <osgAnimation/StackedTransformElement>
#include "ReadWrite.h"

namespace ive {
   class StackedTransformElement : public osgAnimation::StackedTransformElement, public ReadWrite {
   public:
      void write(DataOutputStream* out);
      void read(DataInputStream* in);
      static osg::Object* static_read(DataInputStream* in);
   };
}

#endif
