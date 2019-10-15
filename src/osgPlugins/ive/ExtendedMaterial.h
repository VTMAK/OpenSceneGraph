#ifndef IVE_EXTENDED_MATERIAL
#define IVE_EXTENDED_MATERIAL 1

#include <osg/ExtendedMaterial>
#include "ReadWrite.h"

namespace ive{
	class ExtendedMaterial : public osg::ExtendedMaterial, public ReadWrite {
	public:
		void write(DataOutputStream* out);
		void read(DataInputStream* in);
	};
}

#endif
