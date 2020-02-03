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
#include <stdio.h>

#include <osg/Notify>
#include <osg/Texture1D>
#include <osg/Texture2D>
#include <osg/Texture2DMultisample>
#include <osg/Texture3D>
#include <osg/Texture2DArray>
#include <osg/Texture2DMultisampleArray>
#include <osg/TextureRectangle>
#include <osg/TextureCubeMap>
#include <osg/ContextData>
#include <osg/GLExtensions>
#include <osg/GLU>

// VRV_PATCH
#include <osg/Profile>

#include <osgUtil/Statistics>

#include <osgUtil/RenderStage>

using namespace osg;
using namespace osgUtil;

// register a RenderStage prototype with the RenderBin prototype list.
//RegisterRenderBinProxy<RenderStage> s_registerRenderStageProxy;

RenderStage::RenderStage():
    RenderBin(getDefaultRenderBinSortMode()),
    _disableFboAfterRender(true)
{
    // point RenderBin's _stage to this to ensure that references to
    // stage don't go tempted away to any other stage.
    _stage = this;
    _stageDrawnThisFrame = false;

    _drawBuffer = GL_NONE;
    _drawBufferApplyMask = false;
    _readBuffer = GL_NONE;
    _readBufferApplyMask = false;

    _clearMask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
    _clearColor.set(0.0f,0.0f,0.0f,0.0f);
    _clearAccum.set(0.0f,0.0f,0.0f,0.0f);
    _clearDepth = 1.0;
    _clearStencil = 0;

    _cameraRequiresSetUp = false;
    _cameraAttachmentMapModifiedCount = 0;
    _camera = 0;

    _level = 0;
    _face = 0;

    _imageReadPixelFormat = GL_RGBA;
    _imageReadPixelDataType = GL_UNSIGNED_BYTE;
}

RenderStage::RenderStage(SortMode mode):
    RenderBin(mode),
    _disableFboAfterRender(true)
{
    // point RenderBin's _stage to this to ensure that references to
    // stage don't go tempted away to any other stage.
    _stage = this;
    _stageDrawnThisFrame = false;

    _drawBuffer = GL_NONE;
    _drawBufferApplyMask = false;
    _readBuffer = GL_NONE;
    _readBufferApplyMask = false;

    _clearMask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
    _clearColor.set(0.0f,0.0f,0.0f,0.0f);
    _clearAccum.set(0.0f,0.0f,0.0f,0.0f);
    _clearDepth = 1.0;
    _clearStencil = 0;

    _cameraRequiresSetUp = false;
    _cameraAttachmentMapModifiedCount = 0;
    _camera = 0;

    _level = 0;
    _face = 0;

    _imageReadPixelFormat = GL_RGBA;
    _imageReadPixelDataType = GL_UNSIGNED_BYTE;
}

RenderStage::RenderStage(const RenderStage& rhs,const osg::CopyOp& copyop):
        RenderBin(rhs,copyop),
        _stageDrawnThisFrame(false),
        _preRenderList(rhs._preRenderList),
        _postRenderList(rhs._postRenderList),
        _viewport(rhs._viewport),
        _drawBuffer(rhs._drawBuffer),
        _drawBufferApplyMask(rhs._drawBufferApplyMask),
        _readBuffer(rhs._readBuffer),
        _readBufferApplyMask(rhs._readBufferApplyMask),
        _clearMask(rhs._clearMask),
        _colorMask(rhs._colorMask),
        _clearColor(rhs._clearColor),
        _clearAccum(rhs._clearAccum),
        _clearDepth(rhs._clearDepth),
        _clearStencil(rhs._clearStencil),
        _cameraRequiresSetUp(rhs._cameraRequiresSetUp),
        _cameraAttachmentMapModifiedCount(rhs._cameraAttachmentMapModifiedCount),
        _camera(rhs._camera),
        _level(rhs._level),
        _face(rhs._face),
        _imageReadPixelFormat(rhs._imageReadPixelFormat),
        _imageReadPixelDataType(rhs._imageReadPixelDataType),
        _disableFboAfterRender(rhs._disableFboAfterRender),
        _renderStageLighting(rhs._renderStageLighting)
{
    _stage = this;
}


RenderStage::~RenderStage()
{
}

void RenderStage::reset()
{
    _stageDrawnThisFrame = false;

    if (_renderStageLighting.valid()) _renderStageLighting->reset();

    for(RenderStageList::iterator pre_itr = _preRenderList.begin();
        pre_itr != _preRenderList.end();
        ++pre_itr)
    {
        pre_itr->second->reset();
    }

    RenderBin::reset();

    for(RenderStageList::iterator post_itr = _postRenderList.begin();
        post_itr != _postRenderList.end();
        ++post_itr)
    {
        post_itr->second->reset();
    }

    _preRenderList.clear();
    _postRenderList.clear();
}

void RenderStage::sort()
{
    for(RenderStageList::iterator pre_itr = _preRenderList.begin();
        pre_itr != _preRenderList.end();
        ++pre_itr)
    {
        pre_itr->second->sort();
    }

    RenderBin::sort();

    for(RenderStageList::iterator post_itr = _postRenderList.begin();
        post_itr != _postRenderList.end();
        ++post_itr)
    {
        post_itr->second->sort();
    }
}

void RenderStage::addPreRenderStage(RenderStage* rs, int order)
{
    if (rs)
    {
        RenderStageList::iterator itr;
        for(itr = _preRenderList.begin(); itr != _preRenderList.end(); ++itr) {
            if(order < itr->first) {
                break;
            }
        }
        if(itr == _preRenderList.end()) {
            _preRenderList.push_back(RenderStageOrderPair(order,rs));
        } else {
            _preRenderList.insert(itr,RenderStageOrderPair(order,rs));
        }
    }
}

void RenderStage::addPostRenderStage(RenderStage* rs, int order)
{
    if (rs)
    {
        RenderStageList::iterator itr;
        for(itr = _postRenderList.begin(); itr != _postRenderList.end(); ++itr) {
            if(order < itr->first) {
                break;
            }
        }
        if(itr == _postRenderList.end()) {
            _postRenderList.push_back(RenderStageOrderPair(order,rs));
        } else {
            _postRenderList.insert(itr,RenderStageOrderPair(order,rs));
        }
    }
}

void RenderStage::drawPreRenderStages(osg::RenderInfo& renderInfo,RenderLeaf*& previous)
{
    if (_preRenderList.empty()) return;

    //cout << "Drawing prerendering stages "<<this<< "  "<<_viewport->x()<<","<< _viewport->y()<<","<< _viewport->width()<<","<< _viewport->height()<<std::endl;
    for(RenderStageList::iterator itr=_preRenderList.begin();
        itr!=_preRenderList.end();
        ++itr)
    {
        itr->second->draw(renderInfo,previous);
    }
    //cout << "Done Drawing prerendering stages "<<this<< "  "<<_viewport->x()<<","<< _viewport->y()<<","<< _viewport->width()<<","<< _viewport->height()<<std::endl;
}


const char* getBufferComponentStr(Camera::BufferComponent buffer)
{
   switch (buffer)
   {
   case (osg::Camera::DEPTH_BUFFER) : return "DEPTH_BUFFER";
   case (osg::Camera::STENCIL_BUFFER) : return "STENCIL_BUFFER";
   case (osg::Camera::PACKED_DEPTH_STENCIL_BUFFER) : return "PACKED_DEPTH_STENCIL_BUFFER";
   case (osg::Camera::COLOR_BUFFER) : return "COLOR_BUFFER";
   case (osg::Camera::COLOR_BUFFER0) : return "COLOR_BUFFER0";
   case (osg::Camera::COLOR_BUFFER1) : return "COLOR_BUFFER1";
   case (osg::Camera::COLOR_BUFFER2) : return "COLOR_BUFFER2";
   case (osg::Camera::COLOR_BUFFER3) : return "COLOR_BUFFER3";
   case (osg::Camera::COLOR_BUFFER4) : return "COLOR_BUFFER4";
   case (osg::Camera::COLOR_BUFFER5) : return "COLOR_BUFFER5";
   case (osg::Camera::COLOR_BUFFER6) : return "COLOR_BUFFER6";
   case (osg::Camera::COLOR_BUFFER7) : return "COLOR_BUFFER7";
   case (osg::Camera::COLOR_BUFFER8) : return "COLOR_BUFFER8";
   case (osg::Camera::COLOR_BUFFER9) : return "COLOR_BUFFER9";
   case (osg::Camera::COLOR_BUFFER10) : return "COLOR_BUFFER10";
   case (osg::Camera::COLOR_BUFFER11) : return "COLOR_BUFFER11";
   case (osg::Camera::COLOR_BUFFER12) : return "COLOR_BUFFER12";
   case (osg::Camera::COLOR_BUFFER13) : return "COLOR_BUFFER13";
   case (osg::Camera::COLOR_BUFFER14) : return "COLOR_BUFFER14";
   case (osg::Camera::COLOR_BUFFER15) : return "COLOR_BUFFER15";
   default: return "UnknownBufferComponent";
   }
}

