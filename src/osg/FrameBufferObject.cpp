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

// initial FBO support written by Marco Jez, June 2005.

#include <osg/FrameBufferObject>
#include <osg/State>
#include <osg/GLExtensions>
#include <osg/Texture1D>
#include <osg/Texture2D>
#include <osg/Texture2DMultisample>
#include <osg/Texture3D>
#include <osg/Texture2DArray>
#include <osg/Texture2DMultisampleArray>
#include <osg/TextureCubeMap>
#include <osg/TextureRectangle>
#include <osg/Notify>
#include <osg/ContextData>
#include <osg/Timer>

// VRV_PATCH: start
#define OSG_FBO_DEBUG 0

#if OSG_FBO_DEBUG
#if defined(__linux__)
#include "GL/glx.h"
#else
#include <windows.h>
#endif
#include <iostream>
#endif
// VRV_PATCH: end
using namespace osg;


// VRV_PATCH: start
GLRenderBufferManager::GLRenderBufferManager(unsigned int nonSharedContextID):
    GLObjectManager("GLRenderBufferManager", nonSharedContextID)
{}

void GLRenderBufferManager::deleteGLObject(GLuint globj)
{
    const GLExtensions* extensions = GLExtensions::Get(_contextID,true);

#if OSG_FBO_DEBUG
#if defined(WIN32) || defined(WIN64)
    HGLRC currentGlContext = wglGetCurrentContext();
#else
    GLXContext currentGlContext = glXGetCurrentContext();
#endif
    std::cout << "deleted rbo: " << globj << " , context: " << currentGlContext << std::endl;
#endif
    if (extensions->isGlslSupported) extensions->glDeleteRenderbuffers(1, &globj );
}

GLFrameBufferObjectManager::GLFrameBufferObjectManager(unsigned int nonSharedContextID):
    GLObjectManager("GLFrameBufferObjectManager", nonSharedContextID)
{}

void GLFrameBufferObjectManager::deleteGLObject(GLuint globj)
{
#if OSG_FBO_DEBUG
#if defined(WIN32) || defined(WIN64)
   HGLRC currentGlContext = wglGetCurrentContext();
#else
   GLXContext currentGlContext = glXGetCurrentContext();
#endif
    std::cout << "deleted fbo: " << globj << " , context: " << currentGlContext << std::endl;
#endif
    const GLExtensions* extensions = GLExtensions::Get(_contextID,true);
    if (extensions->isGlslSupported) extensions->glDeleteFramebuffers(1, &globj );
}
// VRV_PATCH: end


/**************************************************************************
 * RenderBuffer
 **************************************************************************/

RenderBuffer::RenderBuffer()
:    Object(),
    _internalFormat(GL_DEPTH_COMPONENT24),
    _width(512),
    _height(512),
    _samples(0),
    _colorSamples(0)
{
}

RenderBuffer::RenderBuffer(int width, int height, GLenum internalFormat, int samples, int colorSamples)
:    Object(),
    _internalFormat(internalFormat),
    _width(width),
    _height(height),
    _samples(samples),
    _colorSamples(colorSamples)
{
}

RenderBuffer::RenderBuffer(const RenderBuffer &copy, const CopyOp &copyop)
:    Object(copy, copyop),
    _internalFormat(copy._internalFormat),
    _width(copy._width),
    _height(copy._height),
    _samples(copy._samples),
    _colorSamples(copy._colorSamples)
{
}

RenderBuffer::~RenderBuffer()
{
    for(unsigned i=0; i<_objectID.size(); ++i)
    {
        if (_objectID[i]) osg::get<GLRenderBufferManager>(i)->scheduleGLObjectForDeletion(_objectID[i]);
    }
}

