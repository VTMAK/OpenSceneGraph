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
#include <osg/State>
#include <osg/Texture>
#include <osg/Notify>
#include <osg/GLU>
#include <osg/GLExtensions>
#include <osg/Drawable>
#include <osg/ApplicationUsage>
#include <osg/ContextData>
#include <osg/PointSprite>
#include <osg/os_utils>

#include <sstream>
#include <algorithm>

// VRV_PATCH BEGIN - for updateCurrentDefines sprintf
#include <stdio.h>
#include <osg/Profile>
// VRV_PATCH END

#ifndef GL_MAX_TEXTURE_COORDS
#define GL_MAX_TEXTURE_COORDS 0x8871
#endif

#ifndef GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#endif

#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif

using namespace std;
using namespace osg;

static ApplicationUsageProxy State_e0(ApplicationUsage::ENVIRONMENTAL_VARIABLE,"OSG_GL_ERROR_CHECKING <type>","ONCE_PER_ATTRIBUTE | ON | on enables fine grained checking,  ONCE_PER_FRAME enables coarse grained checking");

#define USE_TRANS_STACK_UBO

namespace osg
{
   struct osg_TransformStack
   {
      osg::Matrixf ModelViewMatrixInverse;
      osg::Matrixf ModelViewMatrix;
      osg::Matrixf ModelViewProjectionMatrix;
      osg::Matrixf ProjectionMatrix;
      osg::Matrixf NormalMatrix;
   };
}
const char * UBO_DECL =
"struct osg_TransformStack\n"
"{\n"
"    mat4 ModelViewMatrixInverse;\n"
"    mat4 ModelViewMatrix;\n"
"    mat4 ModelViewProjectionMatrix;\n"
"    mat4 ProjectionMatrix;\n"
"    mat3 NormalMatrix;\n"
"    vec4 padding;\n"
"};"
"\n\n"
"layout(std140) uniform osg_transform_stack_block\n"
"{\n"
"   osg_TransformStack osg;\n"
"};\n";

const char * State::getTransformUboDeclaration()
{
#ifdef USE_TRANS_STACK_UBO
   return UBO_DECL;
#else 
   return NULL;
#endif
}

// VRV_PATCH
unsigned int State::s_nonSharedContextIDCounter = 0;
// VRV_PATCH

State::State():
    Referenced(true)
{
    _graphicsContext = 0;
    _contextID = 0;

    // VRV_PATCH
    _nonSharedContextID = 0;
    // VRV_PATCH

    _shaderCompositionEnabled = false;
    _shaderCompositionDirty = true;
    _shaderComposer = new ShaderComposer;
    _currentShaderCompositionProgram = 0L;

    _drawBuffer = GL_INVALID_ENUM; // avoid the lazy state mechanism from ignoreing the first call to State::glDrawBuffer() to make sure it's always passed to OpenGL
    _readBuffer = GL_INVALID_ENUM; // avoid the lazy state mechanism from ignoreing the first call to State::glReadBuffer() to make sure it's always passed to OpenGL

    _identity = new osg::RefMatrix(); // default RefMatrix constructs to identity.
    _initialViewMatrix = _identity;
    _projection = _identity;
    _modelView = _identity;
    _modelViewCache = new osg::RefMatrix;

    #if !defined(OSG_GL_FIXED_FUNCTION_AVAILABLE)
        _useModelViewAndProjectionUniforms = true;
        _useVertexAttributeAliasing = true;
    #else
        _useModelViewAndProjectionUniforms = false;
        _useVertexAttributeAliasing = false;
    #endif

#ifndef USE_TRANS_STACK_UBO
    _modelViewMatrixUniform = new Uniform(Uniform::FLOAT_MAT4, "osg_ModelViewMatrix");
    _modelViewMatrixInverseUniform = new Uniform(Uniform::FLOAT_MAT4, "osg_ModelViewMatrixInverse");
    _projectionMatrixUniform = new Uniform(Uniform::FLOAT_MAT4, "osg_ProjectionMatrix");
    _modelViewProjectionMatrixUniform = new Uniform(Uniform::FLOAT_MAT4,"osg_ModelViewProjectionMatrix");
    _normalMatrixUniform = new Uniform(Uniform::FLOAT_MAT3,"osg_NormalMatrix");
    
#endif
    _transformStackUBO = new osg_TransformStack();
    _transformStackID = 0;

    resetVertexAttributeAlias();

    _abortRenderingPtr = NULL;

#if _DEBUG
    _checkGLErrors = ONCE_PER_FRAME;
#else
    _checkGLErrors = NEVER_CHECK_GL_ERRORS;
#endif

    std::string str;
    if (getEnvVar("OSG_GL_ERROR_CHECKING", str))
    {
        if (str=="ONCE_PER_ATTRIBUTE" || str=="ON" || str=="on")
        {
            _checkGLErrors = ONCE_PER_ATTRIBUTE;
        }
        else if (str=="OFF" || str=="off")
        {
            _checkGLErrors = NEVER_CHECK_GL_ERRORS;
        }
    }

    _currentActiveTextureUnit=0;
    _currentClientActiveTextureUnit=0;

    _currentPBO = 0;
    _currentDIBO = 0;
    _currentVAO = 0;

    _isSecondaryColorSupported = false;
    _isFogCoordSupported = false;
    _isVertexBufferObjectSupported = false;
    _isVertexArrayObjectSupported = false;

    if (OSG_GL3_FEATURES)
    {
        _forceVertexBufferObject = true;
        _forceVertexArrayObject = true;
    }
    else 
    {
        _forceVertexBufferObject = false;
        _forceVertexArrayObject = false;
    }

    _lastAppliedProgramObject = 0;
    _lastAppliedFboId = 0;

    _extensionProcsInitialized = false;
    _glClientActiveTexture = 0;
    _glActiveTexture = 0;
    _glFogCoordPointer = 0;
    _glSecondaryColorPointer = 0;
    _glVertexAttribPointer = 0;
    _glVertexAttribIPointer = 0;
    _glVertexAttribLPointer = 0;
    _glEnableVertexAttribArray = 0;
    _glDisableVertexAttribArray = 0;
    _glDrawArraysInstanced = 0;
    _glDrawElementsInstanced = 0;
    _glMultiTexCoord4f = 0;
    _glVertexAttrib4fv = 0;
    _glVertexAttrib4f = 0;
    _glBindBuffer = 0;

    _dynamicObjectCount  = 0;

    _glMaxTextureCoords = 1;
    _glMaxTextureUnits = 1;

    _maxTexturePoolSize = 0;
    _maxBufferObjectPoolSize = 0;

    _arrayDispatchers.setState(this);

    _graphicsCostEstimator = new GraphicsCostEstimator;

    _startTick = 0;
    _gpuTick = 0;
    _gpuTimestamp = 0;
    _timestampBits = 0;

    _vas = 0;
}

State::~State()
{
    // delete the GLExtensions object associated with this osg::State.
    if (_glExtensions)
    {
        if (_transformStackID){
            _glExtensions->glDeleteBuffers(1, &_transformStackID);
        }
       
        // VRV_PATCH
        // TDG: this was causing crashes in GLBufferObject because the extensions were getting deleted
        // this line is not really usefull because we are using shared contexts so they all have a context ID of 0
        // and use the same extensions.
        //GLExtensions::Set(_contextID, 0);
        // VRV_PATCH

        // MAK REVIEW: The above was commented out in makOSG 3.4.  The below is new of 3.6.  Are they compatible?

        _glExtensions = 0;
        GLExtensions* glExtensions = GLExtensions::Get(_contextID, false);
        if (glExtensions && glExtensions->referenceCount() == 1) {
            // the only reference left to the extension is in the static map itself, so we clean it up now
            GLExtensions::Set(_contextID, 0);
        }
    }

    delete _transformStackUBO;
    _transformStackUBO = NULL;
    //_texCoordArrayList.clear();

    //_vertexAttribArrayList.clear();
}

void State::setUseVertexAttributeAliasing(bool flag)
{
    _useVertexAttributeAliasing = flag;
    if (_globalVertexArrayState.valid()) _globalVertexArrayState->assignAllDispatchers();
}

