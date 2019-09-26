/* -*-c++-*- OpenSceneGraph - Copyright (C) 1998-2009 Robert Osfield
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

/**
 * \brief    Shader generator framework.
 * \author   Maciej Krol
 */

#include <osgUtil/ShaderGen>
#include <osg/Geode>
#include <osg/Geometry> // for ShaderGenVisitor::update
#include <osg/Fog>
#include <sstream>

#include "shaders/shadergen_vert.cpp"
#include "shaders/shadergen_frag.cpp"

using namespace osgUtil;

namespace osgUtil
{

ShaderGenVisitor::ShaderGenVisitor():
    osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
{
}

void ShaderGenVisitor::assignUberProgram(osg::StateSet *stateSet)
{
    if (stateSet)
    {
        osg::ref_ptr<osg::Program> uberProgram = new osg::Program;
        uberProgram->addShader(new osg::Shader(osg::Shader::VERTEX, shadergen_vert));
        uberProgram->addShader(new osg::Shader(osg::Shader::FRAGMENT, shadergen_frag));

        stateSet->setAttribute(uberProgram.get());
        stateSet->addUniform(new osg::Uniform("diffuseMap", 0));

        remapStateSet(stateSet);
    }
}

void ShaderGenVisitor::apply(osg::Node &node)
{
    osg::StateSet* stateSet = node.getStateSet();
    if (stateSet) remapStateSet(stateSet);

    traverse(node);
}


void ShaderGenVisitor::remapStateSet(osg::StateSet* stateSet)
{
    if (!stateSet) return;

#pragma error("REVIEW: Should the following allocation be removed in favor of the stateSet coming in")
   osg::StateSet *stateSet = new osg::StateSet;
   osg::Program *program = new osg::Program;
   stateSet->setAttribute(program);

   std::ostringstream vert;
   std::ostringstream frag;
   vert <<
      "#version 150 compatibility \n"
      "// Surface material:                                         \n"
      "   struct osg_MaterialParameters                             \n"
      "   {                                                         \n"
      "      vec4 ambient;     // Acm                               \n"
      "      vec4 diffuse;     // Dcm                               \n"
      "      vec4 emission;    // Ecm                               \n"
      "      vec4 specular;    // Scm                               \n"
      "      float shininess;  // Srm                               \n"
      "   };                                                        \n"
      "                                                             \n"
      "   layout(std140) uniform osg_material_uniform_block         \n"
      "   {                                                         \n"
      "      uniform osg_MaterialParameters osg_FrontMaterial;      \n"
      "   };                                                        \n"
      "                                                             \n";

    // write varyings
    if ((stateMask & LIGHTING) && !(stateMask & NORMAL_MAP))
    {
        vert << "varying vec3 normalDir;\n";
    }

    if (stateMask & (LIGHTING | NORMAL_MAP))
    {
        vert << "varying vec3 lightDir;\n";
    }

    if (stateMask & (LIGHTING | NORMAL_MAP | FOG))
    {
        vert << "varying vec3 viewDir;\n";
    }

    // copy varying to fragment shader
    frag << vert.str();

    // write uniforms and attributes
    int unit = 0;
    if (stateMask & DIFFUSE_MAP)
    {
        osg::Uniform *diffuseMap = new osg::Uniform("diffuseMap", unit++);
        stateSet->addUniform(diffuseMap);
        frag << "uniform sampler2D diffuseMap;\n";
    }

    if (stateMask & NORMAL_MAP)
    {
        osg::Uniform *normalMap = new osg::Uniform("normalMap", unit++);
        stateSet->addUniform(normalMap);
        frag << "uniform sampler2D normalMap;\n";
        program->addBindAttribLocation("tangent", 6);
        vert << "attribute vec3 tangent;\n";
    }

    vert << "\n"\
        "void main()\n"\
        "{\n"\
        "  gl_Position = ftransform();\n";

    if (stateMask & (DIFFUSE_MAP | NORMAL_MAP))
    {
        vert << "  gl_TexCoord[0] = gl_MultiTexCoord0;\n";
    }

    // remove any modes that won't be appropriate when using shaders, and remap them to the apppropriate Uniform/Define combination


    osg::StateSet::ModeList& modes = stateSet->getModeList();

    if (modes.count(GL_LIGHTING)>0)
    {
        osg::StateAttribute::GLModeValue lightingMode =modes[GL_LIGHTING];

        stateSet->removeMode(GL_LIGHTING);
        stateSet->removeMode(GL_LIGHT0);

        stateSet->setDefine("GL_LIGHTING", lightingMode);
        if (stateMask & (LIGHTING | NORMAL_MAP))
        {
            frag <<
                "  vec3 nd = normalize(normalDir);\n"\
                "  vec3 ld = normalize(lightDir);\n"\
                "  vec3 vd = normalize(viewDir);\n"\
                "  vec4 color = gl_FrontLightModelProduct.sceneColor;\n"\
                "  color += gl_FrontLightProduct[0].ambient;\n"\
            "  color += vec4(.1,.1,.1,0);\n"\
            "  float diff = max(dot(ld, nd), 0.0);\n"\
                "  color += gl_FrontLightProduct[0].diffuse * diff;\n"\
                "  color *= base;\n"\
                "  if (diff > 0.0)\n"\
                "  {\n"\
                "    vec3 halfDir = normalize(ld+vd);\n"\
                "    color.rgb += base.a * gl_FrontLightProduct[0].specular.rgb * \n"\
                "      pow(max(dot(halfDir, nd), 0.0), osg_FrontMaterial.shininess);\n"\
                "  }\n";
        }
        else
        {
            frag << "  vec4 color = base;\n";
        }
    }


    if (modes.count(GL_FOG)>0)
    {
        osg::StateAttribute::GLModeValue fogMode = modes[GL_FOG];
        stateSet->removeMode(GL_FOG);
        stateSet->setDefine("GL_FOG", fogMode);
    }


    if (!stateSet->getTextureModeList().empty())
    {
        osg::StateSet::ModeList& textureModes = stateSet->getTextureModeList()[0];

        if (textureModes.count(GL_TEXTURE_2D)>0)
        {
            osg::StateAttribute::GLModeValue textureMode = textureModes[GL_TEXTURE_2D];
            stateSet->removeTextureMode(0, GL_TEXTURE_2D);
            stateSet->setDefine("GL_TEXTURE_2D", textureMode);
        }
    }
}

} // namespace osgUtil