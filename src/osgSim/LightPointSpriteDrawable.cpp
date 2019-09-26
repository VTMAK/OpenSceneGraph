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

#include "LightPointSpriteDrawable.h"

#include <osg/Point>

using namespace osgSim;

LightPointSpriteDrawable::LightPointSpriteDrawable():
    osgSim::LightPointDrawable()
{
    _sprite = new osg::PointSprite;
}

LightPointSpriteDrawable::LightPointSpriteDrawable(const LightPointSpriteDrawable& lpsd,const osg::CopyOp& copyop):
    osgSim::LightPointDrawable(lpsd,copyop)
{
}

void LightPointSpriteDrawable::drawImplementation(osg::RenderInfo& renderInfo) const
{
#if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE) && !defined(OSG_GL3_AVAILABLE)
    osg::State& state = *renderInfo.getState();

    if (!state.getModeValidity(GL_POINT_SPRITE_ARB))
    {
        LightPointDrawable::drawImplementation(renderInfo);
        return;
    }

    state.applyMode(GL_POINT_SMOOTH,true);
    state.applyMode(GL_BLEND,true);
    state.applyMode(GL_LIGHTING,false);
    state.applyTextureMode(0,GL_TEXTURE_2D,true);

    state.applyMode(GL_POINT_SPRITE_ARB,true);
    state.applyTextureAttribute(0,_sprite.get());
    // Assume the point sprite texture map is already specified
    // (typically in the owning LightPointNode StateSet).


    glHint(GL_POINT_SMOOTH_HINT,GL_NICEST);

    //VRV PATCH - For reverse depth buffer support
    // osg::Depth defaults to GL_LESS, but Vantage
    // in reverse depth uses GL_GREATER so query state
    bool changeDepthFunc = _depthFunction == -1;
    if (changeDepthFunc)
    {
       glGetIntegerv(GL_DEPTH_FUNC, &(_depthFunction));
       _depthOn->setFunction((osg::Depth::Function)_depthFunction);
    }
    //END VRV PATCH

    state.applyAttribute(_depthOn.get());

    state.applyAttribute(_blendOneMinusSrcAlpha.get());
    state.applyMode(GL_POINT_SMOOTH,true);

    SizedLightPointList::const_iterator sitr;
    unsigned int pointsize;
    for(pointsize=1,sitr=_sizedOpaqueLightPointList.begin();
        sitr!=_sizedOpaqueLightPointList.end();
        ++sitr,++pointsize)
    {

        const LightPointList& lpl = *sitr;
        if (!lpl.empty())
        {
            glPointSize(pointsize);
            //glInterleavedArrays(GL_C4UB_V3F,0,&lpl.front());
#if defined(OSG_GL_FIXED_FUNCTION_AVAILABLE)
            state.setInterleavedArrays(GL_C4UB_V3F,0,&lpl.front());
#else
            state.disableAllVertexArrays();
            state.setVertexAttribPointer(state.getColorAlias()._location, 4, GL_FLOAT, false, sizeof(lpl[0]), &lpl[0].first);
            state.setVertexAttribPointer(state.getVertexAlias()._location, 3, GL_FLOAT, false, sizeof(lpl[0]), &lpl[0].second);
            state.dirtyAllVertexArrays();
#endif
            glDrawArrays(GL_POINTS,0,lpl.size());
        }
    }

    state.applyMode(GL_BLEND,true);

    //VRV PATCH - For reverse depth buffer support
    if (changeDepthFunc)
    {
       _depthOff->setFunction((osg::Depth::Function)_depthFunction);
    }
    //END VRV PATCH

    state.applyAttribute(_depthOff.get());


    state.applyAttribute(_blendOneMinusSrcAlpha.get());

    for(pointsize=1,sitr=_sizedBlendedLightPointList.begin();
        sitr!=_sizedBlendedLightPointList.end();
        ++sitr,++pointsize)
    {

        const LightPointList& lpl = *sitr;
        if (!lpl.empty())
        {
            glPointSize(pointsize);
            //glInterleavedArrays(GL_C4UB_V3F,0,&lpl.front());
#if defined(OSG_GL_FIXED_FUNCTION_AVAILABLE)
            state.setInterleavedArrays(GL_C4UB_V3F,0,&lpl.front());
#else
            state.disableAllVertexArrays();
            state.setVertexAttribPointer(state.getColorAlias()._location, 4, GL_FLOAT, false, sizeof(lpl[0]), &lpl[0].first);
            state.setVertexAttribPointer(state.getVertexAlias()._location, 3, GL_FLOAT, false, sizeof(lpl[0]), &lpl[0].second);
            state.dirtyAllVertexArrays();
#endif
            glDrawArrays(GL_POINTS,0,lpl.size());
        }
    }


    state.applyAttribute(_blendOne.get());

    for(pointsize=1,sitr=_sizedAdditiveLightPointList.begin();
        sitr!=_sizedAdditiveLightPointList.end();
        ++sitr,++pointsize)
    {

        const LightPointList& lpl = *sitr;
        if (!lpl.empty())
        {
            //state.applyMode(GL_POINT_SMOOTH,pointsize!=1);
            glPointSize(pointsize);
            //glInterleavedArrays(GL_C4UB_V3F,0,&lpl.front());
#if defined(OSG_GL_FIXED_FUNCTION_AVAILABLE)
            state.setInterleavedArrays(GL_C4UB_V3F,0,&lpl.front());
#else
            state.disableAllVertexArrays();
            state.setVertexAttribPointer(state.getColorAlias()._location, 4, GL_FLOAT, false, sizeof(lpl[0]), &lpl[0].first);
            state.setVertexAttribPointer(state.getVertexAlias()._location, 3, GL_FLOAT, false, sizeof(lpl[0]), &lpl[0].second);
            state.dirtyAllVertexArrays();
#endif
            glDrawArrays(GL_POINTS,0,lpl.size());
        }
    }

    glPointSize(1);

    glHint(GL_POINT_SMOOTH_HINT,GL_FASTEST);

    state.haveAppliedAttribute(osg::StateAttribute::POINT);

    state.dirtyAllVertexArrays();
    state.disableAllVertexArrays();

    // restore the state afterwards.
    state.apply();
#else
    OSG_NOTICE<<"Warning: LightPointDrawable not supported."<<std::endl;
#endif
}