void State::initializeExtensionProcs()
{
    if (_extensionProcsInitialized) return;

    const char* vendor = (const char*) glGetString( GL_VENDOR );
    if (vendor)
    {
        std::string str_vendor(vendor);
        std::replace(str_vendor.begin(), str_vendor.end(), ' ', '_');
        OSG_INFO<<"GL_VENDOR = ["<<str_vendor<<"]"<<std::endl;
        _defineMap.map[str_vendor].defineVec.push_back(osg::StateSet::DefinePair("1",osg::StateAttribute::ON));
        _defineMap.map[str_vendor].changed = true;
        _defineMap.changed = true;
    }

    _glExtensions = GLExtensions::Get(_contextID, true);

    _isSecondaryColorSupported = osg::isGLExtensionSupported(_contextID,"GL_EXT_secondary_color");
    _isFogCoordSupported = osg::isGLExtensionSupported(_contextID,"GL_EXT_fog_coord");
    _isVertexBufferObjectSupported = OSG_GLES2_FEATURES || OSG_GLES3_FEATURES || OSG_GL3_FEATURES || osg::isGLExtensionSupported(_contextID,"GL_ARB_vertex_buffer_object");
    _isVertexArrayObjectSupported = _glExtensions->isVAOSupported;

    const DisplaySettings* ds = getDisplaySettings() ? getDisplaySettings() : osg::DisplaySettings::instance().get();

    if (ds->getVertexBufferHint()==DisplaySettings::VERTEX_BUFFER_OBJECT)
    {
        _forceVertexBufferObject = true;
        _forceVertexArrayObject = false;
    }
    else if (ds->getVertexBufferHint()==DisplaySettings::VERTEX_ARRAY_OBJECT)
    {
        _forceVertexBufferObject = true;
        _forceVertexArrayObject = true;
    }

    OSG_INFO<<"osg::State::initializeExtensionProcs() _forceVertexArrayObject = "<<_forceVertexArrayObject<<std::endl;
    OSG_INFO<<"                                       _forceVertexBufferObject = "<<_forceVertexBufferObject<<std::endl;


    // Set up up global VertexArrayState object
    _globalVertexArrayState = new VertexArrayState(this);
    _globalVertexArrayState->assignAllDispatchers();
    // if (_useVertexArrayObject) _globalVertexArrayState->generateVertexArrayObject();

    setCurrentToGlobalVertexArrayState();


    setGLExtensionFuncPtr(_glClientActiveTexture,"glClientActiveTexture","glClientActiveTextureARB");
    setGLExtensionFuncPtr(_glActiveTexture, "glActiveTexture","glActiveTextureARB");
    setGLExtensionFuncPtr(_glFogCoordPointer, "glFogCoordPointer","glFogCoordPointerEXT");
    setGLExtensionFuncPtr(_glSecondaryColorPointer, "glSecondaryColorPointer","glSecondaryColorPointerEXT");
    setGLExtensionFuncPtr(_glVertexAttribPointer, "glVertexAttribPointer","glVertexAttribPointerARB");
    setGLExtensionFuncPtr(_glVertexAttribIPointer, "glVertexAttribIPointer");
    setGLExtensionFuncPtr(_glVertexAttribLPointer, "glVertexAttribLPointer","glVertexAttribPointerARB");
    setGLExtensionFuncPtr(_glEnableVertexAttribArray, "glEnableVertexAttribArray","glEnableVertexAttribArrayARB");
    setGLExtensionFuncPtr(_glMultiTexCoord4f, "glMultiTexCoord4f","glMultiTexCoord4fARB");
    setGLExtensionFuncPtr(_glVertexAttrib4f, "glVertexAttrib4f");
    setGLExtensionFuncPtr(_glVertexAttrib4fv, "glVertexAttrib4fv");
    setGLExtensionFuncPtr(_glDisableVertexAttribArray, "glDisableVertexAttribArray","glDisableVertexAttribArrayARB");
    setGLExtensionFuncPtr(_glBindBuffer, "glBindBuffer","glBindBufferARB");

    setGLExtensionFuncPtr(_glDrawArraysInstanced, "glDrawArraysInstanced","glDrawArraysInstancedARB","glDrawArraysInstancedEXT");
    setGLExtensionFuncPtr(_glDrawElementsInstanced, "glDrawElementsInstanced","glDrawElementsInstancedARB","glDrawElementsInstancedEXT");

    if (osg::getGLVersionNumber() >= 2.0 || osg::isGLExtensionSupported(_contextID, "GL_ARB_vertex_shader") || OSG_GLES2_FEATURES || OSG_GLES3_FEATURES || OSG_GL3_FEATURES)
    {
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&_glMaxTextureUnits);
        #ifdef OSG_GL_FIXED_FUNCTION_AVAILABLE
            glGetIntegerv(GL_MAX_TEXTURE_COORDS, &_glMaxTextureCoords);
        #else
            _glMaxTextureCoords = _glMaxTextureUnits;
        #endif
    }
    else if ( osg::getGLVersionNumber() >= 1.3 ||
                                 osg::isGLExtensionSupported(_contextID,"GL_ARB_multitexture") ||
                                 osg::isGLExtensionSupported(_contextID,"GL_EXT_multitexture") ||
                                 OSG_GLES1_FEATURES)
    {
        GLint maxTextureUnits = 0;
        glGetIntegerv(GL_MAX_TEXTURE_UNITS,&maxTextureUnits);
        _glMaxTextureUnits = maxTextureUnits;
        _glMaxTextureCoords = maxTextureUnits;
    }
    else
    {
        _glMaxTextureUnits = 1;
        _glMaxTextureCoords = 1;
    }

    if (_glExtensions->isARBTimerQuerySupported)
    {
        const GLubyte* renderer = glGetString(GL_RENDERER);
        std::string rendererString = renderer ? (const char*)renderer : "";
        if (rendererString.find("Radeon")!=std::string::npos || rendererString.find("RADEON")!=std::string::npos || rendererString.find("FirePro")!=std::string::npos)
        {
            // AMD/ATI drivers are producing an invalid enumerate error on the
            // glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS_ARB, &bits);
            // call so work around it by assuming 64 bits for counter.
            setTimestampBits(64);
            //setTimestampBits(0);
        }
        else
        {
            GLint bits = 0;
            _glExtensions->glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS_ARB, &bits);
            setTimestampBits(bits);
        }
    }

    // set the validity of Modes
    {
        bool pointSpriteModeValid = _glExtensions->isPointSpriteModeSupported;

    #if defined( OSG_GLES1_AVAILABLE ) //point sprites don't exist on es 2.0
        setModeValidity(GL_POINT_SPRITE_OES, pointSpriteModeValid);
    #else
        setModeValidity(GL_POINT_SPRITE_ARB, pointSpriteModeValid);
    #endif
    }


    _extensionProcsInitialized = true;

    if (_graphicsCostEstimator.valid())
    {
        RenderInfo renderInfo(this,0);
        _graphicsCostEstimator->calibrate(renderInfo);
    }
}

void State::releaseGLObjects()
{
    // release any GL objects held by the shader composer
    _shaderComposer->releaseGLObjects(this);

    // release any StateSet's on the stack
    for(StateSetStack::iterator itr = _stateStateStack.begin();
        itr != _stateStateStack.end();
        ++itr)
    {
        (*itr)->releaseGLObjects(this);
    }

    _modeMap.clear();
    _textureModeMapList.clear();

    // release any cached attributes
    for(AttributeMap::iterator aitr = _attributeMap.begin();
        aitr != _attributeMap.end();
        ++aitr)
    {
        AttributeStack& as = aitr->second;
        if (as.global_default_attribute.valid())
        {
            as.global_default_attribute->releaseGLObjects(this);
        }
    }
    _attributeMap.clear();

    // release any cached texture attributes
    for(TextureAttributeMapList::iterator itr = _textureAttributeMapList.begin();
        itr != _textureAttributeMapList.end();
        ++itr)
    {
        AttributeMap& attributeMap = *itr;
        for(AttributeMap::iterator aitr = attributeMap.begin();
            aitr != attributeMap.end();
            ++aitr)
        {
            AttributeStack& as = aitr->second;
            if (as.global_default_attribute.valid())
            {
                as.global_default_attribute->releaseGLObjects(this);
            }
        }
    }

    _textureAttributeMapList.clear();
}

void State::reset()
{
    OSG_NOTICE<<std::endl<<"State::reset() *************************** "<<std::endl;

#if 1
    for(ModeMap::iterator mitr=_modeMap.begin();
        mitr!=_modeMap.end();
        ++mitr)
    {
        ModeStack& ms = mitr->second;
        ms.valueVec.clear();
        ms.last_applied_value = !ms.global_default_value;
        ms.changed = true;
        //VRV_PATCH
        //TDG:When dirty all modes or have applied mode is called this flag is used to 
        // make sure that the openGL mode is updated to match the OSG state
        // the next time applyMode is called.
        ms.dirtyDueToExternalApply = true;
        //END VRV_PATCH
    }
#else
    _modeMap.clear();
#endif

    _modeMap[GL_DEPTH_TEST].global_default_value = true;
    _modeMap[GL_DEPTH_TEST].changed = true;

    // go through all active StateAttribute's, setting to change to force update,
    // the idea is to leave only the global defaults left.
    for(AttributeMap::iterator aitr=_attributeMap.begin();
        aitr!=_attributeMap.end();
        ++aitr)
    {
        AttributeStack& as = aitr->second;
        as.attributeVec.clear();
        as.last_applied_attribute = NULL;
        as.last_applied_shadercomponent = NULL;
        as.changed = true;
    }

    // we can do a straight clear, we aren't interested in GL_DEPTH_TEST defaults in texture modes.
    for(TextureModeMapList::iterator tmmItr=_textureModeMapList.begin();
        tmmItr!=_textureModeMapList.end();
        ++tmmItr)
    {
        tmmItr->clear();
    }

    // empty all the texture attributes as per normal attributes, leaving only the global defaults left.
    for(TextureAttributeMapList::iterator tamItr=_textureAttributeMapList.begin();
        tamItr!=_textureAttributeMapList.end();
        ++tamItr)
    {
        AttributeMap& attributeMap = *tamItr;
        // go through all active StateAttribute's, setting to change to force update.
        for(AttributeMap::iterator aitr=attributeMap.begin();
            aitr!=attributeMap.end();
            ++aitr)
        {
            AttributeStack& as = aitr->second;
            as.attributeVec.clear();
            as.last_applied_attribute = NULL;
            as.last_applied_shadercomponent = NULL;
            as.changed = true;
        }
    }

    _stateStateStack.clear();

    _modelView = _identity;
    _projection = _identity;

    dirtyAllVertexArrays();

#if 1
    // reset active texture unit values and call OpenGL
    // note, this OpenGL op precludes the use of State::reset() without a
    // valid graphics context, therefore the new implementation below
    // is preferred.
    setActiveTextureUnit(0);
#else
    // reset active texture unit values without calling OpenGL
    _currentActiveTextureUnit = 0;
    _currentClientActiveTextureUnit = 0;
#endif

    _shaderCompositionDirty = true;
    _currentShaderCompositionUniformList.clear();

    _lastAppliedProgramObject = 0;
    _lastAppliedFboId = 0;

    // what about uniforms??? need to clear them too...
    // go through all active Uniform's, setting to change to force update,
    // the idea is to leave only the global defaults left.
    for(UniformMap::iterator uitr=_uniformMap.begin();
        uitr!=_uniformMap.end();
        ++uitr)
    {
        UniformStack& us = uitr->second;
        us.uniformVec.clear();
    }

}

