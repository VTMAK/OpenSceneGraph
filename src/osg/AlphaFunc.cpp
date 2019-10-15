/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2006 Robert Osfield
 *
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * OpenSceneGraph Public License for more details.
*/

#include <osg/AlphaFunc>
#include <osg/Notify>
#include <osg/State>
#include <osg/Material>

#include <sstream>
#include <iomanip>

using namespace osg;
/**
* StateAttributeCallback that will update osg::AlphaFunction properties as Uniforms
*/
/*
class  AlphaFunctionCallback : public osg::StateAttributeCallback
{
public:
	virtual void operator() (osg::StateAttribute* attr, osg::NodeVisitor* nv);
};

*/
static unsigned int _s_uboID = 0;

AlphaFunc::AlphaFunc()
{
    _comparisonFunc = ALWAYS;
    _referenceValue = 1.0f;

	 //setUpdateCallback(new AlphaFunctionCallback());

}


AlphaFunc::AlphaFunc(ComparisonFunction func, float ref) :
_comparisonFunc(func),
_referenceValue(ref) 
{
	//setUpdateCallback(new AlphaFunctionCallback());
}

/** Copy constructor using CopyOp to manage deep vs shallow copy. */
AlphaFunc::AlphaFunc(const AlphaFunc& af, const CopyOp& copyop) :
StateAttribute(af, copyop),
_comparisonFunc(af._comparisonFunc),
_referenceValue(af._referenceValue)
{
	//setUpdateCallback(new AlphaFunctionCallback());
}

AlphaFunc::~AlphaFunc()
{
}

void AlphaFunc::releaseUBO(osg::State* state) 
{
   if (_s_uboID)
   {
      if (state) {
         osg::ref_ptr<GLExtensions> _extensions = GLExtensions::Get(state->getContextID(), true);
         _extensions->glDeleteBuffers(1, &_s_uboID);
      }
      else {
         osg::MaterialManager::theMaterialManager.pushMaterialBufferToDelete(_s_uboID);
      }
      _s_uboID = 0;
   }
}

//! Destroy the openGL objects.
void AlphaFunc::releaseGLObjects(osg::State* state) const
{
   if (_s_uboID)
   {
      if (state) {
         osg::ref_ptr<GLExtensions> _extensions = GLExtensions::Get(state->getContextID(), true);
         _extensions->glDeleteBuffers(1, &_s_uboID);
      }
      else {
         osg::MaterialManager::theMaterialManager.pushMaterialBufferToDelete(_s_uboID);
      }
   }
   _s_uboID = 0;
}

void AlphaFunc::apply(State& state) const
{
#ifdef OSG_GL_FIXED_FUNCTION_AVAILABLE
   glAlphaFunc((GLenum)_comparisonFunc,_referenceValue);
#else
   /*
      static int warn = 1;
      if (warn){
      warn = 0;
      OSG_NOTICE << "Warning: AlphaFunc::apply(State&) - not supported." << std::endl;
      }*/

   //printf("%#08X %f\n", _comparisonFunc, _referenceValue);
   GLExtensions * _extensions = GLExtensions::Get(state.getContextID(), true);
   if (_s_uboID == 0)
   {
      // leaks on exit currently
      _extensions->glGenBuffers(1, &_s_uboID);
      _extensions->glBindBuffer(GL_UNIFORM_BUFFER, _s_uboID);
      if (_extensions->glObjectLabel){
         _extensions->glObjectLabel(GL_BUFFER, _s_uboID, -1, "alpha_uniforms");
      }
      _extensions->glBindBuffer(GL_UNIFORM_BUFFER, 0);
   }
   osg::Vec4f rval(-.1f, -.1f, -.1f, -.1f);
   if (_comparisonFunc == AlphaFunc::GREATER)
   {
      rval[0] = _referenceValue;
      rval[2] = _referenceValue;
   }
   else if (_comparisonFunc == AlphaFunc::GEQUAL)
   {
      rval[0] = _referenceValue;
   }
   else if (_comparisonFunc == AlphaFunc::ALWAYS)
   {
      rval[1] = 1.1f;
   }
   else if (_comparisonFunc == AlphaFunc::NEVER)
   {
      rval[0] = 1.1f;
   }
   else if (_comparisonFunc == AlphaFunc::LESS)
   {
      rval[0] = 1.1f;
      rval[1] = _referenceValue;
      rval[2] = _referenceValue;
   }
   else if (_comparisonFunc == AlphaFunc::LEQUAL)
   {
      rval[0] = 1.1f;
      rval[1] = _referenceValue;
   }
   else if (_comparisonFunc == AlphaFunc::EQUAL)
   {
      rval[0] = 1.1f;
      rval[2] = _referenceValue;
   }
   else if (_comparisonFunc == AlphaFunc::NOTEQUAL)
   {
      rval[3] = _referenceValue;
   }

   _extensions->glBindBuffer(GL_UNIFORM_BUFFER, _s_uboID);
   _extensions->glBufferData(GL_UNIFORM_BUFFER, 4 * sizeof(float), &rval, GL_DYNAMIC_DRAW);
   _extensions->glBindBuffer(GL_UNIFORM_BUFFER, 0);

   // FIXME matches hard code in program.cpp
   const int ALPHA_INDEX = 1;
   _extensions->glBindBufferBase(GL_UNIFORM_BUFFER, ALPHA_INDEX, _s_uboID);

#endif

}


/*............................................................................
// didn't work
void AlphaFunctionCallback::operator() (osg::StateAttribute* attr, osg::NodeVisitor* nv)
{
	osg::AlphaFunc* alpha = static_cast<osg::AlphaFunc*>(attr);
	for (unsigned int i = 0; i < attr->getNumParents(); i++)
	{
		osg::StateSet* stateSet = attr->getParent(i);

		stateSet->getOrCreateUniform("osg_alpha_threshold", osg::Uniform::FLOAT)->set(alpha->getReferenceValue());

		std::stringstream ss;
		ss << "0x" << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << alpha->getFunction();
		stateSet->setDefine("ALPHA_FUNCTION", ss.str());
	}
}

*/