const char *
glFormatsToString(int compressed_format_type)
{
   switch (compressed_format_type)
   {
   case GL_RGB: return"GL_RGB";
   case GL_SRGB:return"GL_SRGB";
   case GL_RGBA: return"GL_RGBA";
   case GL_SRGB_ALPHA_EXT: return"GL_SRGB_ALPHA";
   case GL_LUMINANCE: return "GL_LUMINANCE";
   case GL_SLUMINANCE:   return "GL_SLUMINANCE";
   case GL_LUMINANCE_ALPHA:      return "GL_LUMINANCE_ALPHA";
   case GL_SLUMINANCE_ALPHA:      return "GL_SLUMINANCE_ALPHA";
   case GL_SRGB8:      return "GL_SRGB8";
   case GL_SRGB8_ALPHA8:      return "GL_SRGB8_ALPHA8";
   case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:    return "GL_COMPRESSED_RGB_S3TC_DXT1";
   case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:      return "GL_COMPRESSED_RGBA_S3TC_DXT1";
   case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:      return "GL_COMPRESSED_RGBA_S3TC_DXT3";
   case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:      return "GL_COMPRESSED_RGBA_S3TC_DXT5";
   case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:      return "GL_COMPRESSED_SRGB_S3TC_DXT1";
   case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:      return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1";
   case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:      return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3";
   case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:      return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5";
   case GL_RGB4:      return "GL_RGB4";
   case GL_RGB5:      return "GL_RGB5";
   case GL_RGB8:      return "GL_RGB8";
   case GL_RGB10:     return "GL_RGB10";
   case GL_RGB12:     return "GL_RGB12";
   case GL_RGB16:     return "GL_RGB16";
   case GL_RGBA2:     return "GL_RGBA2";
   case GL_RGBA4:     return "GL_RGBA4";
   case GL_RGB5_A1:   return "GL_RGB5_A1";
   case GL_RGBA8:     return "GL_RGBA8";
   case GL_RGB10_A2:  return "GL_RGB10_A2";
   case GL_RGBA12:    return "GL_RGBA12";
   case GL_RGBA16:    return "GL_RGBA16";
   case GL_DEPTH_COMPONENT16: return "GL_DEPTH_COMPONENT16";
   case GL_DEPTH_COMPONENT24: return "GL_DEPTH_COMPONENT24";
   case GL_DEPTH_COMPONENT32: return "GL_DEPTH_COMPONENT32";
   case GL_DEPTH_COMPONENT32F: return "GL_DEPTH_COMPONENT32F";
         

   case GL_RG:  return "GL_RG";
   case GL_RG_INTEGER:  return "GL_RG_INTEGER";
   case GL_R8:  return "GL_R8";
   case GL_R16:  return "GL_R16";
   case GL_RG8:  return "GL_RG8";
   case GL_RG16:  return "GL_RG16";
   case GL_R16F:  return "GL_R16F";
   case GL_R32F:  return "GL_R32F";
   case GL_RG16F:  return "GL_RG16F";
   case GL_RG32F:  return "GL_RG32F";
   case GL_R8I:  return "GL_R8I";
   case GL_R8UI:  return "GL_R8UI";
   case GL_R16I:  return "GL_R16I";
   case GL_R16UI:  return "GL_R16UI";
   case GL_R32I:  return "GL_R32I";
   case GL_R32UI:  return "GL_R32UI";
   case GL_RG8I:  return "GL_RG8I";
   case GL_RG8UI:  return "GL_RG8UI";
   case GL_RG16I:  return "GL_RG16I";
   case GL_RG16UI:  return "GL_RG16UI";
   case GL_RG32I:  return "GL_RG32I";
   case GL_RG32UI: return "GL_RG32UI";
   case GL_RGBA32F_ARB: return "GL_RGBA32F";
   case GL_RGB32F_ARB: return "GL_RGB32F_ARB";
   case GL_ALPHA32F_ARB: return "GL_ALPHA32F_ARB";
   case GL_INTENSITY32F_ARB: return "GL_INTENSITY32F";
   case GL_LUMINANCE32F_ARB: return "GL_LUMINANCE32F";
   case GL_LUMINANCE_ALPHA32F_ARB: return "GL_LUMINANCE_ALPHA32F";
   case GL_RGBA16F_ARB: return "GL_RGBA16F";
   case GL_RGB16F_ARB: return "GL_RGB16F";
   case GL_ALPHA16F_ARB: return "GL_ALPHA16F";
   case GL_INTENSITY16F_ARB: return "GL_INTENSITY16F";
   case GL_LUMINANCE16F_ARB: return "GL_LUMINANCE16F";
   case GL_LUMINANCE_ALPHA16F_ARB: return "GL_LUMINANCE_ALPHA16F";
   case -1:      return "Unspecified GL_FORMAT";

   }
   return  "Unknown GL Format";
}


void dumpFboInfo(osg::ref_ptr<osg::FrameBufferObject> fbo)
{
    OSG_WARN << "Dumping FBO info for " << fbo->getName() << std::endl;

   for (int i = 0; i < Camera::COLOR_BUFFER15; i++)
   {
      if (fbo->hasAttachment((Camera::BufferComponent)i))
      {
         OSG_WARN << "Has Attachment for BufferComponent " << getBufferComponentStr((Camera::BufferComponent)i) << std::endl;
         const osg::FrameBufferAttachment & fba = fbo->getAttachment((Camera::BufferComponent)i);
         if (fba.getTexture()){
            const  Texture * texture = fba.getTexture();
            OSG_WARN << "   " << texture->className() << " Named : " << texture->getName() << std::endl;
            const Texture2DArray * tex2da = dynamic_cast<const Texture2DArray *>(fba.getTexture());
            if (tex2da) {
            //    OSG_WARN << "   Num Images: " << tex2da->getNumImages() << std::endl;
                OSG_WARN << "   width: " << tex2da->getTextureWidth() << std::endl;
                OSG_WARN << "   height: " << tex2da->getTextureHeight() << std::endl;
                OSG_WARN << "   depth: " << tex2da->getTextureDepth() << std::endl;
            }
            else {
                OSG_WARN << "   width: " << texture->getTextureWidth() << std::endl;
                OSG_WARN << "   height: " << texture->getTextureHeight() << std::endl;
            }
            OSG_WARN << "   internal format: " << std::hex << texture->getInternalFormat() << " - " << glFormatsToString(texture->getInternalFormat()) << std::endl;
            OSG_WARN << "   num images: " << texture->getNumImages() << std::endl;
         }
         if (fba.getRenderBuffer())
         {
             const RenderBuffer * rb = fba.getRenderBuffer();
            OSG_WARN << "   Renderbuffer Named: " << rb->getName() << std::endl;
            OSG_WARN << "   width: " << rb->getWidth() << std::endl;
            OSG_WARN << "   height: " << rb->getHeight() << std::endl;
            OSG_WARN << "   internal format: " << std::hex << rb->getInternalFormat() << " - " << glFormatsToString(rb->getInternalFormat()) << std::endl;
            OSG_WARN << "   samples: " << rb->getSamples() << std::endl;
         }
      }
   };
}