int RenderBuffer::getMaxSamples(unsigned int contextID, const GLExtensions* ext)
{
    static osg::buffered_value<GLint> maxSamplesList;

    GLint& maxSamples = maxSamplesList[contextID];

    if (!maxSamples && ext->isMultisampleSupported)
    {
        glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamples);
    }

    return maxSamples;
}
// VRV_PATCH: start
GLuint RenderBuffer::getObjectID(unsigned int nonSharedContextId, const GLExtensions* ext) const
{
    GLuint &objectID = _objectID[nonSharedContextId];

    int &dirty = _dirty[nonSharedContextId];

    if (objectID == 0)
    {
        ext->glGenRenderbuffers(1, &objectID);

#if OSG_FBO_DEBUG
#if defined(WIN32) || defined(WIN64)
        HGLRC currentGlContext = wglGetCurrentContext();
#else
        GLXContext currentGlContext = glXGetCurrentContext();
#endif
        std::cout << "created rbo: " << objectID << " , context: " << currentGlContext <<", nonSharedContextId: " << nonSharedContextId<< std::endl;
#endif
        if (objectID == 0)
            return 0;
        dirty = 1;
    }

    if (dirty)
    {
        // bind and configure
        ext->glBindRenderbuffer(GL_RENDERBUFFER_EXT, objectID);

        //vrv_patch
        if (ext->glObjectLabel && getName().length()){
           ext->glObjectLabel(GL_RENDERBUFFER_EXT, objectID, -1, getName().c_str());
        }

        // framebuffer_multisample_coverage specification requires that coverage
        // samples must be >= color samples.
        if (_samples < _colorSamples)
        {
            OSG_WARN << "Coverage samples must be greater than or equal to color samples."
                " Setting coverage samples equal to color samples." << std::endl;
            const_cast<RenderBuffer*>(this)->setSamples(_colorSamples);
        }

        if (_samples > 0 && ext->isRenderbufferMultisampleCoverageSupported())
        {
            int samples = minimum(_samples, getMaxSamples(nonSharedContextId, ext));
            int colorSamples = minimum(_colorSamples, samples);

            ext->glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER_EXT,
                samples, colorSamples, _internalFormat, _width, _height);
        }
        else if (_samples > 0 && ext->isRenderbufferMultisampleSupported())
        {
            int samples = minimum(_samples, getMaxSamples(nonSharedContextId, ext));

            ext->glRenderbufferStorageMultisample(GL_RENDERBUFFER_EXT,
                samples, _internalFormat, _width, _height);
        }
        else
        {
            ext->glRenderbufferStorage(GL_RENDERBUFFER_EXT, _internalFormat, _width, _height);
        }
        dirty = 0;
    }

    return objectID;
}
// VRV_PATCH: end

void RenderBuffer::resizeGLObjectBuffers(unsigned int maxSize)
{
    _objectID.resize(maxSize);
    _dirty.resize(maxSize);
}

// VRV_PATCH: start
void RenderBuffer::releaseGLObjects(osg::State* state) const
{
    if (state)
    {
        unsigned int nonSharedContextID = state->getNonSharedContextID();
        if (_objectID[nonSharedContextID])
        {
            osg::get<GLRenderBufferManager>(nonSharedContextID)->scheduleGLObjectForDeletion(_objectID[nonSharedContextID]);
            _objectID[nonSharedContextID] = 0;
        }
    }
    else
    {
        for(unsigned i=0; i<_objectID.size(); ++i)
        {
            if (_objectID[i])
            {
                osg::get<GLRenderBufferManager>(i)->scheduleGLObjectForDeletion(_objectID[i]);
                _objectID[i] = 0;
            }
        }
    }
}
// VRV_PATCH: end
/**************************************************************************
 * FrameBufferAttachment
 **************************************************************************/

#ifndef GL_TEXTURE_CUBE_MAP_POSITIVE_X
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X  0x8515
#endif

struct FrameBufferAttachment::Pimpl
{
    enum TargetType
    {
        RENDERBUFFER,
        TEXTURE1D,
        TEXTURE2D,
        TEXTURE3D,
        TEXTURECUBE,
        TEXTURERECT,
        TEXTURE2DARRAY,
        TEXTURE2DMULTISAMPLE,
        TEXTURE2DMULTISAMPLEARRAY
    };

    TargetType targetType;
    ref_ptr<RenderBuffer> renderbufferTarget;
    ref_ptr<Texture> textureTarget;
    unsigned int cubeMapFace;
    unsigned int level;
    unsigned int zoffset;

    explicit Pimpl(TargetType ttype = RENDERBUFFER, unsigned int lev = 0)
    :    targetType(ttype),
        cubeMapFace(0),
        level(lev),
        zoffset(0)
    {
    }

    Pimpl(const Pimpl &copy)
    :    targetType(copy.targetType),
        renderbufferTarget(copy.renderbufferTarget),
        textureTarget(copy.textureTarget),
        cubeMapFace(copy.cubeMapFace),
        level(copy.level),
        zoffset(copy.zoffset)
    {
    }
};

FrameBufferAttachment::FrameBufferAttachment()
{
    _ximpl = new Pimpl;
}

FrameBufferAttachment::FrameBufferAttachment(const FrameBufferAttachment &copy)
{
    _ximpl = new Pimpl(*copy._ximpl);
}