void State::glDrawBuffer(GLenum buffer)
{
    if (_drawBuffer!=buffer)
    {
        #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE)
        ::glDrawBuffer(buffer);
        #endif
        _drawBuffer=buffer;
    }
}

void State::glReadBuffer(GLenum buffer)
{
    if (_readBuffer!=buffer)
    {
        #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE)
        ::glReadBuffer(buffer);
        #endif
        _readBuffer=buffer;
    }
}

void State::setInitialViewMatrix(const osg::RefMatrix* matrix)
{
    if (matrix) _initialViewMatrix = matrix;
    else _initialViewMatrix = _identity;

    _initialInverseViewMatrix.invert(*_initialViewMatrix);
}

void State::setMaxTexturePoolSize(unsigned int size)
{
    _maxTexturePoolSize = size;
    osg::get<TextureObjectManager>(_contextID)->setMaxTexturePoolSize(size);
    OSG_INFO<<"osg::State::_maxTexturePoolSize="<<_maxTexturePoolSize<<std::endl;
}

void State::setMaxBufferObjectPoolSize(unsigned int size)
{
    _maxBufferObjectPoolSize = size;
    osg::get<GLBufferObjectManager>(_contextID)->setMaxGLBufferObjectPoolSize(_maxBufferObjectPoolSize);
    OSG_INFO<<"osg::State::_maxBufferObjectPoolSize="<<_maxBufferObjectPoolSize<<std::endl;
}

void State::pushStateSet(const StateSet* dstate)
{

    _stateStateStack.push_back(dstate);
    if (dstate)
    {

        pushModeList(_modeMap,dstate->getModeList());

        // iterator through texture modes.
        unsigned int unit;
        const StateSet::TextureModeList& ds_textureModeList = dstate->getTextureModeList();
        for(unit=0;unit<ds_textureModeList.size();++unit)
        {
            pushModeList(getOrCreateTextureModeMap(unit),ds_textureModeList[unit]);
        }

        pushAttributeList(_attributeMap,dstate->getAttributeList());

        // iterator through texture attributes.
        const StateSet::TextureAttributeList& ds_textureAttributeList = dstate->getTextureAttributeList();
        for(unit=0;unit<ds_textureAttributeList.size();++unit)
        {
            pushAttributeList(getOrCreateTextureAttributeMap(unit),ds_textureAttributeList[unit]);
        }

        pushUniformList(_uniformMap,dstate->getUniformList());

        pushDefineList(_defineMap,dstate->getDefineList());
    }

    // OSG_NOTICE<<"State::pushStateSet()"<<_stateStateStack.size()<<std::endl;
}

void State::popAllStateSets()
{
    // OSG_NOTICE<<"State::popAllStateSets()"<<_stateStateStack.size()<<std::endl;

    while (!_stateStateStack.empty()) popStateSet();

    applyProjectionAndModelViewMatrix(0, 0);
//    applyProjectionMatrix(0);
//    applyModelViewMatrix(0);

    _lastAppliedProgramObject = 0;
}

void State::popStateSet()
{
    // OSG_NOTICE<<"State::popStateSet()"<<_stateStateStack.size()<<std::endl;

    if (_stateStateStack.empty()) return;


    const StateSet* dstate = _stateStateStack.back();

    if (dstate)
    {

        popModeList(_modeMap,dstate->getModeList());

        // iterator through texture modes.
        unsigned int unit;
        const StateSet::TextureModeList& ds_textureModeList = dstate->getTextureModeList();
        for(unit=0;unit<ds_textureModeList.size();++unit)
        {
            popModeList(getOrCreateTextureModeMap(unit),ds_textureModeList[unit]);
        }

        popAttributeList(_attributeMap,dstate->getAttributeList());

        // iterator through texture attributes.
        const StateSet::TextureAttributeList& ds_textureAttributeList = dstate->getTextureAttributeList();
        for(unit=0;unit<ds_textureAttributeList.size();++unit)
        {
            popAttributeList(getOrCreateTextureAttributeMap(unit),ds_textureAttributeList[unit]);
        }

        popUniformList(_uniformMap,dstate->getUniformList());

        popDefineList(_defineMap,dstate->getDefineList());

    }

    // remove the top draw state from the stack.
    _stateStateStack.pop_back();
}

void State::insertStateSet(unsigned int pos,const StateSet* dstate)
{
    StateSetStack tempStack;

    // first pop the StateSet above the position we need to insert at
    while (_stateStateStack.size()>pos)
    {
        tempStack.push_back(_stateStateStack.back());
        popStateSet();
    }

    // push our new stateset
    pushStateSet(dstate);

    // push back the original ones
    for(StateSetStack::reverse_iterator itr = tempStack.rbegin();
        itr != tempStack.rend();
        ++itr)
    {
        pushStateSet(*itr);
    }

}

void State::removeStateSet(unsigned int pos)
{
    if (pos >= _stateStateStack.size())
    {
        OSG_NOTICE<<"Warning: State::removeStateSet("<<pos<<") out of range"<<std::endl;
        return;
    }

    // record the StateSet above the one we intend to remove
    StateSetStack tempStack;
    while (_stateStateStack.size()-1>pos)
    {
        tempStack.push_back(_stateStateStack.back());
        popStateSet();
    }

    // remove the intended StateSet as well
    popStateSet();

    // push back the original ones that were above the remove StateSet
    for(StateSetStack::reverse_iterator itr = tempStack.rbegin();
        itr != tempStack.rend();
        ++itr)
    {
        pushStateSet(*itr);
    }
}

void State::captureCurrentState(StateSet& stateset) const
{
    // empty the stateset first.
    stateset.clear();

    for(ModeMap::const_iterator mitr=_modeMap.begin();
        mitr!=_modeMap.end();
        ++mitr)
    {
        // note GLMode = mitr->first
        const ModeStack& ms = mitr->second;
        if (!ms.valueVec.empty())
        {
            stateset.setMode(mitr->first,ms.valueVec.back());
        }
    }

    for(AttributeMap::const_iterator aitr=_attributeMap.begin();
        aitr!=_attributeMap.end();
        ++aitr)
    {
        const AttributeStack& as = aitr->second;
        if (!as.attributeVec.empty())
        {
            stateset.setAttribute(const_cast<StateAttribute*>(as.attributeVec.back().first));
        }
    }

}


void State::apply(const StateSet* dstate)
{
     OsgProfileC("State::apply(const StateSet* dstate)", tracy::Color::Yellow3);

//   marker_series series("State Apply");
//   span UpdateTick(series, 2, "Dif Apply");

    if (_checkGLErrors==ONCE_PER_ATTRIBUTE) checkGLErrors("start of State::apply(StateSet*)");

    // equivalent to:
    //pushStateSet(dstate);
    //apply();
    //popStateSet();
    //return;

    if (dstate)
    {
        // push the stateset on the stack so it can be queried from within StateAttribute
        _stateStateStack.push_back(dstate);

        _currentShaderCompositionUniformList.clear();

        // apply all texture state and modes
        const StateSet::TextureModeList& ds_textureModeList = dstate->getTextureModeList();
        const StateSet::TextureAttributeList& ds_textureAttributeList = dstate->getTextureAttributeList();

        unsigned int unit;
        unsigned int unitMax = maximum(static_cast<unsigned int>(ds_textureModeList.size()),static_cast<unsigned int>(ds_textureAttributeList.size()));
        unitMax = maximum(static_cast<unsigned int>(unitMax),static_cast<unsigned int>(_textureModeMapList.size()));
        unitMax = maximum(static_cast<unsigned int>(unitMax),static_cast<unsigned int>(_textureAttributeMapList.size()));
        for(unit=0;unit<unitMax;++unit)
        {
            if (unit<ds_textureModeList.size()) applyModeListOnTexUnit(unit,getOrCreateTextureModeMap(unit),ds_textureModeList[unit]);
            else if (unit<_textureModeMapList.size()) applyModeMapOnTexUnit(unit,_textureModeMapList[unit]);

            if (unit<ds_textureAttributeList.size()) applyAttributeListOnTexUnit(unit,getOrCreateTextureAttributeMap(unit),ds_textureAttributeList[unit]);
            else if (unit<_textureAttributeMapList.size()) applyAttributeMapOnTexUnit(unit,_textureAttributeMapList[unit]);
        }

        const Program::PerContextProgram* previousLastAppliedProgramObject = _lastAppliedProgramObject;

        applyModeList(_modeMap,dstate->getModeList());
#if 1
        pushDefineList(_defineMap, dstate->getDefineList());
#else
        applyDefineList(_defineMap, dstate->getDefineList());
#endif

        applyAttributeList(_attributeMap,dstate->getAttributeList());

        if ((_lastAppliedProgramObject!=0) && (previousLastAppliedProgramObject==_lastAppliedProgramObject) && _defineMap.changed)
        {
            // OSG_NOTICE<<"State::apply(StateSet*) Program already applied ("<<(previousLastAppliedProgramObject==_lastAppliedProgramObject)<<") and _defineMap.changed= "<<_defineMap.changed<<std::endl;
            _lastAppliedProgramObject->getProgram()->apply(*this);
        }

        if (_shaderCompositionEnabled)
        {
            if (previousLastAppliedProgramObject == _lastAppliedProgramObject || _lastAppliedProgramObject==0)
            {
                // No program has been applied by the StateSet stack so assume shader composition is required
                applyShaderComposition();
            }
        }

        if (dstate->getUniformList().empty())
        {
            if (_currentShaderCompositionUniformList.empty()) applyUniformMap(_uniformMap);
            else applyUniformList(_uniformMap, _currentShaderCompositionUniformList);
        }
        else
        {
            if (_currentShaderCompositionUniformList.empty()) applyUniformList(_uniformMap, dstate->getUniformList());
            else
            {
                // need top merge uniforms lists, but cheat for now by just applying both.
                _currentShaderCompositionUniformList.insert(dstate->getUniformList().begin(), dstate->getUniformList().end());
                applyUniformList(_uniformMap, _currentShaderCompositionUniformList);
            }
        }

#if 1
        popDefineList(_defineMap, dstate->getDefineList());
#endif

        // pop the stateset from the stack
        _stateStateStack.pop_back();
    }
    else
    {
        // no incoming stateset, so simply apply state.
        apply();
    }

    if (_checkGLErrors==ONCE_PER_ATTRIBUTE) checkGLErrors("end of State::apply(StateSet*)");
}