void RenderStage::runCameraSetUp(osg::RenderInfo& renderInfo)
{
    _cameraRequiresSetUp = false;

    if (!_camera) return;

    OSG_INFO<<"RenderStage::runCameraSetUp(osg::RenderInfo& renderInfo) "<<this<<std::endl;

    _cameraAttachmentMapModifiedCount = _camera->getAttachmentMapModifiedCount();

    osg::State& state = *renderInfo.getState();

    osg::Camera::RenderTargetImplementation renderTargetImplementation = _camera->getRenderTargetImplementation();
    osg::Camera::RenderTargetImplementation renderTargetFallback = _camera->getRenderTargetFallback();

    osg::Camera::BufferAttachmentMap& bufferAttachments = _camera->getBufferAttachmentMap();

    _bufferAttachmentMap.clear();
    _resolveArrayLayerFbos.clear();
    _arrayLayerFbos.clear();

    // compute the required dimensions
    int width = static_cast<int>(_viewport->x() + _viewport->width());
    int height = static_cast<int>(_viewport->y() + _viewport->height());
    int depth = 1;
    bool isArray = false;
    for(osg::Camera::BufferAttachmentMap::iterator itr = bufferAttachments.begin();
        itr != bufferAttachments.end();
        ++itr)
    {
        width = osg::maximum(width,itr->second.width());
        height = osg::maximum(height,itr->second.height());
        depth = osg::maximum(depth,itr->second.depth());
    }

    // OSG_NOTICE<<"RenderStage::runCameraSetUp viewport "<<_viewport->x()<<" "<<_viewport->y()<<" "<<_viewport->width()<<" "<<_viewport->height()<<std::endl;
    // OSG_NOTICE<<"RenderStage::runCameraSetUp computed "<<width<<" "<<height<<" "<<depth<<std::endl;

    // attach images that need to be copied after the stage is drawn.
    for(osg::Camera::BufferAttachmentMap::iterator itr = bufferAttachments.begin();
        itr != bufferAttachments.end();
        ++itr)
    {
        // if one exist attach image to the RenderStage.
        if (itr->second._image.valid())
        {
            osg::Image* image = itr->second._image.get();
            GLenum pixelFormat = image->getPixelFormat();
            GLenum dataType = image->getDataType();

            if (image->data()==0)
            {
                if (pixelFormat==0) pixelFormat = itr->second._internalFormat;
                if (pixelFormat==0) pixelFormat = _imageReadPixelFormat;
                if (pixelFormat==0) pixelFormat = GL_RGBA;

                if (dataType==0) dataType = _imageReadPixelDataType;
                if (dataType==0) dataType = GL_UNSIGNED_BYTE;
            }

            _bufferAttachmentMap[itr->first]._imageReadPixelFormat = pixelFormat;
            _bufferAttachmentMap[itr->first]._imageReadPixelDataType = dataType;
            _bufferAttachmentMap[itr->first]._image = image;
        }

        if (itr->second._texture.valid())
        {
            osg::Texture* texture = itr->second._texture.get();
            osg::Texture1D* texture1D = 0;
            osg::Texture2D* texture2D = 0;
            osg::Texture2DMultisample* texture2DMS = 0;
            osg::Texture3D* texture3D = 0;
            osg::TextureCubeMap* textureCubeMap = 0;
            osg::TextureRectangle* textureRectangle = 0;
            osg::Texture2DArray* texture2DArray = 0;
            if (0 != (texture1D=dynamic_cast<osg::Texture1D*>(texture)))
            {
                if (texture1D->getTextureWidth()==0)
                {
                    texture1D->setTextureWidth(width);
                }
            }
            else if (0 != (texture2D = dynamic_cast<osg::Texture2D*>(texture)))
            {
                if (texture2D->getTextureWidth()==0 || texture2D->getTextureHeight()==0)
                {
                    texture2D->setTextureSize(width,height);
                }
            }
            else if (0 != (texture2DMS = dynamic_cast<osg::Texture2DMultisample*>(texture)))
            {
                if (texture2DMS->getTextureWidth()==0 || texture2DMS->getTextureHeight()==0)
                {
                    texture2DMS->setTextureSize(width,height);
                }
            }
            else if (0 != (texture3D = dynamic_cast<osg::Texture3D*>(texture)))
            {
                if (texture3D->getTextureWidth()==0 || texture3D->getTextureHeight()==0 || texture3D->getTextureDepth()==0 )
                {
                    // note we dont' have the depth here, so we'll heave to assume that height and depth are the same..
                    texture3D->setTextureSize(width,height,height);
                }
            }
            else if (0 != (textureCubeMap = dynamic_cast<osg::TextureCubeMap*>(texture)))
            {
                if (textureCubeMap->getTextureWidth()==0 || textureCubeMap->getTextureHeight()==0)
                {
                    textureCubeMap->setTextureSize(width,height);
                }
            }
            else if (0 != (textureRectangle = dynamic_cast<osg::TextureRectangle*>(texture)))
            {
                if (textureRectangle->getTextureWidth()==0 || textureRectangle->getTextureHeight()==0)
                {
                    textureRectangle->setTextureSize(width,height);
                }
            }
            else if (0 != (texture2DArray = dynamic_cast<osg::Texture2DArray*>(texture)))
            {
                isArray = true;
                //width = texture2DArray->getTextureWidth();
                //height = texture2DArray->getTextureHeight();
                depth = texture2DArray->getTextureDepth();
            }
        }
    }

    if (renderTargetImplementation == osg::Camera::FRAME_BUFFER_OBJECT)
    {
        osg::GLExtensions* ext = state.get<osg::GLExtensions>();
        bool fbo_supported = ext->isFrameBufferObjectSupported;

        if (fbo_supported)
        {
            OSG_INFO<<"Setting up osg::Camera::FRAME_BUFFER_OBJECT"<<std::endl;

            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(*(_camera->getDataChangeMutex()));

            osg::ref_ptr<osg::FrameBufferObject> fbo = new osg::FrameBufferObject;
            osg::ref_ptr<osg::FrameBufferObject> fbo_multisample;
            fbo->setName(_camera->getName());

            bool colorAttached = false;
            bool depthAttached = false;
            bool stencilAttached = false;
            unsigned samples = 0;
            unsigned colorSamples = 0;

            // This is not a cut and paste error. Set BOTH local masks
            // to the value of the Camera's use render buffers mask.
            // We'll change this if and only if we decide we're doing MSFBO.
            unsigned int renderBuffersMask = _camera->getImplicitBufferAttachmentRenderMask(true);
            unsigned int resolveBuffersMask = _camera->getImplicitBufferAttachmentRenderMask(true);

            if (ext->isRenderbufferMultisampleSupported())
            {
                for(osg::Camera::BufferAttachmentMap::iterator itr = bufferAttachments.begin();
                    itr != bufferAttachments.end();
                    ++itr)
                {
                    osg::Camera::Attachment& attachment = itr->second;
                    samples = maximum(samples, attachment._multisampleSamples);
                    colorSamples = maximum(colorSamples, attachment._multisampleColorSamples);
                }

                if (colorSamples > samples)
                {
                    OSG_NOTIFY(WARN) << "Multisample color samples must be less than or "
                        "equal to samples. Setting color samples equal to samples." << std::endl;
                    colorSamples = samples;
                }

                if (samples)
                {
                    fbo_multisample = new osg::FrameBufferObject;
                    fbo_multisample->setName(_camera->getName() + "_ms_");

                    // Use the value of the Camera's use resolve buffers mask as the
                    // resolve mask.
                    resolveBuffersMask = _camera->getImplicitBufferAttachmentResolveMask(true);
                }
            }

            for(osg::Camera::BufferAttachmentMap::iterator itr = bufferAttachments.begin();
                itr != bufferAttachments.end();
                ++itr)
            {

                osg::Camera::BufferComponent buffer = itr->first;
                osg::Camera::Attachment& attachment = itr->second;

                if (attachment._texture.valid() || attachment._image.valid())
                    fbo->setAttachment(buffer, osg::FrameBufferAttachment(attachment));
                else
                    fbo->setAttachment(buffer, osg::FrameBufferAttachment(new osg::RenderBuffer(width, height, attachment._internalFormat)));

                if (fbo_multisample.valid())
                {
                    GLenum internalFormat = attachment._internalFormat;
                    if (!internalFormat)
                    {
                        switch (buffer)
                        {
                        case Camera::DEPTH_BUFFER:
                            internalFormat = GL_DEPTH_COMPONENT24;
                            break;
                        case Camera::STENCIL_BUFFER:
                            internalFormat = GL_STENCIL_INDEX8_EXT;
                            break;
                        case Camera::PACKED_DEPTH_STENCIL_BUFFER:
                            internalFormat = GL_DEPTH_STENCIL_EXT;
                            break;

                        // all other buffers are color buffers
                        default:
                            // setup the internal format based on attached texture if such exists, otherwise just default format
                            if (attachment._texture)
                                internalFormat = attachment._texture->getInternalFormat();
                            else
                                internalFormat = GL_RGBA;
                            break;
                        }
                    }

                    // VRV_PATCH BEGIN
                    if(!isArray)
                    {
                        fbo_multisample->setAttachment(buffer,
                            osg::FrameBufferAttachment(new osg::RenderBuffer(
                            width, height, internalFormat,
                            samples, colorSamples)));
                    }
                    else
                    {
                        if(ext->isTextureMultisampledSupported)
                        {
                            osg::Texture2DMultisampleArray* multiSampleTexArray = new osg::Texture2DMultisampleArray(width, height, depth, internalFormat, samples, GL_FALSE);
                            fbo_multisample->setAttachment(buffer, osg::FrameBufferAttachment(multiSampleTexArray, attachment._face, 0));

                            osg::Texture2DArray* attachmentAsTex2dArray = dynamic_cast<osg::Texture2DArray*>(attachment._texture.get());

                            // make a read and draw fbos for each layer so we can resolve later
                            for(unsigned int i=0; i<depth; i++)
                            {
                                osg::ref_ptr<osg::FrameBufferObject> layerfbo;
                                osg::ref_ptr<osg::FrameBufferObject> resolvelayerfbo;

                                if(_arrayLayerFbos.size() <= i)
                                {
                                    layerfbo = new osg::FrameBufferObject;
                                    layerfbo->setName(_camera->getName() + "_layer_");
                                    _arrayLayerFbos.push_back(layerfbo);
                                }
                                else
                                {
                                    layerfbo = _arrayLayerFbos[i];
                                }

                                if (_resolveArrayLayerFbos.size() <= i)
                                {
                                    resolvelayerfbo = new osg::FrameBufferObject;
                                    resolvelayerfbo->setName(_camera->getName() + "_resolvelayer_");
                                    _resolveArrayLayerFbos.push_back(resolvelayerfbo);
                                }
                                else
                                {
                                    resolvelayerfbo = _resolveArrayLayerFbos[i];
                                }

                                resolvelayerfbo->setAttachment(buffer, osg::FrameBufferAttachment(attachmentAsTex2dArray, i, 0));
                                layerfbo->setAttachment(buffer, osg::FrameBufferAttachment(multiSampleTexArray, i, 0));
                            }
                        }
                        else
                        {
                            fbo_multisample = NULL;
                        }
                    }
                    // VRV_PATCH END
                }

                if (buffer==osg::Camera::DEPTH_BUFFER) depthAttached = true;
                else if (buffer==osg::Camera::STENCIL_BUFFER) stencilAttached = true;
                else if (buffer==osg::Camera::PACKED_DEPTH_STENCIL_BUFFER)
                {
                    depthAttached = true;
                    stencilAttached = true;
                }
                else if (buffer>=Camera::COLOR_BUFFER) colorAttached = true;

            }

            if (!depthAttached)
            {
                // If doing MSFBO (and therefore need two FBOs, one for multisampled rendering and one for
                // final resolve), then configure "fbo" as the resolve FBO, and When done
                // configuring, swap it into "_resolveFbo" (see line 554). But, if not
                // using MSFBO, then "fbo" is just the render fbo.
                // If using MSFBO, then resolveBuffersMask
                // is the value set by the app for the resolve buffers. But if not using
                // MSFBO, then resolveBuffersMask is the value set by the app for render
                // buffers. In both cases, resolveBuffersMask is used to configure "fbo".
                if( resolveBuffersMask & osg::Camera::IMPLICIT_DEPTH_BUFFER_ATTACHMENT )
                {
                    osg::RenderBuffer * rb = new osg::RenderBuffer(width, height, GL_DEPTH_COMPONENT24);
                    rb->setName(_name + "_autogen_rb");
                    fbo->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(rb));
                    depthAttached = true;
                }
                if (fbo_multisample.valid() &&
                    ( renderBuffersMask & osg::Camera::IMPLICIT_DEPTH_BUFFER_ATTACHMENT ) )
                {
                    osg::RenderBuffer * rb = new osg::RenderBuffer(width, height, GL_DEPTH_COMPONENT24, samples, colorSamples);
                    rb->setName(_name + "_autogen_ms_rb");
                    fbo_multisample->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(rb));
                }
            }
            if (!stencilAttached)
            {
                if( resolveBuffersMask & osg::Camera::IMPLICIT_STENCIL_BUFFER_ATTACHMENT )
                {
                    fbo->setAttachment(osg::Camera::STENCIL_BUFFER, osg::FrameBufferAttachment(new osg::RenderBuffer(width, height, GL_STENCIL_INDEX8_EXT)));
                    stencilAttached = true;
                }
                if (fbo_multisample.valid() &&
                    ( renderBuffersMask & osg::Camera::IMPLICIT_STENCIL_BUFFER_ATTACHMENT ) )
                {
                    fbo_multisample->setAttachment(osg::Camera::STENCIL_BUFFER,
                        osg::FrameBufferAttachment(new osg::RenderBuffer(width,
                        height, GL_STENCIL_INDEX8_EXT, samples, colorSamples)));
                }
            }

            if (!colorAttached)
            {
                if( resolveBuffersMask & osg::Camera::IMPLICIT_COLOR_BUFFER_ATTACHMENT )
                {
                    fbo->setAttachment(Camera::COLOR_BUFFER, osg::FrameBufferAttachment(new osg::RenderBuffer(width, height, GL_RGB)));
                    colorAttached = true;
                }
                if (fbo_multisample.valid() &&
                    ( renderBuffersMask & osg::Camera::IMPLICIT_COLOR_BUFFER_ATTACHMENT ) )
                {
                    fbo_multisample->setAttachment(Camera::COLOR_BUFFER,
                        osg::FrameBufferAttachment(new osg::RenderBuffer(width,
                        height, GL_RGB, samples, colorSamples)));
                }
            }

            fbo->apply(state);

            // If no color attachment make sure to set glDrawBuffer/glReadBuffer to none
            // otherwise glCheckFramebufferStatus will fail
            // It has to be done after call to glBindFramebuffer (fbo->apply)
            // and before call to glCheckFramebufferStatus
            if ( !colorAttached )
            {
            #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE)
                setDrawBuffer( GL_NONE, true );
                setReadBuffer( GL_NONE, true );
                state.glDrawBuffer( GL_NONE );
                state.glReadBuffer( GL_NONE );
            #endif
            }

            GLenum status = ext->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);

            if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
            {
                OSG_WARN<<"\nRenderStage::runCameraSetUp(), FBO setup failed, FBO status= 0x"<<std::hex<<status<<std::dec<<std::endl;
                if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT){
                   OSG_WARN << "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT" << std::endl;
                }
                else if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT){
                   OSG_WARN << "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT" << std::endl;
                }
                else if (status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT){
                   OSG_WARN << "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS" << std::endl;
                }
                else if (status == GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT){
                   OSG_WARN << "GL_FRAMEBUFFER_INCOMPLETE_FORMATS" << std::endl;
                }                
                else if (status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT){
                   OSG_WARN << "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER" << std::endl;
                }
                else if (status == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT){
                   OSG_WARN << "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER" << std::endl;
                }
                else if (status == GL_FRAMEBUFFER_UNSUPPORTED_EXT){
                   OSG_WARN << "GL_FRAMEBUFFER_UNSUPPORTED" << std::endl;
                }
                else if (status == GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS){
                   OSG_WARN << "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS" << std::endl;
                }

                OSG_WARN << "Camera '"<< _camera->getName() << "'" << std::endl;
                OSG_WARN << "  width:" << width << std::endl;
                OSG_WARN << "  height:" << height << std::endl;
                OSG_WARN << "  colorAttached: " << (colorAttached ? "Yes" : "No") << std::endl;
                OSG_WARN << "  depthAttached: " << (depthAttached ? "Yes" : "No") << std::endl;
                OSG_WARN << "  stencilAttached: " << (stencilAttached ? "Yes" : "No") << std::endl;
                OSG_WARN << "  samples: " << samples << std::endl;
                OSG_WARN << "  colorSamples: " << colorSamples << std::endl;

                OSG_WARN << "FBO '" << fbo->getName() << "' fbo details" << std::endl;

                dumpFboInfo(fbo);


                fbo_supported = false;
                GLuint fboId = state.getGraphicsContext() ? state.getGraphicsContext()->getDefaultFboId() : 0;
                ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, fboId);
                state.setLastAppliedFboId(fboId); //save off the current framebuffer in the state

                fbo = 0;

                // clean up.
                osg::get<osg::GLRenderBufferManager>(state.getContextID())->flushAllDeletedGLObjects();
                osg::get<osg::GLFrameBufferObjectManager>(state.getContextID())->flushAllDeletedGLObjects();
            }
            else
            {
                //OSG_WARN << "Camera '" << _camera->getName() << "' fbo details" << std::endl;

                //dumpFboInfo(fbo);
               
                setDrawBuffer(GL_NONE, false);
                setReadBuffer(GL_NONE, false );

                _fbo = fbo;

                if (fbo_multisample.valid())
                {
                    fbo_multisample->apply(state);

                    status = ext->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
                    if (status != GL_FRAMEBUFFER_COMPLETE_EXT)
                    {
                        OSG_NOTICE << "RenderStage::runCameraSetUp(), "
                            "multisample FBO setup failed, FBO status = 0x"
                            << std::hex << status << std::dec << std::endl;

                        dumpFboInfo(fbo_multisample);

                        fbo->apply(state);
                        fbo_multisample = 0;
                        _resolveFbo = 0;

                        // clean up.
                        osg::get<osg::GLRenderBufferManager>(state.getContextID())->flushAllDeletedGLObjects();
                        osg::get<osg::GLFrameBufferObjectManager>(state.getContextID())->flushAllDeletedGLObjects();
                    }
                    else
                    {
                        _resolveFbo.swap(_fbo);
                        _fbo = fbo_multisample;
                    }
                }
                else
                {
                    _resolveFbo = 0;
                }
            }
        }

        if (!fbo_supported)
        {
            if (renderTargetImplementation<renderTargetFallback)
                renderTargetImplementation = renderTargetFallback;
            else
                renderTargetImplementation = osg::Camera::PIXEL_BUFFER_RTT;
        }
    }

    // check whether PBuffer-RTT is supported or not
    if (renderTargetImplementation==osg::Camera::PIXEL_BUFFER_RTT &&
        !osg::isGLExtensionSupported(state.getContextID(), "WGL_ARB_render_texture"))
    {
        if (renderTargetImplementation<renderTargetFallback)
            renderTargetImplementation = renderTargetFallback;
        else
            renderTargetImplementation = osg::Camera::PIXEL_BUFFER;
    }

    // if any of the renderTargetImplementations require a separate graphics context such as with pbuffer try in turn to
    // set up, but if each level fails then resort to the next level down.
    while (!getGraphicsContext() &&
           (renderTargetImplementation==osg::Camera::PIXEL_BUFFER_RTT ||
            renderTargetImplementation==osg::Camera::PIXEL_BUFFER ||
            renderTargetImplementation==osg::Camera::SEPARATE_WINDOW) )
    {
        osg::ref_ptr<osg::GraphicsContext> context = getGraphicsContext();
        if (!context)
        {

            // set up the traits of the graphics context that we want
            osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;

            traits->width = width;
            traits->height = height;

            // OSG_NOTICE<<"traits = "<<traits->width<<" "<<traits->height<<std::endl;

            traits->pbuffer = (renderTargetImplementation==osg::Camera::PIXEL_BUFFER || renderTargetImplementation==osg::Camera::PIXEL_BUFFER_RTT);
            traits->windowDecoration = (renderTargetImplementation==osg::Camera::SEPARATE_WINDOW);
            traits->doubleBuffer = (renderTargetImplementation==osg::Camera::SEPARATE_WINDOW);

            osg::Texture* pBufferTexture = 0;
            GLenum bufferFormat = GL_NONE;
            unsigned int level = 0;
            unsigned int face = 0;

            bool colorAttached = false;
            bool depthAttached = false;
            for(osg::Camera::BufferAttachmentMap::iterator itr = bufferAttachments.begin();
                itr != bufferAttachments.end();
                ++itr)
            {

                osg::Camera::BufferComponent buffer = itr->first;
                osg::Camera::Attachment& attachment = itr->second;
                switch(buffer)
                {
                    case(osg::Camera::DEPTH_BUFFER):
                    {
                        traits->depth = 24;
                        depthAttached = true;
                        break;
                    }
                    case(osg::Camera::STENCIL_BUFFER):
                    {
                        traits->stencil = 8;
                        break;
                    }
                    case(osg::Camera::PACKED_DEPTH_STENCIL_BUFFER):
                    {
                        traits->depth = 24;
                        depthAttached = true;
                        traits->stencil = 8;
                        break;
                    }
                    case(Camera::COLOR_BUFFER):
                    {
                        if (attachment._internalFormat!=GL_NONE)
                        {
                            bufferFormat = attachment._internalFormat;
                        }
                        else
                        {
                            if (attachment._texture.valid())
                            {
                                pBufferTexture = attachment._texture.get();
                                bufferFormat = attachment._texture->getInternalFormat();
                            }
                            else if (attachment._image.valid())
                            {
                                bufferFormat = attachment._image->getInternalTextureFormat();
                            }
                            else
                            {
                                bufferFormat = GL_RGBA;
                            }
                        }

                        level = attachment._level;
                        face = attachment._face;

                        if (renderTargetImplementation==osg::Camera::PIXEL_BUFFER_RTT)
                        {
                            traits->target = attachment._texture.valid() ? attachment._texture->getTextureTarget() : 0;
                            traits->format = bufferFormat;
                            traits->level = level;
                            traits->face = face;
                            traits->mipMapGeneration = attachment._mipMapGeneration;
                        }
                        break;
                    }
                    default:
                    {
                        if (renderTargetImplementation==osg::Camera::SEPARATE_WINDOW)
                        {
                            OSG_NOTICE<<"Warning: RenderStage::runCameraSetUp(State&) Window ";
                        }
                        else
                        {
                            OSG_NOTICE<<"Warning: RenderStage::runCameraSetUp(State&) Pbuffer ";
                        }

                        OSG_NOTICE<<"does not support multiple color outputs."<<std::endl;
                        break;
                    }

                }
            }

            if (!depthAttached)
            {
                traits->depth = 24;
            }

            if (!colorAttached)
            {
                if (bufferFormat == GL_NONE) bufferFormat = GL_RGB;

                traits->red = 8;
                traits->green = 8;
                traits->blue = 8;
                traits->alpha = (bufferFormat==GL_RGBA) ? 8 : 0;
            }

            // share OpenGL objects if possible...
            if (state.getGraphicsContext())
            {
                traits->sharedContext = state.getGraphicsContext();

                const osg::GraphicsContext::Traits* sharedTraits = traits->sharedContext->getTraits();
                if (sharedTraits)
                {
                    traits->hostName = sharedTraits->hostName;
                    traits->displayNum = sharedTraits->displayNum;
                    traits->screenNum = sharedTraits->screenNum;
                }
            }

            // create the graphics context according to these traits.
            context = osg::GraphicsContext::createGraphicsContext(traits.get());

            if (context.valid() && context->realize())
            {
                OSG_INFO<<"RenderStage::runCameraSetUp(State&) Context has been realized "<<std::endl;

                // successfully set up graphics context as requested,
                // will assign this graphics context to the RenderStage and
                // associated parameters.  Setting the graphics context will
                // single this while loop to exit successful.
                setGraphicsContext(context.get());

                // how to do we detect that an attempt to set up RTT has failed??

                setDrawBuffer(GL_FRONT);
                setReadBuffer(GL_FRONT);

                if (pBufferTexture && renderTargetImplementation==osg::Camera::PIXEL_BUFFER_RTT)
                {
                   OSG_INFO<<"RenderStage::runCameraSetUp(State&) Assign graphics context to Texture"<<std::endl;
                   pBufferTexture->setReadPBuffer(context.get());
                }
                else
                {
                    OSG_INFO<<"RenderStage::runCameraSetUp(State&) Assigning texture to RenderStage so that it does the copy"<<std::endl;
                    setTexture(pBufferTexture, level, face);
                }
            }
            else
            {
                OSG_INFO<<"Failed to acquire Graphics Context"<<std::endl;

                if (renderTargetImplementation==osg::Camera::PIXEL_BUFFER_RTT)
                {
                    // fallback to using standard PBuffer, this will allow this while loop to continue
                    if (renderTargetImplementation<renderTargetFallback)
                        renderTargetImplementation = renderTargetFallback;
                    else
                        renderTargetImplementation = osg::Camera::PIXEL_BUFFER;
                }
                else
                {
                    renderTargetImplementation = osg::Camera::FRAME_BUFFER;
                }
            }

        }
    }

    // finally if all else has failed, then the frame buffer fallback will come in to play.
    if (renderTargetImplementation==osg::Camera::FRAME_BUFFER)
    {
        OSG_INFO<<"Setting up osg::Camera::FRAME_BUFFER"<<std::endl;

        for(osg::Camera::BufferAttachmentMap::iterator itr = bufferAttachments.begin();
            itr != bufferAttachments.end();
            ++itr)
        {
            // assign the texture...
            if (itr->second._texture.valid()) setTexture(itr->second._texture.get(), itr->second._level, itr->second._face);
        }
    }

}