FrameBufferAttachment::FrameBufferAttachment(RenderBuffer* target)
{
    _ximpl = new Pimpl(Pimpl::RENDERBUFFER);
    _ximpl->renderbufferTarget = target;
}

FrameBufferAttachment::FrameBufferAttachment(Texture1D* target, unsigned int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE1D, level);
    _ximpl->textureTarget = target;
}

FrameBufferAttachment::FrameBufferAttachment(Texture2D* target, unsigned int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE2D, level);
    _ximpl->textureTarget = target;
}

FrameBufferAttachment::FrameBufferAttachment(Texture2DMultisample* target, unsigned int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE2DMULTISAMPLE, level);
    _ximpl->textureTarget = target;
}

FrameBufferAttachment::FrameBufferAttachment(Texture3D* target, unsigned int zoffset, unsigned int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE3D, level);
    _ximpl->textureTarget = target;
    _ximpl->zoffset = zoffset;
}

FrameBufferAttachment::FrameBufferAttachment(Texture2DArray* target, unsigned int layer, unsigned int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE2DARRAY, level);
    _ximpl->textureTarget = target;
    _ximpl->zoffset = layer;
}

FrameBufferAttachment::FrameBufferAttachment(Texture2DMultisampleArray* target, unsigned int layer, unsigned int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURE2DMULTISAMPLEARRAY, level);
    _ximpl->textureTarget = target;
    _ximpl->zoffset = layer;
}

FrameBufferAttachment::FrameBufferAttachment(TextureCubeMap* target, unsigned int face, unsigned int level)
{
    _ximpl = new Pimpl(Pimpl::TEXTURECUBE, level);
    _ximpl->textureTarget = target;
    _ximpl->cubeMapFace = face;
}

FrameBufferAttachment::FrameBufferAttachment(TextureRectangle* target)
{
    _ximpl = new Pimpl(Pimpl::TEXTURERECT);
    _ximpl->textureTarget = target;
}

FrameBufferAttachment::FrameBufferAttachment(Camera::Attachment& attachment)
{
    osg::Texture* texture = attachment._texture.get();

    if (texture)
    {
        osg::Texture1D* texture1D = dynamic_cast<osg::Texture1D*>(texture);
        if (texture1D)
        {
            _ximpl = new Pimpl(Pimpl::TEXTURE1D, attachment._level);
            _ximpl->textureTarget = texture1D;
            return;
        }

        osg::Texture2D* texture2D = dynamic_cast<osg::Texture2D*>(texture);
        if (texture2D)
        {
            _ximpl = new Pimpl(Pimpl::TEXTURE2D, attachment._level);
            _ximpl->textureTarget = texture2D;
            return;
        }

        osg::Texture2DMultisample* texture2DMS = dynamic_cast<osg::Texture2DMultisample*>(texture);
        if (texture2DMS)
        {
            _ximpl = new Pimpl(Pimpl::TEXTURE2DMULTISAMPLE, attachment._level);
            _ximpl->textureTarget = texture2DMS;
            return;
        }

        osg::Texture3D* texture3D = dynamic_cast<osg::Texture3D*>(texture);
        if (texture3D)
        {
            _ximpl = new Pimpl(Pimpl::TEXTURE3D, attachment._level);
            _ximpl->textureTarget = texture3D;
            _ximpl->zoffset = attachment._face;
            return;
        }

        osg::Texture2DArray* texture2DArray = dynamic_cast<osg::Texture2DArray*>(texture);
        if (texture2DArray)
        {
            _ximpl = new Pimpl(Pimpl::TEXTURE2DARRAY, attachment._level);
            _ximpl->textureTarget = texture2DArray;
            _ximpl->zoffset = attachment._face;
            return;
        }

        osg::Texture2DMultisampleArray* texture2DMSArray = dynamic_cast<osg::Texture2DMultisampleArray*>(texture);
        if (texture2DMSArray)
        {
            _ximpl = new Pimpl(Pimpl::TEXTURE2DMULTISAMPLEARRAY, attachment._level);
            _ximpl->textureTarget = texture2DMSArray;
            _ximpl->zoffset = attachment._face;
            return;
        }

        osg::TextureCubeMap* textureCubeMap = dynamic_cast<osg::TextureCubeMap*>(texture);
        if (textureCubeMap)
        {
            _ximpl = new Pimpl(Pimpl::TEXTURECUBE, attachment._level);
            _ximpl->textureTarget = textureCubeMap;
            _ximpl->cubeMapFace = attachment._face;
            return;
        }

        osg::TextureRectangle* textureRectangle = dynamic_cast<osg::TextureRectangle*>(texture);
        if (textureRectangle)
        {
            _ximpl = new Pimpl(Pimpl::TEXTURERECT);
            _ximpl->textureTarget = textureRectangle;
            return;
        }
    }

    osg::Image* image = attachment._image.get();
    if (image)
    {
        if (image->s()>0 && image->t()>0)
        {
            GLenum format = attachment._image->getInternalTextureFormat();
            if (format == 0)
                format = attachment._internalFormat;
            _ximpl = new Pimpl(Pimpl::RENDERBUFFER);
            _ximpl->renderbufferTarget = new osg::RenderBuffer(image->s(), image->t(), format);
            return;
        }
        else
        {
            OSG_WARN<<"Error: FrameBufferAttachment::FrameBufferAttachment(Camera::Attachment&) passed an empty osg::Image, image must be allocated first."<<std::endl;
        }
    }
    else
    {
        OSG_WARN<<"Error: FrameBufferAttachment::FrameBufferAttachment(Camera::Attachment&) passed an unrecognised Texture type."<<std::endl;
    }

    // provide all fallback
    _ximpl = new Pimpl();
}