void State::apply()
{
    OsgProfileC("State::apply()", tracy::Color::Yellow3);

    if (_checkGLErrors==ONCE_PER_ATTRIBUTE) checkGLErrors("start of State::apply()");

    _currentShaderCompositionUniformList.clear();

    // apply all texture state and modes
    unsigned int unit;
    unsigned int unitMax = maximum(_textureModeMapList.size(),_textureAttributeMapList.size());
    for(unit=0;unit<unitMax;++unit)
    {
        if (unit<_textureModeMapList.size()) applyModeMapOnTexUnit(unit,_textureModeMapList[unit]);
        if (unit<_textureAttributeMapList.size()) applyAttributeMapOnTexUnit(unit,_textureAttributeMapList[unit]);
    }

    // go through all active OpenGL modes, enabling/disable where
    // appropriate.
    applyModeMap(_modeMap);

    const Program::PerContextProgram* previousLastAppliedProgramObject = _lastAppliedProgramObject;

    // go through all active StateAttribute's, applying where appropriate.
    applyAttributeMap(_attributeMap);


    if ((_lastAppliedProgramObject!=0) && (previousLastAppliedProgramObject==_lastAppliedProgramObject) && _defineMap.changed)
    {
        //OSG_NOTICE<<"State::apply() Program already applied ("<<(previousLastAppliedProgramObject==_lastAppliedProgramObject)<<") and _defineMap.changed= "<<_defineMap.changed<<std::endl;
        if (_lastAppliedProgramObject) _lastAppliedProgramObject->getProgram()->apply(*this);
    }


    if (_shaderCompositionEnabled)
    {
        applyShaderComposition();
    }

    if (_currentShaderCompositionUniformList.empty()) applyUniformMap(_uniformMap);
    else applyUniformList(_uniformMap, _currentShaderCompositionUniformList);

    if (_checkGLErrors==ONCE_PER_ATTRIBUTE) checkGLErrors("end of State::apply()");
}

void State::applyShaderComposition()
{
    if (_shaderCompositionEnabled)
    {
        if (_shaderCompositionDirty)
        {
            // if (isNotifyEnabled(osg::INFO)) print(notify(osg::INFO));

            // build lits of current ShaderComponents
            ShaderComponents shaderComponents;

            // OSG_NOTICE<<"State::applyShaderComposition() : _attributeMap.size()=="<<_attributeMap.size()<<std::endl;

            for(AttributeMap::iterator itr = _attributeMap.begin();
                itr != _attributeMap.end();
                ++itr)
            {
                // OSG_NOTICE<<"  itr->first="<<itr->first.first<<", "<<itr->first.second<<std::endl;

                AttributeStack& as = itr->second;
                if (as.last_applied_shadercomponent)
                {
                    shaderComponents.push_back(const_cast<ShaderComponent*>(as.last_applied_shadercomponent));
                }
            }

            _currentShaderCompositionProgram = _shaderComposer->getOrCreateProgram(shaderComponents);
        }

        if (_currentShaderCompositionProgram)
        {
            Program::PerContextProgram* pcp = _currentShaderCompositionProgram->getPCP(*this);
            if (_lastAppliedProgramObject != pcp) applyAttribute(_currentShaderCompositionProgram);
        }
    }
}


void State::haveAppliedMode(StateAttribute::GLMode mode,StateAttribute::GLModeValue value)
{
    haveAppliedMode(_modeMap,mode,value);
}

void State::haveAppliedMode(StateAttribute::GLMode mode)
{
    haveAppliedMode(_modeMap,mode);
}

void State::haveAppliedAttribute(const StateAttribute* attribute)
{
    haveAppliedAttribute(_attributeMap,attribute);
}

void State::haveAppliedAttribute(StateAttribute::Type type, unsigned int member)
{
    haveAppliedAttribute(_attributeMap,type,member);
}

bool State::getLastAppliedMode(StateAttribute::GLMode mode) const
{
    return getLastAppliedMode(_modeMap,mode);
}

const StateAttribute* State::getLastAppliedAttribute(StateAttribute::Type type, unsigned int member) const
{
    return getLastAppliedAttribute(_attributeMap,type,member);
}


void State::haveAppliedTextureMode(unsigned int unit,StateAttribute::GLMode mode,StateAttribute::GLModeValue value)
{
    haveAppliedMode(getOrCreateTextureModeMap(unit),mode,value);
}

void State::haveAppliedTextureMode(unsigned int unit,StateAttribute::GLMode mode)
{
    haveAppliedMode(getOrCreateTextureModeMap(unit),mode);
}

void State::haveAppliedTextureAttribute(unsigned int unit,const StateAttribute* attribute)
{
    haveAppliedAttribute(getOrCreateTextureAttributeMap(unit),attribute);
}

void State::haveAppliedTextureAttribute(unsigned int unit,StateAttribute::Type type, unsigned int member)
{
    haveAppliedAttribute(getOrCreateTextureAttributeMap(unit),type,member);
}

bool State::getLastAppliedTextureMode(unsigned int unit,StateAttribute::GLMode mode) const
{
    if (unit>=_textureModeMapList.size()) return false;
    return getLastAppliedMode(_textureModeMapList[unit],mode);
}

const StateAttribute* State::getLastAppliedTextureAttribute(unsigned int unit,StateAttribute::Type type, unsigned int member) const
{
    if (unit>=_textureAttributeMapList.size()) return NULL;
    return getLastAppliedAttribute(_textureAttributeMapList[unit],type,member);
}


void State::haveAppliedMode(ModeMap& modeMap,StateAttribute::GLMode mode,StateAttribute::GLModeValue value)
{
    ModeStack& ms = modeMap[mode];

    ms.last_applied_value = value & StateAttribute::ON;

    // will need to disable this mode on next apply so set it to changed.
    ms.changed = true;
    //VRV_PATCH
    //TDG:When dirty all modes or have applied mode is called this flag is used to 
    // make sure that the openGL mode is updated to match the OSG state
    // the next time applyMode is called.
    ms.dirtyDueToExternalApply = true;
    //END VRV_PATCH
}

/** mode has been set externally, update state to reflect this setting.*/
void State::haveAppliedMode(ModeMap& modeMap,StateAttribute::GLMode mode)
{
    ModeStack& ms = modeMap[mode];

    // don't know what last applied value is can't apply it.
    // assume that it has changed by toggle the value of last_applied_value.
    ms.last_applied_value = !ms.last_applied_value;

    // will need to disable this mode on next apply so set it to changed.
    ms.changed = true;
    //VRV_PATCH
    //TDG:When dirty all modes or have applied mode is called this flag is used to 
    // make sure that the openGL mode is updated to match the OSG state
    // the next time applyMode is called.
    ms.dirtyDueToExternalApply = true;
    //END VRV_PATCH
}

/** attribute has been applied externally, update state to reflect this setting.*/
void State::haveAppliedAttribute(AttributeMap& attributeMap,const StateAttribute* attribute)
{
    if (attribute)
    {
        AttributeStack& as = attributeMap[attribute->getTypeMemberPair()];

        as.last_applied_attribute = attribute;

        // will need to update this attribute on next apply so set it to changed.
        as.changed = true;
    }
}

void State::haveAppliedAttribute(AttributeMap& attributeMap,StateAttribute::Type type, unsigned int member)
{

    AttributeMap::iterator itr = attributeMap.find(StateAttribute::TypeMemberPair(type,member));
    if (itr!=attributeMap.end())
    {
        AttributeStack& as = itr->second;
        as.last_applied_attribute = 0L;

        // will need to update this attribute on next apply so set it to changed.
        as.changed = true;
    }
}

bool State::getLastAppliedMode(const ModeMap& modeMap,StateAttribute::GLMode mode) const
{
    ModeMap::const_iterator itr = modeMap.find(mode);
    if (itr!=modeMap.end())
    {
        const ModeStack& ms = itr->second;
        return ms.last_applied_value;
    }
    else
    {
        return false;
    }
}

const StateAttribute* State::getLastAppliedAttribute(const AttributeMap& attributeMap,StateAttribute::Type type, unsigned int member) const
{
    AttributeMap::const_iterator itr = attributeMap.find(StateAttribute::TypeMemberPair(type,member));
    if (itr!=attributeMap.end())
    {
        const AttributeStack& as = itr->second;
        return as.last_applied_attribute;
    }
    else
    {
        return NULL;
    }
}