void RenderStage::copyTexture(osg::RenderInfo& renderInfo)
{
    osg::State& state = *renderInfo.getState();

    if ( _readBufferApplyMask )
    {
        #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE)
            glReadBuffer(_readBuffer);
        #endif
    }

    // need to implement texture cube map etc...
    osg::Texture1D* texture1D = 0;
    osg::Texture2D* texture2D = 0;
    osg::Texture3D* texture3D = 0;
    osg::TextureRectangle* textureRec = 0;
    osg::TextureCubeMap* textureCubeMap = 0;

    // use TexCopySubImage with the offset of the viewport into the texture
    // note, this path mirrors the pbuffer and fbo means for updating the texture.
    // Robert Osfield, 3rd August 2006.
    if ((texture2D = dynamic_cast<osg::Texture2D*>(_texture.get())) != 0)
    {
        texture2D->copyTexSubImage2D(state,
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     static_cast<int>(_viewport->width()),
                                     static_cast<int>(_viewport->height()));
    }
    else if ((textureRec = dynamic_cast<osg::TextureRectangle*>(_texture.get())) != 0)
    {
        textureRec->copyTexSubImage2D(state,
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     static_cast<int>(_viewport->width()),
                                     static_cast<int>(_viewport->height()));
    }
    else if ((texture1D = dynamic_cast<osg::Texture1D*>(_texture.get())) != 0)
    {
        // need to implement
        texture1D->copyTexSubImage1D(state,
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     static_cast<int>(_viewport->width()));
    }
    else if ((texture3D = dynamic_cast<osg::Texture3D*>(_texture.get())) != 0)
    {
        // need to implement
        texture3D->copyTexSubImage3D(state,
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     _face,
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     static_cast<int>(_viewport->width()),
                                     static_cast<int>(_viewport->height()));
    }
    else if ((textureCubeMap = dynamic_cast<osg::TextureCubeMap*>(_texture.get())) != 0)
    {
        // need to implement
        textureCubeMap->copyTexSubImageCubeMap(state, _face,
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     static_cast<int>(_viewport->x()),
                                     static_cast<int>(_viewport->y()),
                                     static_cast<int>(_viewport->width()),
                                     static_cast<int>(_viewport->height()));
    }
}