FrameBufferAttachment::~FrameBufferAttachment()
{
    delete _ximpl;
}

FrameBufferAttachment &FrameBufferAttachment::operator = (const FrameBufferAttachment &copy)
{
   if (this != &copy)
   {
      delete _ximpl;
      _ximpl = new Pimpl(*copy._ximpl);
   }

    return *this;
}

bool FrameBufferAttachment::isMultisample() const
{
    if (_ximpl->renderbufferTarget.valid())
    {
        return _ximpl->renderbufferTarget->getSamples() > 0;
    }
    else if(_ximpl->textureTarget.valid())
    {
        osg::Texture2DMultisampleArray* tex2DMSArray = dynamic_cast<osg::Texture2DMultisampleArray*>(_ximpl->textureTarget.get());
        if (tex2DMSArray != NULL)
        {
            return tex2DMSArray->getNumSamples() > 0;
        }
    }

    return false;
}

void FrameBufferAttachment::createRequiredTexturesAndApplyGenerateMipMap(State &state, const GLExtensions* ext) const
{
    unsigned int contextID = state.getContextID();

    // force compile texture if necessary
    Texture::TextureObject *tobj = 0;
    if (_ximpl->textureTarget.valid())
    {
        tobj = _ximpl->textureTarget->getTextureObject(contextID);
        if (!tobj || tobj->id() == 0)
        {
            _ximpl->textureTarget->compileGLObjects(state);
            tobj = _ximpl->textureTarget->getTextureObject(contextID);
        }
        if (!tobj || tobj->id() == 0)
            return;

        Texture::FilterMode minFilter = _ximpl->textureTarget->getFilter(Texture::MIN_FILTER);
        if (minFilter==Texture::LINEAR_MIPMAP_LINEAR ||
            minFilter==Texture::LINEAR_MIPMAP_NEAREST ||
            minFilter==Texture::NEAREST_MIPMAP_LINEAR ||
            minFilter==Texture::NEAREST_MIPMAP_NEAREST)
        {
            state.setActiveTextureUnit(0);
            state.applyTextureAttribute(0, _ximpl->textureTarget.get());
            ext->glGenerateMipmap(_ximpl->textureTarget->getTextureTarget());
        }

    }
}