void State::dirtyAllModes()
{
    for(ModeMap::iterator mitr=_modeMap.begin();
        mitr!=_modeMap.end();
        ++mitr)
    {
        ModeStack& ms = mitr->second;
        ms.last_applied_value = !ms.last_applied_value;
        ms.changed = true;
        //VRV_PATCH
        //TDG:When dirty all modes or have applied mode is called this flag is used to 
        // make sure that the openGL mode is updated to match the OSG state
        // the next time applyMode is called.
        ms.dirtyDueToExternalApply = true;
        //END VRV_PATCH
    }

    for(TextureModeMapList::iterator tmmItr=_textureModeMapList.begin();
        tmmItr!=_textureModeMapList.end();
        ++tmmItr)
    {
        for(ModeMap::iterator mitr=tmmItr->begin();
            mitr!=tmmItr->end();
            ++mitr)
        {
            ModeStack& ms = mitr->second;
            ms.last_applied_value = !ms.last_applied_value;
            ms.changed = true;
            //VRV_PATCH
            //TDG:When dirty all modes or have applied mode is called this flag is used to 
            // make sure that the openGL mode is updated to match the OSG state
            // the next time applyMode is called.
            ms.dirtyDueToExternalApply = true;
            //END VRV_PATCH
        }
    }
}

void State::dirtyAllAttributes()
{
    for(AttributeMap::iterator aitr=_attributeMap.begin();
        aitr!=_attributeMap.end();
        ++aitr)
    {
        AttributeStack& as = aitr->second;
        as.last_applied_attribute = 0;
        as.changed = true;
    }


    for(TextureAttributeMapList::iterator tamItr=_textureAttributeMapList.begin();
        tamItr!=_textureAttributeMapList.end();
        ++tamItr)
    {
        AttributeMap& attributeMap = *tamItr;
        for(AttributeMap::iterator aitr=attributeMap.begin();
            aitr!=attributeMap.end();
            ++aitr)
        {
            AttributeStack& as = aitr->second;
            as.last_applied_attribute = 0;
            as.changed = true;
        }
    }

}


Polytope State::getViewFrustum() const
{
    Polytope cv;
    cv.setToUnitFrustum();
    cv.transformProvidingInverse((*_modelView)*(*_projection));
    return cv;
}


void State::resetVertexAttributeAlias(bool compactAliasing, unsigned int numTextureUnits)
{
    _texCoordAliasList.clear();
    _attributeBindingList.clear();

    if (compactAliasing)
    {
        unsigned int slot = 0;
        setUpVertexAttribAlias(_vertexAlias, slot++, "gl_Vertex","osg_Vertex","vec4 ");
        setUpVertexAttribAlias(_normalAlias, slot++, "gl_Normal","osg_Normal","vec3 ");
        setUpVertexAttribAlias(_colorAlias, slot++, "gl_Color","osg_Color","vec4 ");

        _texCoordAliasList.resize(numTextureUnits);
        for(unsigned int i=0; i<_texCoordAliasList.size(); i++)
        {
            std::stringstream gl_MultiTexCoord;
            std::stringstream osg_MultiTexCoord;
            gl_MultiTexCoord<<"gl_MultiTexCoord"<<i;
            osg_MultiTexCoord<<"osg_MultiTexCoord"<<i;

            setUpVertexAttribAlias(_texCoordAliasList[i], slot++, gl_MultiTexCoord.str(), osg_MultiTexCoord.str(), "vec4 ");
        }

        setUpVertexAttribAlias(_secondaryColorAlias, slot++, "gl_SecondaryColor","osg_SecondaryColor","vec4 ");
        setUpVertexAttribAlias(_fogCoordAlias, slot++, "gl_FogCoord","osg_FogCoord","float ");

    }
    else
    {
        setUpVertexAttribAlias(_vertexAlias,0, "gl_Vertex","osg_Vertex","vec4 ");
        setUpVertexAttribAlias(_normalAlias, 2, "gl_Normal","osg_Normal","vec3 ");
        setUpVertexAttribAlias(_colorAlias, 3, "gl_Color","osg_Color","vec4 ");
        setUpVertexAttribAlias(_secondaryColorAlias, 4, "gl_SecondaryColor","osg_SecondaryColor","vec4 ");
        setUpVertexAttribAlias(_fogCoordAlias, 5, "gl_FogCoord","osg_FogCoord","float ");

        unsigned int base = 8;
        _texCoordAliasList.resize(numTextureUnits);
        for(unsigned int i=0; i<_texCoordAliasList.size(); i++)
        {
            std::stringstream gl_MultiTexCoord;
            std::stringstream osg_MultiTexCoord;
            gl_MultiTexCoord<<"gl_MultiTexCoord"<<i;
            osg_MultiTexCoord<<"osg_MultiTexCoord"<<i;

            setUpVertexAttribAlias(_texCoordAliasList[i], base+i, gl_MultiTexCoord.str(), osg_MultiTexCoord.str(), "vec4 ");
        }
    }
}


void State::disableAllVertexArrays()
{
    disableVertexPointer();
    disableColorPointer();
    disableFogCoordPointer();
    disableNormalPointer();
    disableSecondaryColorPointer();
    disableTexCoordPointersAboveAndIncluding(0);
    disableVertexAttribPointersAboveAndIncluding(0);
}

void State::dirtyAllVertexArrays()
{
    OSG_INFO << "State::dirtyAllVertexArrays()" << std::endl;

    // VRV_PATCH: start
    if (_vas)
    {
       _vas->resetBufferObjectPointers();
    }
    // VRV_PATCH: end
}

bool State::setClientActiveTextureUnit( unsigned int unit )
{
    // if (true)
    if (_currentClientActiveTextureUnit!=unit)
    {
        // OSG_NOTICE<<"State::setClientActiveTextureUnit( "<<unit<<") done"<<std::endl;

        _glClientActiveTexture(GL_TEXTURE0+unit);

        _currentClientActiveTextureUnit = unit;
    }
    else
    {
        //OSG_NOTICE<<"State::setClientActiveTextureUnit( "<<unit<<") not required."<<std::endl;
    }
    return true;
}


unsigned int State::getClientActiveTextureUnit() const
{
    return _currentClientActiveTextureUnit;
}


bool State::checkGLErrors(const char* str1, const char* str2) const
{
    GLenum errorNo = glGetError();
    if (errorNo!=GL_NO_ERROR)
    {
        osg::NotifySeverity notifyLevel = NOTICE; // WARN;
        const char* error = (char*)gluErrorString(errorNo);
        if (error)
        {
            OSG_NOTIFY(notifyLevel)<<"Warning: detected OpenGL error '" << error<<"'";
        }
        else
        {
            OSG_NOTIFY(notifyLevel)<<"Warning: detected OpenGL error number 0x" << std::hex << errorNo << std::dec;
        }

        if (str1 || str2)
        {
            OSG_NOTIFY(notifyLevel)<<" at";
            if (str1) { OSG_NOTIFY(notifyLevel)<<" "<<str1; }
            if (str2) { OSG_NOTIFY(notifyLevel)<<" "<<str2; }
        }
        else
        {
            OSG_NOTIFY(notifyLevel)<<" in osg::State.";
        }

        OSG_NOTIFY(notifyLevel)<< std::endl;

        return true;
    }
    return false;
}

bool State::checkGLErrors(StateAttribute::GLMode mode) const
{
    GLenum errorNo = glGetError();
    if (errorNo!=GL_NO_ERROR)
    {
        const char* error = (char*)gluErrorString(errorNo);
        if (error)
        {
            OSG_NOTIFY(WARN)<<"Warning: detected OpenGL error '"<< error <<"' after applying GLMode 0x"<<hex<<mode<<dec<< std::endl;
        }
        else
        {
            OSG_NOTIFY(WARN)<<"Warning: detected OpenGL error number 0x"<< std::hex << errorNo <<" after applying GLMode 0x"<<hex<<mode<<dec<< std::endl;
        }
        return true;
    }
    return false;
}

bool State::checkGLErrors(const StateAttribute* attribute) const
{
    GLenum errorNo = glGetError();
    if (errorNo!=GL_NO_ERROR)
    {
        const char* error = (char*)gluErrorString(errorNo);
        if (error)
        {
            OSG_NOTIFY(WARN)<<"Warning: detected OpenGL error '"<< error <<"' after applying attribute "<<attribute->className()<<" "<<attribute<< std::endl;
        }
        else
        {
            OSG_NOTIFY(WARN)<<"Warning: detected OpenGL error number 0x"<< std::hex << errorNo <<" after applying attribute "<<attribute->className()<<" "<<attribute<< std::dec << std::endl;
        }

        return true;
    }
    return false;
}


void State::applyModelViewAndProjectionUniformsIfRequired()
{

#ifdef USE_TRANS_STACK_UBO

   GLExtensions * _extensions = GLExtensions::Get(getContextID(), true);
  
   if (_transformStackID == 0){
      _extensions->glGenBuffers(1, &_transformStackID);
      _extensions->glBindBuffer(GL_UNIFORM_BUFFER, _transformStackID);
      if (_extensions->glObjectLabel){
         _extensions->glObjectLabel(GL_BUFFER, _transformStackID, -1, "transform_uniforms");
      }
      _extensions->glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }


   // FIXME matches hard code in program.cpp
   const int PER_DRAW_CALL_INDEX = 3;
   _extensions->glBindBufferBase(GL_UNIFORM_BUFFER, PER_DRAW_CALL_INDEX, _transformStackID);
   _extensions->glBindBuffer(GL_UNIFORM_BUFFER, _transformStackID);
   _extensions->glBufferData(GL_UNIFORM_BUFFER, sizeof(osg_TransformStack), _transformStackUBO, GL_DYNAMIC_DRAW);
   _extensions->glBindBuffer(GL_UNIFORM_BUFFER, 0);

#else    // USE_TRANS_STACK_UBO


    if (!_lastAppliedProgramObject) return;
    
    // vantage change, seeing lots of spew from gl debug layer, bind just in case
    _lastAppliedProgramObject->useProgram();

    if (_modelViewMatrixUniform.valid()) _lastAppliedProgramObject->apply(*_modelViewMatrixUniform);
    if (_modelViewMatrixInverseUniform.valid()) _lastAppliedProgramObject->apply(*_modelViewMatrixInverseUniform);
    if (_projectionMatrixUniform) _lastAppliedProgramObject->apply(*_projectionMatrixUniform);
    if (_modelViewProjectionMatrixUniform) _lastAppliedProgramObject->apply(*_modelViewProjectionMatrixUniform);
    if (_normalMatrixUniform) _lastAppliedProgramObject->apply(*_normalMatrixUniform);
#endif
}

