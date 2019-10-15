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
#include <osg/Viewport>
#include <osg/State>
#include <osg/GLExtensions>

using namespace osg;
//VRV PATCH added support for multiple viewports
Viewport::Viewport()
{
   _numViewports = 1;
   _x.push_back(0);
   _y.push_back(0);
   _width.push_back(800);
   _height.push_back(600);
}


Viewport::~Viewport()
{
}

void Viewport::apply(State& state) const
{
   const GLExtensions* extensions = state.get<GLExtensions>();

   if (!extensions->glViewportIndexedf)
   {
      glViewport(static_cast<GLint>(_x[0]), static_cast<GLint>(_y[0]),
         static_cast<GLsizei>(_width[0]), static_cast<GLsizei>(_height[0]));
   }
   else //   if (_numViewports > 1)
   {
      
      for (int i = 0; i < _numViewports; i++)
      {
         extensions->glViewportIndexedf(i, _x[i], _y[i], _width[i], _height[i]);
      }
   }

}