// VRV_PATCH: start
void FrameBufferAttachment::attach(State &state, GLenum target, GLenum attachment_point, const GLExtensions* ext) const
{
    unsigned int contextID = state.getContextID();
    unsigned int nonSharedContextId = state.getNonSharedContextID();

    if (_ximpl->targetType == Pimpl::RENDERBUFFER)
    {
        ext->glFramebufferRenderbuffer(target, attachment_point, GL_RENDERBUFFER_EXT, _ximpl->renderbufferTarget->getObjectID(nonSharedContextId, ext));
        return;
    }

    // targetType must be a texture, make sure we have a valid texture object
    Texture::TextureObject *tobj = 0;
    if (_ximpl->textureTarget.valid())
    {
        tobj = _ximpl->textureTarget->getTextureObject(contextID);
        if (!tobj || tobj->id() == 0)
        {
            _ximpl->textureTarget->compileGLObjects(state);
            tobj = _ximpl->textureTarget->getTextureObject(contextID);

        }
    }
    if (!tobj || tobj->id() == 0)
        return;

    switch (_ximpl->targetType)
    {
    case Pimpl::RENDERBUFFER:
        break; // already handled above. case should never be hit, just here to quieten compiler warning.
    case Pimpl::TEXTURE1D:
        ext->glFramebufferTexture1D(target, attachment_point, GL_TEXTURE_1D, tobj->id(), _ximpl->level);
        break;
    case Pimpl::TEXTURE2D:
        ext->glFramebufferTexture2D(target, attachment_point, GL_TEXTURE_2D, tobj->id(), _ximpl->level);
        break;
    case Pimpl::TEXTURE2DMULTISAMPLE:
        ext->glFramebufferTexture2D(target, attachment_point, GL_TEXTURE_2D_MULTISAMPLE, tobj->id(), _ximpl->level);
        break;
    case Pimpl::TEXTURE3D:
        if (_ximpl->zoffset == Camera::FACE_CONTROLLED_BY_GEOMETRY_SHADER)
            ext->glFramebufferTexture(target, attachment_point, tobj->id(), _ximpl->level);
        else
            ext->glFramebufferTexture3D(target, attachment_point, GL_TEXTURE_3D, tobj->id(), _ximpl->level, _ximpl->zoffset);
        break;
    case Pimpl::TEXTURE2DARRAY:
        if (_ximpl->zoffset == Camera::FACE_CONTROLLED_BY_GEOMETRY_SHADER)
            ext->glFramebufferTexture(target, attachment_point, tobj->id(), _ximpl->level);
        else if(_ximpl->zoffset == Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER)
        {
            if (ext->glFramebufferTextureMultiviewOVR)
            {
                ext->glFramebufferTextureMultiviewOVR(target, attachment_point, tobj->id(), _ximpl->level, 0, 2);
            }
        }
        else
            ext->glFramebufferTextureLayer(target, attachment_point, tobj->id(), _ximpl->level, _ximpl->zoffset);
        break;
    case Pimpl::TEXTURE2DMULTISAMPLEARRAY:
        if (_ximpl->zoffset == Camera::FACE_CONTROLLED_BY_GEOMETRY_SHADER)
        {
            if (ext->glFramebufferTexture)
            {
                ext->glFramebufferTexture(target, attachment_point, tobj->id(), _ximpl->level);
            }
        }
        else if (_ximpl->zoffset == Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER)
        {
            if (ext->glFramebufferTextureMultiviewOVR)
            {
                ext->glFramebufferTextureMultiviewOVR(target, attachment_point, tobj->id(), _ximpl->level, 0, 2);
            }
        }
        else
            ext->glFramebufferTextureLayer(target, attachment_point, tobj->id(), _ximpl->level, _ximpl->zoffset);
        break;
    case Pimpl::TEXTURERECT:
        ext->glFramebufferTexture2D(target, attachment_point, GL_TEXTURE_RECTANGLE, tobj->id(), 0);
        break;
    case Pimpl::TEXTURECUBE:
        if (_ximpl->cubeMapFace == Camera::FACE_CONTROLLED_BY_GEOMETRY_SHADER)
            ext->glFramebufferTexture(target, attachment_point, tobj->id(), _ximpl->level);
        else
            ext->glFramebufferTexture2D(target, attachment_point, GL_TEXTURE_CUBE_MAP_POSITIVE_X + _ximpl->cubeMapFace, tobj->id(), _ximpl->level);
        break;
    }
}
// VRV_PATCH: end
int FrameBufferAttachment::compare(const FrameBufferAttachment &fa) const
{
    if (&fa == this) return 0;
    if (_ximpl->targetType < fa._ximpl->targetType) return -1;
    if (_ximpl->targetType > fa._ximpl->targetType) return 1;
    if (_ximpl->renderbufferTarget.get() < fa._ximpl->renderbufferTarget.get()) return -1;
    if (_ximpl->renderbufferTarget.get() > fa._ximpl->renderbufferTarget.get()) return 1;
    if (_ximpl->textureTarget.get() < fa._ximpl->textureTarget.get()) return -1;
    if (_ximpl->textureTarget.get() > fa._ximpl->textureTarget.get()) return 1;
    if (_ximpl->cubeMapFace < fa._ximpl->cubeMapFace) return -1;
    if (_ximpl->cubeMapFace > fa._ximpl->cubeMapFace) return 1;
    if (_ximpl->level < fa._ximpl->level) return -1;
    if (_ximpl->level > fa._ximpl->level) return 1;
    if (_ximpl->zoffset < fa._ximpl->zoffset) return -1;
    if (_ximpl->zoffset > fa._ximpl->zoffset) return 1;
    return 0;
}