namespace State_Utils
{
    bool replace(std::string& str, const std::string& original_phrase, const std::string& new_phrase)
    {
        // Prevent infinite loop : if original_phrase is empty, do nothing and return false
        if (original_phrase.empty()) return false;

        bool replacedStr = false;
        std::string::size_type pos = 0;
        while((pos=str.find(original_phrase, pos))!=std::string::npos)
        {
            std::string::size_type endOfPhrasePos = pos+original_phrase.size();
            if (endOfPhrasePos<str.size())
            {
                char c = str[endOfPhrasePos];
                if ((c>='0' && c<='9') ||
                    (c>='a' && c<='z') ||
                    (c>='A' && c<='Z'))
                {
                    pos = endOfPhrasePos;
                    continue;
                }
            }

            replacedStr = true;
            str.replace(pos, original_phrase.size(), new_phrase);
        }
        return replacedStr;
    }

    void replaceAndInsertDeclaration(std::string& source, std::string::size_type declPos, const std::string& originalStr, const std::string& newStr, const std::string& qualifier, const std::string& declarationPrefix, const std::string& declarationSuffix = "")
    {
        if (replace(source, originalStr, newStr))
        {
            source.insert(declPos, qualifier + declarationPrefix + newStr + declarationSuffix + std::string(";\n"));
        }
    }

    void replaceVar(const osg::State& state, std::string& str, std::string::size_type start_pos,  std::string::size_type num_chars)
    {
        std::string var_str(str.substr(start_pos+1, num_chars-1));
        std::string value;
        if (state.getActiveDisplaySettings()->getValue(var_str, value))
        {
            str.replace(start_pos, num_chars, value);
        }
        else
        {
            str.erase(start_pos, num_chars);
        }
    }


    void substitudeEnvVars(const osg::State& state, std::string& str)
    {
        std::string::size_type pos = 0;
        while (pos<str.size() && ((pos=str.find_first_of("$'\"", pos)) != std::string::npos))
        {
            if (pos==str.size())
            {
                break;
            }

            if (str[pos]=='"' || str[pos]=='\'')
            {
                std::string::size_type start_quote = pos;
                ++pos; // skip over first quote
                pos = str.find(str[start_quote], pos);

                if (pos!=std::string::npos)
                {
                    ++pos; // skip over second quote
                }
            }
            else
            {
                std::string::size_type start_var = pos;
                ++pos;
                pos = str.find_first_not_of("ABCDEFGHIJKLMNOPQRTSUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_", pos);
                if (pos != std::string::npos)
                {

                    replaceVar(state, str, start_var, pos-start_var);
                    pos = start_var;
                }
                else
                {
                    replaceVar(state, str, start_var, str.size()-start_var);
                    pos = start_var;
                }
            }
        }
    }
}

bool State::convertShaderSourceToOsgBuiltIns(osg::Shader::Type shader_type, std::string& source) const
{
    OSG_DEBUG<<"State::convertShaderSourceToOsgBuiltIns()"<<std::endl;

    OSG_DEBUG<<"++Before Converted source "<<std::endl<<source<<std::endl<<"++++++++"<<std::endl;


    State_Utils::substitudeEnvVars(*this, source);


    std::string attributeQualifier("attribute ");

    int add_ubo = 0;
    // find the first legal insertion point for replacement declarations. GLSL requires that nothing
    // precede a "#version" compiler directive, so we must insert new declarations after it.

    // GNP- 4.5 is unhappy with anything before "#extension" compiler directive
    std::string::size_type declPos = source.rfind( "#extension ");
    if (declPos == std::string::npos) {
       declPos = source.rfind("#version ");
    }

    if ( declPos != std::string::npos )
    {
        declPos = source.find(" ", declPos); // move to the first space after "#version"
        declPos = source.find_first_not_of(std::string(" "), declPos); // skip all the spaces until you reach the version number
        std::string versionNumber(source, declPos, 3);
        int glslVersion = atoi(versionNumber.c_str());
        OSG_INFO<<"shader version found: "<< glslVersion <<std::endl;
        if (glslVersion >= 130) attributeQualifier = "in ";
        // found the string, now find the next linefeed and set the insertion point after it.
        declPos = source.find( '\n', declPos );
        declPos = declPos != std::string::npos ? declPos+1 : source.length();
        add_ubo = 1;
    }
    else
    {
        declPos = 0;
		// VRV_PATCH for UBO support
        std::string vnum = "#version 150 compatibility\n";
        source.insert(0, vnum);
        declPos = vnum.length();
        add_ubo = 1;
    }
    std::string notice = "// Dynamically added by osg::State::convertVertexShaderSourceToOsgBuiltIns()\n";
    source.insert(declPos, notice);
    declPos += notice.length();

#ifdef USE_TRANS_STACK_UBO
    if (source.find("struct osg_TransformStack") != std::string::npos) {
       add_ubo = 0;
    }
    if (add_ubo){
       source.insert(declPos, UBO_DECL);
    }
#endif

    std::string::size_type extPos = source.rfind( "#extension " );
    if ( extPos != std::string::npos )
    {
        // found the string, now find the next linefeed and set the insertion point after it.
        declPos = source.find( '\n', extPos );
        declPos = declPos != std::string::npos ? declPos+1 : source.length();
    }
    if (_useModelViewAndProjectionUniforms)
    {
        // replace ftransform as it only works with built-ins
        State_Utils::replace(source, "ftransform()", "gl_ModelViewProjectionMatrix * gl_Vertex");

        // materials
        State_Utils::replace(source, "gl_FrontMaterial", "osg_FrontMaterial");
        
        // replace built in uniform
        // do gl_ModelViewMatrixInverse to avoid conflicts with gl_ModelViewMatrix
#ifdef USE_TRANS_STACK_UBO
        State_Utils::replace(source, "gl_ModelViewMatrixInverse", "osg.ModelViewMatrixInverse");
        State_Utils::replace(source, "gl_ModelViewMatrix", "osg.ModelViewMatrix");
        State_Utils::replace(source, "gl_ModelViewProjectionMatrix", "osg.ModelViewProjectionMatrix");
        State_Utils::replace(source, "gl_ProjectionMatrix", "osg.ProjectionMatrix");
        State_Utils::replace(source, "gl_NormalMatrix", "osg.NormalMatrix");
#else
        State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_ModelViewMatrixInverse", "osg_ModelViewMatrixInverse", "uniform mat4 ", "");
        State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_ModelViewMatrix", "osg_ModelViewMatrix", "uniform mat4 ", "");
        State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_ModelViewProjectionMatrix", "osg_ModelViewProjectionMatrix", "uniform mat4 ", "");
        State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_ProjectionMatrix", "osg_ProjectionMatrix", "uniform mat4 ", "");
        State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_NormalMatrix", "osg_NormalMatrix", "uniform mat3 ", "");
#endif
    }

    if (_useVertexAttributeAliasing && shader_type == Shader::VERTEX)
    {
        State_Utils::replaceAndInsertDeclaration(source, declPos, _vertexAlias._glName,         _vertexAlias._osgName,         attributeQualifier, _vertexAlias._declaration);
        State_Utils::replaceAndInsertDeclaration(source, declPos, _normalAlias._glName,         _normalAlias._osgName,         attributeQualifier, _normalAlias._declaration);
        State_Utils::replaceAndInsertDeclaration(source, declPos, _colorAlias._glName,          _colorAlias._osgName,          attributeQualifier, _colorAlias._declaration);
        State_Utils::replaceAndInsertDeclaration(source, declPos, _secondaryColorAlias._glName, _secondaryColorAlias._osgName, attributeQualifier, _secondaryColorAlias._declaration);
        State_Utils::replaceAndInsertDeclaration(source, declPos, _fogCoordAlias._glName,       _fogCoordAlias._osgName,       attributeQualifier, _fogCoordAlias._declaration);
        for (size_t i=0; i<_texCoordAliasList.size(); i++)
        {
            const VertexAttribAlias& texCoordAlias = _texCoordAliasList[i];
            State_Utils::replaceAndInsertDeclaration(source, declPos, texCoordAlias._glName, texCoordAlias._osgName, attributeQualifier, texCoordAlias._declaration);
        }
    }

    State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_ObjectPlaneS", "osg_ObjectPlaneS", "uniform vec4 ", "[8]");
    State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_ObjectPlaneT", "osg_ObjectPlaneT", "uniform vec4 ", "[8]");
    State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_ObjectPlaneR", "osg_ObjectPlaneR", "uniform vec4 ", "[8]");
    State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_ObjectPlaneQ", "osg_ObjectPlaneQ", "uniform vec4 ", "[8]");

    State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_EyePlaneS", "osg_EyePlaneS", "uniform vec4 ", "[8]");
    State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_EyePlaneT", "osg_EyePlaneT", "uniform vec4 ", "[8]");
    State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_EyePlaneR", "osg_EyePlaneR", "uniform vec4 ", "[8]");
    State_Utils::replaceAndInsertDeclaration(source, declPos, "gl_EyePlaneQ", "osg_EyePlaneQ", "uniform vec4 ", "[8]");


    OSG_INFO<<"-------- Converted source "<<std::endl<<source<<std::endl<<"----------------"<<std::endl;

    return true;
}