void RenderStage::drawInner(osg::RenderInfo& renderInfo,RenderLeaf*& previous, bool& doCopyTexture)
{
    struct SubFunc
    {
        static void applyReadFBO(bool& apply_read_fbo,
            const FrameBufferObject* read_fbo, osg::State& state)
        {
            if (read_fbo->isMultisample())
            {
                OSG_WARN << "Attempting to read from a"
                    " multisampled framebuffer object. Set a resolve"
                    " framebuffer on the RenderStage to fix this." << std::endl;
            }

            if (apply_read_fbo)
            {
                // Bind the monosampled FBO to read from
                read_fbo->apply(state, FrameBufferObject::READ_FRAMEBUFFER);
                apply_read_fbo = false;
            }
        }
    };

    osg::State& state = *renderInfo.getState();

    osg::GLExtensions* ext = _fbo.valid() ? state.get<osg::GLExtensions>() : 0;
    bool fbo_supported = ext && ext->isFrameBufferObjectSupported;

    bool using_multiple_render_targets = fbo_supported && _fbo->hasMultipleRenderingTargets();

    if (!using_multiple_render_targets)
    {
        #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE)

            if( getDrawBufferApplyMask() )
                state.glDrawBuffer(_drawBuffer);

            if( getReadBufferApplyMask() )
                state.glReadBuffer(_readBuffer);

        #endif
    }

    if (fbo_supported)
    {
        _fbo->apply(state);
    }

    // do the drawing itself.
    RenderBin::draw(renderInfo,previous);


    if(state.getCheckForGLErrors()!=osg::State::NEVER_CHECK_GL_ERRORS)
    {
        if (state.checkGLErrors("after RenderBin::draw(..)"))
        {
            if ( ext )
            {
                GLenum fbstatus = ext->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
                if ( fbstatus != GL_FRAMEBUFFER_COMPLETE_EXT )
                {
                    OSG_NOTICE<<"RenderStage::drawInner(,) FBO status = 0x"<<std::hex<<fbstatus<<std::dec<<std::endl;
                }
            }
        }
    }

    const FrameBufferObject* read_fbo = fbo_supported ? _fbo.get() : 0;
    bool apply_read_fbo = false;

    if (fbo_supported && _resolveFbo.valid() && ext->glBlitFramebuffer)
    {
        bool isArray = _resolveFbo->getAttachmentMap().begin()->second.isArray(); // VRV_PATCH

        GLbitfield blitMask = 0;
        bool needToBlitColorBuffers = false;

        //find which buffer types should be copied
        for (FrameBufferObject::AttachmentMap::const_iterator
            it = _resolveFbo->getAttachmentMap().begin(),
            end =_resolveFbo->getAttachmentMap().end(); it != end; ++it)
        {
            switch (it->first)
            {
            case Camera::DEPTH_BUFFER:
                blitMask |= GL_DEPTH_BUFFER_BIT;
                break;
            case Camera::STENCIL_BUFFER:
                blitMask |= GL_STENCIL_BUFFER_BIT;
                break;
            case Camera::PACKED_DEPTH_STENCIL_BUFFER:
                blitMask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
                break;
            //VRV PATCH BEGINS
            case Camera::COLOR_BUFFER0:
            //VRV PATCH ENDS
            case Camera::COLOR_BUFFER:
                blitMask |= GL_COLOR_BUFFER_BIT;
                break;
            default:
                needToBlitColorBuffers = true;
                break;
            }
        }

        if (!isArray) // VRV_PATCH
        {

            // Bind the resolve framebuffer to blit into.
            _fbo->apply(state, FrameBufferObject::READ_FRAMEBUFFER);
            _resolveFbo->apply(state, FrameBufferObject::DRAW_FRAMEBUFFER);

            if (blitMask)
            {
                // Blit to the resolve framebuffer.
                // Note that (with nvidia 175.16 windows drivers at least) if the read
                // framebuffer is multisampled then the dimension arguments are ignored
                // and the whole framebuffer is always copied.
                ext->glBlitFramebuffer(
                    static_cast<GLint>(_viewport->x()), static_cast<GLint>(_viewport->y()),
                    static_cast<GLint>(_viewport->x() + _viewport->width()), static_cast<GLint>(_viewport->y() + _viewport->height()),
                    static_cast<GLint>(_viewport->x()), static_cast<GLint>(_viewport->y()),
                    static_cast<GLint>(_viewport->x() + _viewport->width()), static_cast<GLint>(_viewport->y() + _viewport->height()),
                    blitMask, GL_NEAREST);
            }

#if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE)
            if (needToBlitColorBuffers)
            {
                for (FrameBufferObject::AttachmentMap::const_iterator
                    it = _resolveFbo->getAttachmentMap().begin(),
                    end =_resolveFbo->getAttachmentMap().end(); it != end; ++it)
                {
                    osg::Camera::BufferComponent attachment = it->first;
                    if (attachment >= osg::Camera::COLOR_BUFFER0)
                    {
                        glReadBuffer(GL_COLOR_ATTACHMENT0_EXT + (attachment - osg::Camera::COLOR_BUFFER0));
                        glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + (attachment - osg::Camera::COLOR_BUFFER0));

                        ext->glBlitFramebuffer(
                            static_cast<GLint>(_viewport->x()), static_cast<GLint>(_viewport->y()),
                            static_cast<GLint>(_viewport->x() + _viewport->width()), static_cast<GLint>(_viewport->y() + _viewport->height()),
                            static_cast<GLint>(_viewport->x()), static_cast<GLint>(_viewport->y()),
                            static_cast<GLint>(_viewport->x() + _viewport->width()), static_cast<GLint>(_viewport->y() + _viewport->height()),
                            GL_COLOR_BUFFER_BIT, GL_NEAREST);
                    }
                }
                // reset the read and draw buffers?  will comment out for now with the assumption that
                // the buffers will be set explicitly when needed elsewhere.
                // glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
                // glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
            }
#endif
        }
        else
        {
            // VRV_PATCH BEGIN
            if (blitMask)
            {
                for(unsigned int i = 0; i < _resolveArrayLayerFbos.size(); i++)
                {
                    //_arrayLayerFbos[i]->dirtyAll();
                    _arrayLayerFbos[i]->apply(state, FrameBufferObject::READ_FRAMEBUFFER);
                    _resolveArrayLayerFbos[i]->apply(state, FrameBufferObject::DRAW_FRAMEBUFFER);

                    ext->glBlitFramebuffer(
                        static_cast<GLint>(_viewport->x()), static_cast<GLint>(_viewport->y()),
                        static_cast<GLint>(_viewport->x() + _viewport->width()), static_cast<GLint>(_viewport->y() + _viewport->height()),
                        static_cast<GLint>(_viewport->x()), static_cast<GLint>(_viewport->y()),
                        static_cast<GLint>(_viewport->x() + _viewport->width()), static_cast<GLint>(_viewport->y() + _viewport->height()),
                        blitMask, GL_NEAREST);
                }
            }
            // VRV_PATCH END
        }
        
        apply_read_fbo = true;
        read_fbo = _resolveFbo.get();

        using_multiple_render_targets = read_fbo->hasMultipleRenderingTargets();
    }

    // now copy the rendered image to attached texture.
    if (doCopyTexture)
    {
        if (read_fbo) SubFunc::applyReadFBO(apply_read_fbo, read_fbo, state);
        copyTexture(renderInfo);
    }

    for(std::map< osg::Camera::BufferComponent, Attachment>::const_iterator itr = _bufferAttachmentMap.begin();
        itr != _bufferAttachmentMap.end();
        ++itr)
    {
        if (itr->second._image.valid())
        {
            if (read_fbo) SubFunc::applyReadFBO(apply_read_fbo, read_fbo, state);

            #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE)

                if (using_multiple_render_targets)
                {
                    int attachment=itr->first;
                    if (attachment==osg::Camera::DEPTH_BUFFER || attachment==osg::Camera::STENCIL_BUFFER) {
                        // assume first buffer rendered to is the one we want
                        glReadBuffer(read_fbo->getMultipleRenderingTargets()[0]);
                    } else {
                        glReadBuffer(GL_COLOR_ATTACHMENT0_EXT + (attachment - osg::Camera::COLOR_BUFFER0));
                    }
                } else {
                    if (_readBuffer != GL_NONE)
                    {
                        glReadBuffer(_readBuffer);
                    }
                }

            #endif

            GLenum pixelFormat = itr->second._image->getPixelFormat();
            if (pixelFormat==0) pixelFormat = _imageReadPixelFormat;
            if (pixelFormat==0) pixelFormat = GL_RGB;

            GLenum dataType = itr->second._image->getDataType();
            if (dataType==0) dataType = _imageReadPixelDataType;
            if (dataType==0) dataType = GL_UNSIGNED_BYTE;

            itr->second._image->readPixels(static_cast<int>(_viewport->x()),
                                           static_cast<int>(_viewport->y()),
                                           static_cast<int>(_viewport->width()),
                                           static_cast<int>(_viewport->height()),
                                           pixelFormat, dataType);
        }
    }

    if (fbo_supported)
    {
        if (getDisableFboAfterRender())
        {
            // switch off the frame buffer object
            GLuint fboId = state.getGraphicsContext() ? state.getGraphicsContext()->getDefaultFboId() : 0;
            ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, fboId);
            state.setLastAppliedFboId(fboId); //save off the current framebuffer in the state

        }

        doCopyTexture = true;
    }

    if (fbo_supported && _camera.valid())
    {
        // now generate mipmaps if they are required.
        const osg::Camera::BufferAttachmentMap& bufferAttachments = _camera->getBufferAttachmentMap();
        for(osg::Camera::BufferAttachmentMap::const_iterator itr = bufferAttachments.begin();
            itr != bufferAttachments.end();
            ++itr)
        {
            if (itr->second._texture.valid() && itr->second._mipMapGeneration)
            {
                state.setActiveTextureUnit(0);
                state.applyTextureAttribute(0, itr->second._texture.get());
                ext->glGenerateMipmap(itr->second._texture->getTextureTarget());
            }
        }
    }
}