RenderBuffer* FrameBufferAttachment::getRenderBuffer()
{
    return _ximpl->renderbufferTarget.get();
}

Texture* FrameBufferAttachment::getTexture()
{
    return _ximpl->textureTarget.get();
}

const RenderBuffer* FrameBufferAttachment::getRenderBuffer() const
{
    return _ximpl->renderbufferTarget.get();
}

const Texture* FrameBufferAttachment::getTexture() const
{
    return _ximpl->textureTarget.get();
}

unsigned int FrameBufferAttachment::getCubeMapFace() const
{
    return _ximpl->cubeMapFace;
}

unsigned int FrameBufferAttachment::getTextureLevel() const
{
    return _ximpl->level;
}

unsigned int FrameBufferAttachment::getTexture3DZOffset() const
{
    return _ximpl->zoffset;
}

unsigned int FrameBufferAttachment::getTextureArrayLayer() const
{
    return _ximpl->zoffset;
}

bool FrameBufferAttachment::isArray() const
{
    return _ximpl->targetType == Pimpl::TEXTURE2DARRAY || _ximpl->targetType == Pimpl::TEXTURE2DMULTISAMPLEARRAY;
}

void FrameBufferAttachment::resizeGLObjectBuffers(unsigned int maxSize)
{
    if (_ximpl->renderbufferTarget.valid()) _ximpl->renderbufferTarget->resizeGLObjectBuffers(maxSize);
    if (_ximpl->textureTarget.valid()) _ximpl->textureTarget->resizeGLObjectBuffers(maxSize);
}

void FrameBufferAttachment::releaseGLObjects(osg::State* state) const
{
    if (_ximpl->renderbufferTarget.valid()) _ximpl->renderbufferTarget->releaseGLObjects(state);
    if (_ximpl->textureTarget.valid()) _ximpl->textureTarget->releaseGLObjects(state);
}

/**************************************************************************
 * FrameBufferObject
 **************************************************************************/

FrameBufferObject::FrameBufferObject()
:    StateAttribute()
{
}

FrameBufferObject::FrameBufferObject(const FrameBufferObject &copy, const CopyOp &copyop)
:    StateAttribute(copy, copyop),
    _attachments(copy._attachments),
    _drawBuffers(copy._drawBuffers)
{
}

FrameBufferObject::~FrameBufferObject()
{
    for(unsigned i=0; i<_fboID.size(); ++i)
    {
        if (_fboID[i]) osg::get<GLFrameBufferObjectManager>(i)->scheduleGLObjectForDeletion(_fboID[i]);
    }
}