void State::setUpVertexAttribAlias(VertexAttribAlias& alias, GLuint location, const std::string glName, const std::string osgName, const std::string& declaration)
{
    alias = VertexAttribAlias(location, glName, osgName, declaration);
    _attributeBindingList[osgName] = location;
    // OSG_NOTICE<<"State::setUpVertexAttribAlias("<<location<<" "<<glName<<" "<<osgName<<")"<<std::endl;
}

void State::applyProjectionAndModelViewMatrix(const osg::RefMatrix* projection_matrix, const osg::RefMatrix*model_view_matrix)
{
   if (_projection != projection_matrix)
   {
      if (projection_matrix)
      {
         _projection = projection_matrix;
      }
      else
      {
         _projection = _identity;
      }
#ifdef   USE_TRANS_STACK_UBO
      _transformStackUBO->ProjectionMatrix = *_projection;
#else
      if (_projectionMatrixUniform.valid()) {
         _projectionMatrixUniform->set(*_projection);
      }
#endif
   }

   if (_modelView != model_view_matrix)
   {
      if (model_view_matrix)
      {
         _modelView = model_view_matrix;
      }
      else
      {
         _modelView = _identity;
      }

#ifdef  USE_TRANS_STACK_UBO
      _transformStackUBO->ModelViewMatrix = *_modelView;
      _transformStackUBO->ModelViewMatrixInverse = osg::RefMatrix::inverse(*_modelView);
#else
      if (_modelViewMatrixUniform.valid()){
         _modelViewMatrixUniform->set(*_modelView);
      }
      if (_modelViewMatrixInverseUniform.valid()){
         _modelViewMatrixInverseUniform->set(osg::RefMatrix::inverse(*_modelView));
      }
#endif
   }

   if (_useModelViewAndProjectionUniforms)
   {
      updateModelViewAndProjectionMatrixUniforms();
   }

}

void State::applyProjectionMatrix(const osg::RefMatrix* matrix)
{
    if (_projection!=matrix)
    {
        if (matrix)
        {
            _projection=matrix;
        }
        else
        {
            _projection=_identity;
        }

#ifdef  USE_TRANS_STACK_UBO
        _transformStackUBO->ProjectionMatrix = *_projection;
        updateModelViewAndProjectionMatrixUniforms();
#else
        if (_useModelViewAndProjectionUniforms)
        {
            if (_projectionMatrixUniform.valid()) _projectionMatrixUniform->set(*_projection);
            updateModelViewAndProjectionMatrixUniforms();
        }
#endif 

#ifdef OSG_GL_MATRICES_AVAILABLE
        glMatrixMode( GL_PROJECTION );
            glLoadMatrix(_projection->ptr());
        glMatrixMode( GL_MODELVIEW );
#endif
    }
}

void State::loadModelViewMatrix()
{

    if (_useModelViewAndProjectionUniforms)
    {
#ifdef USE_TRANS_STACK_UBO
       _transformStackUBO->ModelViewMatrix = *_modelView;
       _transformStackUBO->ModelViewMatrixInverse = osg::RefMatrix::inverse(*_modelView);
#else
       if (_modelViewMatrixUniform.valid()){
          _modelViewMatrixUniform->set(*_modelView);
       }
       if (_modelViewMatrixInverseUniform.valid()){
          _modelViewMatrixInverseUniform->set(osg::RefMatrix::inverse(*_modelView));
       }
#endif
       updateModelViewAndProjectionMatrixUniforms();
    }

#ifdef OSG_GL_MATRICES_AVAILABLE
    glLoadMatrix(_modelView->ptr());
#endif
}

void State::applyModelViewMatrix(const osg::RefMatrix* matrix)
{
    if (_modelView!=matrix)
    {
        if (matrix)
        {
            _modelView=matrix;
        }
        else
        {
            _modelView=_identity;
        }

        loadModelViewMatrix();
    }
}

void State::applyModelViewMatrix(const osg::Matrix& matrix)
{
    _modelViewCache->set(matrix);
    _modelView = _modelViewCache;

    loadModelViewMatrix();
}

#include <osg/io_utils>

void State::updateModelViewAndProjectionMatrixUniforms()
{
#ifdef  USE_TRANS_STACK_UBO
   _transformStackUBO->ModelViewProjectionMatrix = (*_modelView) * (*_projection);
#else 
   if (_modelViewProjectionMatrixUniform.valid()) {
      _modelViewProjectionMatrixUniform->set((*_modelView) * (*_projection));
   }
#endif
   Matrix mv(*_modelView);
   mv.setTrans(0.0, 0.0, 0.0);

   Matrix matrix;
   matrix.invert(mv);

#ifdef  USE_TRANS_STACK_UBO
   _transformStackUBO->NormalMatrix.set(
      matrix(0, 0), matrix(1, 0), matrix(2, 0), 0,
      matrix(0, 1), matrix(1, 1), matrix(2, 1), 0,
      matrix(0, 2), matrix(1, 2), matrix(2, 2), 0,
      0,0,0,1);
#else 
   if (_normalMatrixUniform.valid())
   {
      _normalMatrixUniform->set(Matrix3(
         matrix(0, 0), matrix(1, 0), matrix(2, 0),
         matrix(0, 1), matrix(1, 1), matrix(2, 1),
         matrix(0, 2), matrix(1, 2), matrix(2, 2)));
   }
#endif 
}

void State::drawQuads(GLint first, GLsizei count, GLsizei primCount)
{
    // OSG_NOTICE<<"State::drawQuads("<<first<<", "<<count<<")"<<std::endl;

    unsigned int array = first % 4;
    unsigned int offsetFirst = ((first-array) / 4) * 6;
    unsigned int numQuads = (count/4);
    unsigned int numIndices = numQuads * 6;
    unsigned int endOfIndices = offsetFirst+numIndices;

    if (endOfIndices<65536)
    {
        IndicesGLushort& indices = _quadIndicesGLushort[array];

        if (endOfIndices >= indices.size())
        {
            // we need to expand the _indexArray to be big enough to cope with all the quads required.
            unsigned int numExistingQuads = indices.size()/6;
            unsigned int numRequiredQuads = endOfIndices/6;
            indices.reserve(endOfIndices);
            for(unsigned int i=numExistingQuads; i<numRequiredQuads; ++i)
            {
                unsigned int base = i*4 + array;
                indices.push_back(base);
                indices.push_back(base+1);
                indices.push_back(base+3);

                indices.push_back(base+1);
                indices.push_back(base+2);
                indices.push_back(base+3);

                // OSG_NOTICE<<"   adding quad indices ("<<base<<")"<<std::endl;
            }
        }

        // if (array!=0) return;

        // OSG_NOTICE<<"  glDrawElements(GL_TRIANGLES, "<<numIndices<<", GL_UNSIGNED_SHORT, "<<&(indices[base])<<")"<<std::endl;
        glDrawElementsInstanced(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, &(indices[offsetFirst]), primCount);
    }
    else
    {
        IndicesGLuint& indices = _quadIndicesGLuint[array];

        if (endOfIndices >= indices.size())
        {
            // we need to expand the _indexArray to be big enough to cope with all the quads required.
            unsigned int numExistingQuads = indices.size()/6;
            unsigned int numRequiredQuads = endOfIndices/6;
            indices.reserve(endOfIndices);
            for(unsigned int i=numExistingQuads; i<numRequiredQuads; ++i)
            {
                unsigned int base = i*4 + array;
                indices.push_back(base);
                indices.push_back(base+1);
                indices.push_back(base+3);

                indices.push_back(base+1);
                indices.push_back(base+2);
                indices.push_back(base+3);

                // OSG_NOTICE<<"   adding quad indices ("<<base<<")"<<std::endl;
            }
        }

        // if (array!=0) return;

        // OSG_NOTICE<<"  glDrawElements(GL_TRIANGLES, "<<numIndices<<", GL_UNSIGNED_SHORT, "<<&(indices[base])<<")"<<std::endl;
        glDrawElementsInstanced(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, &(indices[offsetFirst]), primCount);
    }
}

void State::ModeStack::print(std::ostream& fout) const
{
    fout<<"    valid = "<<valid<<std::endl;
    fout<<"    changed = "<<changed<<std::endl;
    fout<<"    last_applied_value = "<<last_applied_value<<std::endl;
    fout<<"    global_default_value = "<<global_default_value<<std::endl;
    fout<<"    valueVec { "<<std::endl;
    for(ModeStack::ValueVec::const_iterator itr = valueVec.begin();
        itr != valueVec.end();
        ++itr)
    {
        if (itr!=valueVec.begin()) fout<<", ";
        fout<<*itr;
    }
    fout<<" }"<<std::endl;
}

void State::AttributeStack::print(std::ostream& fout) const
{
    fout<<"    changed = "<<changed<<std::endl;
    fout<<"    last_applied_attribute = "<<last_applied_attribute;
    if (last_applied_attribute) fout<<", "<<last_applied_attribute->className()<<", "<<last_applied_attribute->getName()<<std::endl;
    fout<<"    last_applied_shadercomponent = "<<last_applied_shadercomponent<<std::endl;
    if (last_applied_shadercomponent)  fout<<", "<<last_applied_shadercomponent->className()<<", "<<last_applied_shadercomponent->getName()<<std::endl;
    fout<<"    global_default_attribute = "<<global_default_attribute.get()<<std::endl;
    fout<<"    attributeVec { ";
    for(AttributeVec::const_iterator itr = attributeVec.begin();
        itr != attributeVec.end();
        ++itr)
    {
        if (itr!=attributeVec.begin()) fout<<", ";
        fout<<"("<<itr->first<<", "<<itr->second<<")";
    }
    fout<<" }"<<std::endl;
}