struct DrawInnerOperation : public osg::Operation
{
    DrawInnerOperation(RenderStage* stage, osg::RenderInfo& renderInfo) :
        osg::Referenced(true),
        osg::Operation("DrawInnerStage",false),
        _stage(stage),
        _renderInfo(renderInfo) {}

    virtual void operator () (osg::Object* object)
    {
        osg::GraphicsContext* context = dynamic_cast<osg::GraphicsContext*>(object);
        if (!context) return;

        // OSG_NOTICE<<"DrawInnerOperation operator"<<std::endl;
        if (_stage && context)
        {
            RenderLeaf* previous = 0;
            bool doCopyTexture = false;
            _renderInfo.setState(context->getState());
            _stage->drawInner(_renderInfo, previous, doCopyTexture);
        }
    }

    RenderStage* _stage;
    RenderInfo _renderInfo;
};


void RenderStage::draw(osg::RenderInfo& renderInfo,RenderLeaf*& previous)
{
    // VRV_PATCH
    OsgProfile(getName().length() > 0 ? getName().c_str() : "RenderStage::draw");

    if (_stageDrawnThisFrame) return;

    if(_initialViewMatrix.valid()) renderInfo.getState()->setInitialViewMatrix(_initialViewMatrix.get());

    // push the stages camera so that drawing code can query it
    if (_camera.valid()) renderInfo.pushCamera(_camera.get());

    _stageDrawnThisFrame = true;

    if (_camera.valid() && _camera->getInitialDrawCallback())
    {
        // if we have a camera with a initial draw callback invoke it.
        _camera->getInitialDrawCallback()->run(renderInfo);
    }

    // note, SceneView does call to drawPreRenderStages explicitly
    // so there is no need to call it here.
    drawPreRenderStages(renderInfo,previous);

    if (_cameraRequiresSetUp || (_camera.valid() && _cameraAttachmentMapModifiedCount!=_camera->getAttachmentMapModifiedCount()))
    {
        runCameraSetUp(renderInfo);
    }

    osg::State& state = *renderInfo.getState();

    osg::State* useState = &state;
    osg::GraphicsContext* callingContext = state.getGraphicsContext();
    osg::GraphicsContext* useContext = callingContext;
    osg::OperationThread* useThread = 0;
    osg::RenderInfo useRenderInfo(renderInfo);

    RenderLeaf* saved_previous = previous;

    if (_graphicsContext.valid() && _graphicsContext != callingContext)
    {
        // show we release the context so that others can use it?? will do so right
        // now as an experiment.
        callingContext->releaseContext();

        // OSG_NOTICE<<"  enclosing state before - "<<state.getStateSetStackSize()<<std::endl;

        useState = _graphicsContext->getState();
        useContext = _graphicsContext.get();
        useThread = useContext->getGraphicsThread();
        useRenderInfo.setState(useState);

        // synchronize the frame stamps
        useState->setFrameStamp(const_cast<osg::FrameStamp*>(state.getFrameStamp()));

        // map the DynamicObjectCount across to the new window
        useState->setDynamicObjectCount(state.getDynamicObjectCount());
        useState->setDynamicObjectRenderingCompletedCallback(state.getDynamicObjectRenderingCompletedCallback());

        if (!useThread)
        {
            previous = 0;
            useContext->makeCurrent();

            // OSG_NOTICE<<"  nested state before - "<<useState->getStateSetStackSize()<<std::endl;
        }
    }

    unsigned int originalStackSize = useState->getStateSetStackSize();

    if (_camera.valid() && _camera->getPreDrawCallback())
    {
        // if we have a camera with a pre draw callback invoke it.
        _camera->getPreDrawCallback()->run(renderInfo);
    }

    bool doCopyTexture = _texture.valid() ?
                        (callingContext != useContext) :
                        false;

    if (useThread)
    {
#if 1
        ref_ptr<osg::BlockAndFlushOperation> block = new osg::BlockAndFlushOperation;

        useThread->add(new DrawInnerOperation( this, renderInfo ));

        useThread->add(block.get());

        // wait till the DrawInnerOperations is complete.
        block->block();

        doCopyTexture = false;

#else
        useThread->add(new DrawInnerOperation( this, renderInfo ), true);

        doCopyTexture = false;
#endif
    }
    else
    {
        drawInner( useRenderInfo, previous, doCopyTexture);

        if (useRenderInfo.getUserData() != renderInfo.getUserData())
        {
            renderInfo.setUserData(useRenderInfo.getUserData());
        }

    }

    if (useState != &state)
    {
        // reset the local State's DynamicObjectCount
        state.setDynamicObjectCount(useState->getDynamicObjectCount());
        useState->setDynamicObjectRenderingCompletedCallback(0);
    }


    // now copy the rendered image to attached texture.
    if (_texture.valid() && !doCopyTexture)
    {
        if (callingContext && useContext!= callingContext)
        {
            // make the calling context use the pbuffer context for reading.
            callingContext->makeContextCurrent(useContext);
        }

        copyTexture(renderInfo);
    }

    if (_camera.valid() && _camera->getPostDrawCallback())
    {
        // if we have a camera with a post draw callback invoke it.
        _camera->getPostDrawCallback()->run(renderInfo);
    }

    if (_graphicsContext.valid() && _graphicsContext != callingContext)
    {
        useState->popStateSetStackToSize(originalStackSize);

        if (!useThread)
        {


            // flush any command left in the useContex's FIFO
            // to ensure that textures are updated before main thread commenses.
            glFlush();


            useContext->releaseContext();
        }
    }

    if (callingContext && useContext != callingContext)
    {
        // restore the graphics context.

        previous = saved_previous;

        // OSG_NOTICE<<"  nested state after - "<<useState->getStateSetStackSize()<<std::endl;
        // OSG_NOTICE<<"  enclosing state after - "<<state.getStateSetStackSize()<<std::endl;

        callingContext->makeCurrent();
    }

    // render all the post draw callbacks
    drawPostRenderStages(renderInfo,previous);

    if (_camera.valid() && _camera->getFinalDrawCallback())
    {
        // if we have a camera with a final callback invoke it.
        _camera->getFinalDrawCallback()->run(renderInfo);
    }

    // pop the render stages camera.
    if (_camera.valid()) renderInfo.popCamera();

    // clean up state graph to make sure RenderLeaf etc, can be reused
    if (_rootStateGraph.valid()) _rootStateGraph->clean();
}