void FrameBufferObject::resizeGLObjectBuffers(unsigned int maxSize)
{
    _fboID.resize(maxSize);
    _unsupported.resize(maxSize);
    _fboID.resize(maxSize);

    for(AttachmentMap::iterator itr = _attachments.begin(); itr != _attachments.end(); ++itr)
    {
        itr->second.resizeGLObjectBuffers(maxSize);
    }
}
// VRV_PATCH: start
void FrameBufferObject::releaseGLObjects(osg::State* state) const
{
    if (state)
    {
        unsigned int nonSharedContextID = state->getNonSharedContextID();
        if (_fboID[nonSharedContextID])
        {
            osg::get<GLFrameBufferObjectManager>(nonSharedContextID)->scheduleGLObjectForDeletion(_fboID[nonSharedContextID]);
            _fboID[nonSharedContextID] = 0;
        }
    }
    else
    {
        for(unsigned int i=0; i<_fboID.size(); ++i)
        {
            if (_fboID[i])
            {
                osg::get<GLFrameBufferObjectManager>(i)->scheduleGLObjectForDeletion(_fboID[i]);
                _fboID[i] = 0;
            }
        }
    }

    for(AttachmentMap::const_iterator itr = _attachments.begin(); itr != _attachments.end(); ++itr)
    {
        itr->second.releaseGLObjects(state);
    }
}
// VRV_PATCH: end
// helper function in RenderStage
static const char* getBufferComponentStr(Camera::BufferComponent buffer)
{
   switch (buffer)
   {
   case (osg::Camera::DEPTH_BUFFER): return "DEPTH_BUFFER";
   case (osg::Camera::STENCIL_BUFFER): return "STENCIL_BUFFER";
   case (osg::Camera::PACKED_DEPTH_STENCIL_BUFFER): return "PACKED_DEPTH_STENCIL_BUFFER";
   case (osg::Camera::COLOR_BUFFER): return "COLOR_BUFFER";
   case (osg::Camera::COLOR_BUFFER0): return "COLOR_BUFFER0";
   case (osg::Camera::COLOR_BUFFER1): return "COLOR_BUFFER1";
   case (osg::Camera::COLOR_BUFFER2): return "COLOR_BUFFER2";
   case (osg::Camera::COLOR_BUFFER3): return "COLOR_BUFFER3";
   case (osg::Camera::COLOR_BUFFER4): return "COLOR_BUFFER4";
   case (osg::Camera::COLOR_BUFFER5): return "COLOR_BUFFER5";
   case (osg::Camera::COLOR_BUFFER6): return "COLOR_BUFFER6";
   case (osg::Camera::COLOR_BUFFER7): return "COLOR_BUFFER7";
   case (osg::Camera::COLOR_BUFFER8): return "COLOR_BUFFER8";
   case (osg::Camera::COLOR_BUFFER9): return "COLOR_BUFFER9";
   case (osg::Camera::COLOR_BUFFER10): return "COLOR_BUFFER10";
   case (osg::Camera::COLOR_BUFFER11): return "COLOR_BUFFER11";
   case (osg::Camera::COLOR_BUFFER12): return "COLOR_BUFFER12";
   case (osg::Camera::COLOR_BUFFER13): return "COLOR_BUFFER13";
   case (osg::Camera::COLOR_BUFFER14): return "COLOR_BUFFER14";
   case (osg::Camera::COLOR_BUFFER15): return "COLOR_BUFFER15";
   default: return "UnknownBufferComponent";
   }
}
void FrameBufferObject::setAttachment(BufferComponent attachment_point, const FrameBufferAttachment &attachment)
{
    _attachments[attachment_point] = attachment;
    //vrv_patch
    FrameBufferAttachment & attach = _attachments[attachment_point];
    if (attach.getTexture() && attach.getTexture()->getName().length() == 0){
       attach.getTexture()->setName(getName() + getBufferComponentStr(attachment_point) + "_fbo");
    }
    if (attach.getRenderBuffer() && attach.getRenderBuffer()->getName().length() == 0){
       attach.getRenderBuffer()->setName(getName() + getBufferComponentStr(attachment_point) + "_fbo_rb");
    }
    updateDrawBuffers();
    dirtyAll();
}


GLenum FrameBufferObject::convertBufferComponentToGLenum(BufferComponent attachment_point) const
{
    switch(attachment_point)
    {
        case(Camera::DEPTH_BUFFER): return GL_DEPTH_ATTACHMENT_EXT;
        case(Camera::STENCIL_BUFFER): return GL_STENCIL_ATTACHMENT_EXT;
        case(Camera::COLOR_BUFFER): return GL_COLOR_ATTACHMENT0_EXT;
        default: return GLenum(GL_COLOR_ATTACHMENT0_EXT + (attachment_point-Camera::COLOR_BUFFER0));
    }
}

void FrameBufferObject::updateDrawBuffers()
{
    _drawBuffers.clear();

    // create textures and mipmaps before we bind the frame buffer object
    for (AttachmentMap::const_iterator i=_attachments.begin(); i!=_attachments.end(); ++i)
    {
        // setup draw buffers based on the attachment definition
        if (i->first >= Camera::COLOR_BUFFER0 && i->first <= Camera::COLOR_BUFFER15)
            _drawBuffers.push_back(convertBufferComponentToGLenum(i->first));
    }
}

void FrameBufferObject::apply(State &state) const
{
    apply(state, READ_DRAW_FRAMEBUFFER);
}