void State::UniformStack::print(std::ostream& fout) const
{
    fout<<"    UniformVec { ";
    for(UniformVec::const_iterator itr = uniformVec.begin();
        itr != uniformVec.end();
        ++itr)
    {
        if (itr!=uniformVec.begin()) fout<<", ";
        fout<<"("<<itr->first<<", "<<itr->second<<")";
    }
    fout<<" }"<<std::endl;
}





void State::print(std::ostream& fout) const
{
#if 0
        GraphicsContext*            _graphicsContext;
        unsigned int                _contextID;
        bool                            _shaderCompositionEnabled;
        bool                            _shaderCompositionDirty;
        osg::ref_ptr<ShaderComposer>    _shaderComposer;
#endif

#if 0
        osg::Program*                   _currentShaderCompositionProgram;
        StateSet::UniformList           _currentShaderCompositionUniformList;
#endif

#if 0
        ref_ptr<FrameStamp>         _frameStamp;

        ref_ptr<const RefMatrix>    _identity;
        ref_ptr<const RefMatrix>    _initialViewMatrix;
        ref_ptr<const RefMatrix>    _projection;
        ref_ptr<const RefMatrix>    _modelView;
        ref_ptr<RefMatrix>          _modelViewCache;

        bool                        _useModelViewAndProjectionUniforms;
        ref_ptr<Uniform>            _modelViewMatrixUniform;
        ref_ptr<Uniform>            _projectionMatrixUniform;
        ref_ptr<Uniform>            _modelViewProjectionMatrixUniform;
        ref_ptr<Uniform>            _normalMatrixUniform;

        Matrix                      _initialInverseViewMatrix;

        ref_ptr<DisplaySettings>    _displaySettings;

        bool*                       _abortRenderingPtr;
        CheckForGLErrors            _checkGLErrors;


        bool                        _useVertexAttributeAliasing;
        VertexAttribAlias           _vertexAlias;
        VertexAttribAlias           _normalAlias;
        VertexAttribAlias           _colorAlias;
        VertexAttribAlias           _secondaryColorAlias;
        VertexAttribAlias           _fogCoordAlias;
        VertexAttribAliasList       _texCoordAliasList;

        Program::AttribBindingList  _attributeBindingList;
#endif
        fout<<"ModeMap _modeMap {"<<std::endl;
        for(ModeMap::const_iterator itr = _modeMap.begin();
            itr != _modeMap.end();
            ++itr)
        {
            fout<<"  GLMode="<<itr->first<<", ModeStack {"<<std::endl;
            itr->second.print(fout);
            fout<<"  }"<<std::endl;
        }
        fout<<"}"<<std::endl;

        fout<<"AttributeMap _attributeMap {"<<std::endl;
        for(AttributeMap::const_iterator itr = _attributeMap.begin();
            itr != _attributeMap.end();
            ++itr)
        {
            fout<<"  TypeMemberPaid=("<<itr->first.first<<", "<<itr->first.second<<") AttributeStack {"<<std::endl;
            itr->second.print(fout);
            fout<<"  }"<<std::endl;
        }
        fout<<"}"<<std::endl;

        fout<<"UniformMap _uniformMap {"<<std::endl;
        for(UniformMap::const_iterator itr = _uniformMap.begin();
            itr != _uniformMap.end();
            ++itr)
        {
            fout<<"  name="<<itr->first<<", UniformStack {"<<std::endl;
            itr->second.print(fout);
            fout<<"  }"<<std::endl;
        }
        fout<<"}"<<std::endl;


        fout<<"StateSetStack _stateSetStack {"<<std::endl;
        for(StateSetStack::const_iterator itr = _stateStateStack.begin();
            itr != _stateStateStack.end();
            ++itr)
        {
            fout<<(*itr)->getName()<<"  "<<*itr<<std::endl;
        }
        fout<<"}"<<std::endl;
}

void State::frameCompleted()
{
    if (getTimestampBits())
    {
        GLint64 timestamp;
        _glExtensions->glGetInteger64v(GL_TIMESTAMP, &timestamp);
        setGpuTimestamp(osg::Timer::instance()->tick(), timestamp);
        //OSG_NOTICE<<"State::frameCompleted() setting time stamp. timestamp="<<timestamp<<std::endl;
    }
}

bool State::DefineMap::updateCurrentDefines()
{
    // VRV_PATCH
    OsgProfileC("State::DefineMap::updateCurrentDefines()", tracy::Color::Yellow3);

    currentDefines.clear();
    for (DefineStackMap::const_iterator itr = map.begin();
        itr != map.end();
        ++itr)
    {
        const DefineStack::DefineVec& dv = itr->second.defineVec;
        if (!dv.empty())
        {
            const StateSet::DefinePair& dp = dv.back();
            if (dp.second & osg::StateAttribute::ON)
            {
                currentDefines[itr->first] = dp;
            }
        }
    }

    //VRVantage patch to improve performance, all defines are always passed to the shader
    char defineBuf[4096];
    int idx = 0;
    StateSet::DefineList::const_iterator cd_itr = currentDefines.begin();
    StateSet::DefineList::const_iterator cd_nd = currentDefines.end();
    for(;cd_itr != cd_nd; cd_itr++)
    {
        const StateSet::DefinePair& dp = cd_itr->second;
        idx += sprintf(defineBuf + idx, "#define %s", cd_itr->first.c_str());
        if (!dp.first.empty())
        {
            idx += sprintf(defineBuf + idx, " %s", dp.first.c_str());
        }
        //patch for fixing defines on windows         
#ifdef WIN32
        idx += sprintf(defineBuf + idx, "\r\n");
#else
        idx += sprintf(defineBuf + idx, "\n");
#endif

    }
    shaderDefineString = defineBuf;
    changed = false;
    return true;
}

// VRV_PATCH BEGIN
bool State::defineMapChanged(const State::DefineMap& curr)
{
   const State::DefineMap::DefineStackMap& prevMap = _lastAppliedDefineMap.map;
   const State::DefineMap::DefineStackMap& currMap = curr.map;

   if (currMap.size() != prevMap.size())
   {
      return true; // yup, the defines are different
   }

   State::DefineMap::DefineStackMap::const_iterator currItr = currMap.begin();
   State::DefineMap::DefineStackMap::const_iterator prevItr = prevMap.begin();
   State::DefineMap::DefineStackMap::const_iterator currEnd = currMap.end();
   State::DefineMap::DefineStackMap::const_iterator prevEnd = prevMap.end();

   for (; currItr != currEnd && prevItr != prevEnd;
      ++currItr, ++prevItr)
   {
      const State::DefineStack::DefineVec& currDV = currItr->second.defineVec;
      const State::DefineStack::DefineVec& prevDV = prevItr->second.defineVec;

      const unsigned int currVecSize = currDV.size();

      // Names match?
      if (currItr->first != prevItr->first)
      {
         return true;
      }

      if (currVecSize != prevDV.size())
      {
         return true;
      }

      for (unsigned int i = 0; i < currVecSize; ++i)
      {
         const StateSet::DefinePair& currPair = currDV[i];
         const StateSet::DefinePair& prevPair = prevDV[i];

         if (currPair.first != prevPair.first)
         {
            return true;
         }

         if (currPair.second != prevPair.second)
         {
            return true;
         }
      }
   }

   return false; // no change
}
// VRV_PATCH END

const std::string & State::getDefineString(const osg::ShaderDefines& shaderDefines)
{
   // VRV_PATCH BEGIN
   if (_defineMap.changed && defineMapChanged(_defineMap))
   // VRV_PATCH END
   {
      _defineMap.updateCurrentDefines();
      // VRV PATCH BEGIN
      _lastAppliedDefineMap = _defineMap;
      // VRV PATCH END
   }

   // VRV PATCH BEGIN
   // If the defines are all up to date then they are not changed
   // Not having this was causing State::apply to misbehave in the
   // "&& _defineMap.changed" part of the check for getProgram()->apply
   // line 636 and it would result in glUseProgram being called many times
   _defineMap.changed = false;
   // VRV_PATCH END

   //VRVantage patch to improve performance, all defines are always passed to the shader
   return _defineMap.shaderDefineString;
}

bool State::supportsShaderRequirements(const osg::ShaderDefines& shaderRequirements)
{
    if (shaderRequirements.empty()) return true;

    if (_defineMap.changed) _defineMap.updateCurrentDefines();

    const StateSet::DefineList& currentDefines = _defineMap.currentDefines;
    for(ShaderDefines::const_iterator sr_itr = shaderRequirements.begin();
        sr_itr != shaderRequirements.end();
        ++sr_itr)
    {
        if (currentDefines.find(*sr_itr)==currentDefines.end()) return false;
    }
    return true;
}

bool State::supportsShaderRequirement(const std::string& shaderRequirement)
{
    if (_defineMap.changed) _defineMap.updateCurrentDefines();
    const StateSet::DefineList& currentDefines = _defineMap.currentDefines;
    return (currentDefines.find(shaderRequirement)!=currentDefines.end());
}

bool 
State::getUseUboTransformStack()
{
#ifdef USE_TRANS_STACK_UBO
   return true;
#endif
   return false;
}

// VRV_PATCH
void
State::setContextID(unsigned int contextID)
{ 
   _contextID = contextID; 
   _nonSharedContextID = s_nonSharedContextIDCounter++;
}
// VRV_PATCH
