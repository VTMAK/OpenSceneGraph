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
#include <osg/GL>
#include <osg/LineStipple>
#include <osg/Notify>

using namespace osg;


LineStipple::LineStipple()
{
  _factor = 1;
  _pattern = 0xffff;
}


LineStipple::~LineStipple()
{
}

void LineStipple::setFactor(GLint factor)
{
    _factor = factor;
}

void LineStipple::setPattern(GLushort pattern)
{
    _pattern = pattern;
}

void LineStipple::apply(State&) const
{
#ifdef OSG_GL1_AVAILABLE
    glLineStipple(_factor, _pattern);
#else
	// VRV_PATCH: start
	// re-enabled this to maintain backward compatibility because some of our
	// tactical graphics still use this LineStipple attribute and its used by vrvGraphicsUtils
	// as well.
	// We should switch both to use shader based stippling
	glLineStipple(_factor, _pattern);
	//OSG_NOTICE << "Warning: LineStipple::apply(State&) - not supported." << std::endl;
	// VRV_PATCH: end
#endif
}