void RenderStage::drawImplementation(osg::RenderInfo& renderInfo,RenderLeaf*& previous)
{
    osg::State& state = *renderInfo.getState();

    if (!_viewport)
    {
        OSG_FATAL << "Error: cannot draw stage due to undefined viewport."<< std::endl;
        return;
    }

    // set up the back buffer.
    state.applyAttribute(_viewport.get());
    
    // vantage patch for multicast shadows
    if (_viewport.get()->numViewports() == 1)
    {
       glScissor(static_cast<int>(_viewport->x()),
          static_cast<int>(_viewport->y()),
          static_cast<int>(_viewport->width()),
          static_cast<int>(_viewport->height()));
       //cout << "    clearing "<<this<< "  "<<_viewport->x()<<","<< _viewport->y()<<","<< _viewport->width()<<","<< _viewport->height()<<std::endl;
       state.applyMode(GL_SCISSOR_TEST, true);
    }
    else{
       // vantage patch for multicast shadows
       state.applyMode(GL_SCISSOR_TEST, false);
    }
    // glEnable( GL_DEPTH_TEST );

    // set which color planes to operate on.
    if (_colorMask.valid()) _colorMask->apply(state);
    else glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

    if (_clearMask != 0)
    {
        if (_clearMask & GL_COLOR_BUFFER_BIT)
        {
            glClearColor( _clearColor[0], _clearColor[1], _clearColor[2], _clearColor[3]);
        }

        if (_clearMask & GL_DEPTH_BUFFER_BIT)
        {
            #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE)
                glClearDepth( _clearDepth);
            #else
                glClearDepthf( _clearDepth);
            #endif

            glDepthMask ( GL_TRUE );
            state.haveAppliedAttribute( osg::StateAttribute::DEPTH );
        }

        if (_clearMask & GL_STENCIL_BUFFER_BIT)
        {
            glClearStencil( _clearStencil);
            glStencilMask ( ~0u );
            state.haveAppliedAttribute( osg::StateAttribute::STENCIL );
        }

        #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE) && !defined(OSG_GL3_AVAILABLE)
            if (_clearMask & GL_ACCUM_BUFFER_BIT)
            {
                glClearAccum( _clearAccum[0], _clearAccum[1], _clearAccum[2], _clearAccum[3]);
            }
        #endif

        glClear( _clearMask );
    }

    #ifdef USE_SCISSOR_TEST
        glDisable( GL_SCISSOR_TEST );
    #endif

    #ifdef OSG_GL_MATRICES_AVAILABLE
        glMatrixMode( GL_MODELVIEW );
        glLoadIdentity();
    #endif

    // apply the positional state.
    if (_inheritedPositionalStateContainer.valid())
    {
        _inheritedPositionalStateContainer->draw(state, previous, &_inheritedPositionalStateContainerMatrix);
    }

    // apply the positional state.
    if (_renderStageLighting.valid())
    {
        _renderStageLighting->draw(state, previous, 0);
    }

    // draw the children and local.
    RenderBin::drawImplementation(renderInfo,previous);

    state.apply();

}