// VRV_PATCH: start
void FrameBufferObject::apply(State &state, BindTarget target) const
{
    unsigned int nonSharedContextId = state.getNonSharedContextID();

    if (_unsupported[nonSharedContextId])
        return;


    GLExtensions* ext = state.get<GLExtensions>();
    if (!ext->isFrameBufferObjectSupported)
    {
        _unsupported[nonSharedContextId] = 1;
        OSG_WARN << "Warning: EXT_framebuffer_object is not supported" << std::endl;
        return;
    }

    if (_attachments.empty())
    {
        ext->glBindFramebuffer(target, 0);
        state.setLastAppliedFboId(0); //save off the current framebuffer in the state
        return;
    }

    int &dirtyAttachmentList = _dirtyAttachmentList[nonSharedContextId];

    GLuint &fboID = _fboID[nonSharedContextId];
    if (fboID == 0)
    {
        ext->glGenFramebuffers(1, &fboID);
#if OSG_FBO_DEBUG
#if defined(WIN32) || defined(WIN64)
        HGLRC currentGlContext = wglGetCurrentContext();
#else
        GLXContext currentGlContext = glXGetCurrentContext();
#endif
        std::cout << "created fbo: " << fboID << " , context: " << currentGlContext << ", nonSharedContextId: " << nonSharedContextId << ", gc: "<<state.getGraphicsContext()<< std::endl;
#endif
        if (fboID == 0)
        {
            OSG_WARN << "Warning: FrameBufferObject: could not create the FBO" << std::endl;
            return;
        }

        dirtyAttachmentList = 1;

    }

    if (dirtyAttachmentList)
    {
        // the set of of attachments appears to be thread sensitive, it shouldn't be because
        // OpenGL FBO handles osg::FrameBufferObject has are multi-buffered...
        // so as a temporary fix will stick in a mutex to ensure that only one thread passes through here
        // at one time.
        static OpenThreads::Mutex s_mutex;
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_mutex);

        // create textures and mipmaps before we bind the frame buffer object
        for (AttachmentMap::const_iterator i=_attachments.begin(); i!=_attachments.end(); ++i)
        {
            const FrameBufferAttachment &fa = i->second;
            fa.createRequiredTexturesAndApplyGenerateMipMap(state, ext);
        }

    }


    ext->glBindFramebuffer(target, fboID);
    state.setLastAppliedFboId(fboID); //save off the current framebuffer in the state


    //vrv_patch
    if (dirtyAttachmentList){
       if (ext->glObjectLabel && getName().length()){
          ext->glObjectLabel(GL_FRAMEBUFFER_EXT, fboID, getName().length(), getName().c_str());
       }
    }


    // enable drawing buffers to render the result to fbo
    if ( (target == READ_DRAW_FRAMEBUFFER) || (target == DRAW_FRAMEBUFFER) )
    {
        if (_drawBuffers.size() > 0)
        {
            GLExtensions *gl2e = state.get<GLExtensions>();
            if (gl2e && gl2e->glDrawBuffers)
            {
                gl2e->glDrawBuffers(_drawBuffers.size(), &(_drawBuffers[0]));
            }
            else
            {
                OSG_WARN <<"Warning: FrameBufferObject: could not set draw buffers, glDrawBuffers is not supported!" << std::endl;
            }
        }
    }

    if (dirtyAttachmentList)
    {
        for (AttachmentMap::const_iterator i=_attachments.begin(); i!=_attachments.end(); ++i)
        {
            const FrameBufferAttachment &fa = i->second;
            switch(i->first)
            {
                case(Camera::PACKED_DEPTH_STENCIL_BUFFER):
                    if (ext->isPackedDepthStencilSupported)
                    {
                        fa.attach(state, target, GL_DEPTH_ATTACHMENT_EXT, ext);
                        fa.attach(state, target, GL_STENCIL_ATTACHMENT_EXT, ext);
                    }
                    else
                    {
                        OSG_WARN <<
                            "Warning: FrameBufferObject: could not attach PACKED_DEPTH_STENCIL_BUFFER, "
                            "EXT_packed_depth_stencil is not supported!" << std::endl;
                    }
                    break;

                default:
                    fa.attach(state, target, convertBufferComponentToGLenum(i->first), ext);
                    break;
            }
        }
        dirtyAttachmentList = 0;
    }

}
// VRV_PATCH: end
bool FrameBufferObject::isMultisample() const
{
    if (_attachments.size())
    {
        // If the FBO is correctly set up then all attachments will be either
        // multisampled or single sampled. Therefore we can just return the
        // result of the first attachment.
        return _attachments.begin()->second.isMultisample();
    }

    return false;
}

bool FrameBufferObject::isArray() const
{
    if (_attachments.size())
    {
        // If the FBO is correctly set up then all attachments will be either
        // arrays or standard types
        return _attachments.begin()->second.isArray();
    }

    return false;
}

int FrameBufferObject::compare(const StateAttribute &sa) const
{
    COMPARE_StateAttribute_Types(FrameBufferObject, sa);
    COMPARE_StateAttribute_Parameter(_attachments.size());
    AttachmentMap::const_iterator i = _attachments.begin();
    AttachmentMap::const_iterator j = rhs._attachments.begin();
    for (; i!=_attachments.end(); ++i, ++j)
    {
        int cmp = i->second.compare(j->second);
        if (cmp != 0) return cmp;
    }
    return 0;
}