void RenderStage::drawPostRenderStages(osg::RenderInfo& renderInfo,RenderLeaf*& previous)
{
    if (_postRenderList.empty()) return;

    //cout << "Drawing prerendering stages "<<this<< "  "<<_viewport->x()<<","<< _viewport->y()<<","<< _viewport->width()<<","<< _viewport->height()<<std::endl;
    for(RenderStageList::iterator itr=_postRenderList.begin();
        itr!=_postRenderList.end();
        ++itr)
    {
        itr->second->draw(renderInfo,previous);
    }
    //cout << "Done Drawing prerendering stages "<<this<< "  "<<_viewport->x()<<","<< _viewport->y()<<","<< _viewport->width()<<","<< _viewport->height()<<std::endl;
}

// Statistics features
bool RenderStage::getStats(Statistics& stats) const
{
    bool statsCollected = false;

    for(RenderStageList::const_iterator pre_itr = _preRenderList.begin();
        pre_itr != _preRenderList.end();
        ++pre_itr)
    {
        if (pre_itr->second->getStats(stats))
        {
            statsCollected = true;
        }
    }

    for(RenderStageList::const_iterator post_itr = _postRenderList.begin();
        post_itr != _postRenderList.end();
        ++post_itr)
    {
        if (post_itr->second->getStats(stats))
        {
            statsCollected = true;
        }
    }

    if (RenderBin::getStats(stats))
    {
        statsCollected = true;
    }
    return statsCollected;
}

void RenderStage::attach(osg::Camera::BufferComponent buffer, osg::Image* image)
{
    _bufferAttachmentMap[buffer]._image = image;
}

unsigned int RenderStage::computeNumberOfDynamicRenderLeaves() const
{
    unsigned int count = 0;

    for(RenderStageList::const_iterator pre_itr = _preRenderList.begin();
        pre_itr != _preRenderList.end();
        ++pre_itr)
    {
        count += pre_itr->second->computeNumberOfDynamicRenderLeaves();
    }

    count += RenderBin::computeNumberOfDynamicRenderLeaves();

    for(RenderStageList::const_iterator post_itr = _postRenderList.begin();
        post_itr != _postRenderList.end();
        ++post_itr)
    {
        count += post_itr->second->computeNumberOfDynamicRenderLeaves();
    }

    return count;
}


void RenderStage::setMultisampleResolveFramebufferObject(osg::FrameBufferObject* fbo)
{
    if (fbo && fbo->isMultisample())
    {
        OSG_WARN << "Resolve framebuffer must not be"
            " multisampled." << std::endl;
    }
    _resolveFbo = fbo;
}

void RenderStage::collateReferencesToDependentCameras()
{
    _dependentCameras.clear();

    for(RenderStageList::iterator itr = _preRenderList.begin();
        itr != _preRenderList.end();
        ++itr)
    {
        itr->second->collateReferencesToDependentCameras();
        osg::Camera* camera = itr->second->getCamera();
        if (camera) _dependentCameras.push_back(camera);
    }

    for(RenderStageList::iterator itr = _postRenderList.begin();
        itr != _postRenderList.end();
        ++itr)
    {
        itr->second->collateReferencesToDependentCameras();
        osg::Camera* camera = itr->second->getCamera();
        if (camera) _dependentCameras.push_back(camera);
    }
}

void RenderStage::clearReferencesToDependentCameras()
{
    for(RenderStageList::iterator itr = _preRenderList.begin();
        itr != _preRenderList.end();
        ++itr)
    {
        itr->second->clearReferencesToDependentCameras();
    }

    for(RenderStageList::iterator itr = _postRenderList.begin();
        itr != _postRenderList.end();
        ++itr)
    {
        itr->second->clearReferencesToDependentCameras();
    }

    _dependentCameras.clear();
}

void RenderStage::releaseGLObjects(osg::State* state) const
{
    RenderBin::releaseGLObjects(state);

    for(RenderStageList::const_iterator itr = _preRenderList.begin();
        itr != _preRenderList.end();
        ++itr)
    {
        itr->second->releaseGLObjects(state);
    }

    for(RenderStageList::const_iterator itr = _postRenderList.begin();
        itr != _postRenderList.end();
        ++itr)
    {
        itr->second->releaseGLObjects(state);
    }

    for(Cameras::const_iterator itr = _dependentCameras.begin();
        itr != _dependentCameras.end();
        ++itr)
    {
        (*itr)->releaseGLObjects(state);
    }

    if (_texture.valid()) _texture->releaseGLObjects(state);

    if (_fbo.valid()) _fbo->releaseGLObjects(state);
    if (_resolveFbo.valid()) _resolveFbo->releaseGLObjects(state);
    if (_graphicsContext.valid())  _graphicsContext->releaseGLObjects(state);
}
