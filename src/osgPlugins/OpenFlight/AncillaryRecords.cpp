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

//
// OpenFlight® loader for OpenSceneGraph
//
//  Copyright (C) 2005-2007  Brede Johansen
//

#include <cstdio>
#include <osg/MatrixTransform>
#include <osg/Texture2D>
#include <osg/TexEnv>

#include "Registry.h"
#include "Document.h"
#include "RecordInputStream.h"

namespace flt {

/** Comment -
  */
class Comment : public Record
{
    public:

        Comment() {}

        META_Record(Comment)

    protected:

        virtual ~Comment() {}

        virtual void readRecord(RecordInputStream& in, Document& /*document*/)
        {
            std::streamsize size = in.getRecordSize();
            std::string commentfield = in.readString(size-4);

            if (_parent.valid())
            {
#if 0            
                _parent->setComment(commentfield);
#else
                unsigned int front_of_line = 0;
                unsigned int end_of_line = 0;
                while (end_of_line<commentfield.size())
                {
                    if (commentfield[end_of_line]=='\r')
                    {
                        _parent->setComment( std::string( commentfield, front_of_line, end_of_line-front_of_line) );

                        if (end_of_line+1<commentfield.size() &&
                            commentfield[end_of_line+1]=='\n') ++end_of_line;

                        ++end_of_line;
                        front_of_line = end_of_line;
                    }
                    else if (commentfield[end_of_line]=='\n')
                    {
                        _parent->setComment( std::string( commentfield, front_of_line, end_of_line-front_of_line) );
                        ++end_of_line;
                        front_of_line = end_of_line;
                    }
                    else ++end_of_line;
                }
                if (front_of_line<end_of_line)
                {
                    _parent->setComment( std::string( commentfield, front_of_line, end_of_line-front_of_line) );
                }

            }
#endif

        }
};

REGISTER_FLTRECORD(Comment, COMMENT_OP)



/** LongID -
  */
class LongID : public Record
{
    public:

        LongID() {}

        META_Record(LongID)

    protected:

        virtual ~LongID() {}

        virtual void readRecord(RecordInputStream& in, Document& /*document*/)
        {
            std::streamsize size = in.getRecordSize();
            std::string id = in.readString(size-4);

            if (_parent.valid())
                _parent->setID(id);
        }
};

REGISTER_FLTRECORD(LongID, LONG_ID_OP)



/** Matrix -
  */
class Matrix : public Record
{
    public:

        Matrix() {}

        META_Record(Matrix)

    protected:

        virtual ~Matrix() {}

        virtual void readRecord(RecordInputStream& in, Document& document)
        {
            osg::Matrix matrix;
            for (int i=0; i<4; ++i)
            {
                for (int j=0; j<4; ++j)
                {
                    matrix(i,j) = in.readFloat32();
                }
            }

            // scale position.
            osg::Vec3 pos = matrix.getTrans();
            matrix *= osg::Matrix::translate(-pos);
            pos *= (float)document.unitScale();
            matrix *= osg::Matrix::translate(pos);

            if (_parent.valid())
                _parent->setMatrix(matrix);
        }
};

REGISTER_FLTRECORD(Matrix, MATRIX_OP)



/** Multitexture -
  */
class Multitexture : public Record
{
    public:

        Multitexture() {}

        META_Record(Multitexture)

        // Effect
        enum EffectMode
        {
            TEXTURE_ENVIRONMENT = 0,
            BUMP_MAP = 1
        };

    protected:

        virtual ~Multitexture() {}

        virtual void readRecord(RecordInputStream& in, Document& document)
        {
           if (document.getIgnoreNonBaseTextures())
           {
              //ignore any other texture than the base texture in the face attributes
              return;
           }

            //7 * (4 * ( 2 )) + zero
            char stateSetKey[57];
            memset(stateSetKey,0,sizeof(char[57]));
            bool textureMask[7] = {0,0,0,0,0,0,0};
            int16 textureIndices[7] = {0,0,0,0,0,0,0};
            int16 textureEffects[7] = {0,0,0,0,0,0,0};
            
            int stateSetKeyIndex = 0;
            uint32 mask = in.readUInt32();
            for (int layer=1; layer<8; layer++)
            {
                uint32 layerBit = 0x80000000u >> (layer-1);
                if (mask & layerBit)
                {
                   textureMask[layer - 1] = true;
                    textureIndices[layer - 1] = in.readInt16();
                    textureEffects[layer - 1] = in.readInt16();
                    /*int16 mappingIndex =*/ in.readInt16();
                    /*uint16 data =*/ in.readUInt16();
                    sprintf(stateSetKey + stateSetKeyIndex,"%04x%04x",textureIndices[layer - 1],textureEffects[layer - 1]);
                    stateSetKeyIndex += 4;
                }                
            }

            if (_parent.valid())
            {
               osg::ref_ptr<osg::StateSet> multiTextureStateSet = document.getOrCreateMultiTexturePool()->get(*((MultiTextureKey*)stateSetKey));
               //Check if effect texture are inserted to the stateset, if so, we want them tightly packed 
               //with the texture found in this record
               ExtendedMaterialPool* extendedMaterial = document.getExtendedMaterialPool();
               int firstUnitAvaiableAfterEffectTextures = -1;
               // VRV_PATCH BEGIN
               int extendedMaterialSize = 0;
               // VRV_PATCH END
               if (extendedMaterial)
               {
                  extendedMaterialSize = extendedMaterial->size();
                  firstUnitAvaiableAfterEffectTextures = extendedMaterial->firstTextureUnitAfterEffectTexture();
               }
               if(!multiTextureStateSet.valid())
               {
                  //Create a new multi texture stateset, and populate it.
                  multiTextureStateSet = document.getOrCreateMultiTexturePool()->create(*((MultiTextureKey*)stateSetKey));
                  int firstSlot = 1;
                  // VRV_PATCH BEGIN
                  bool setTexture = false;
                  // VRV_PATCH END
                  if (firstUnitAvaiableAfterEffectTextures != -1)
                  {
                     firstSlot = firstUnitAvaiableAfterEffectTextures;
                  }
                  if(multiTextureStateSet.valid())
                  {
                     for (int layer=1; layer<8; layer++)
                     {
                         // VRV_PATCH BEGIN
                        setTexture = false;
                        // VRV_PATCH END
                        if(!textureMask[layer-1]) continue;
                        osg::ref_ptr<osg::StateSet> texturePoolStateset = document.getOrCreateTexturePool()->get(textureIndices[layer - 1]);
                        if (texturePoolStateset.valid())
                        {
                            // Apply texture from texture pool.
                            osg::Texture* texture = dynamic_cast<osg::Texture*>(texturePoolStateset->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
                            if (texture)
                            {
                                // VRV_PATCH BEGIN
                                if (extendedMaterialSize)
                                {
                                    multiTextureStateSet->setTextureAttributeAndModes(firstSlot, texture, osg::StateAttribute::ON);
                                }
                                else
                                {
                                    multiTextureStateSet->setTextureAttributeAndModes(layer, texture, osg::StateAttribute::ON);
                                }
                                
                                setTexture = true;
                                // VRV_PATCH END
                            }

                            // Apply texture environment
                            switch (textureEffects[layer - 1])
                            {
                                case TEXTURE_ENVIRONMENT:
                                {
                                    // Use texture environment setting from .attr file.
                                    osg::TexEnv* texenv = dynamic_cast<osg::TexEnv*>(texturePoolStateset->getTextureAttribute(0, osg::StateAttribute::TEXENV));
                                    if (texenv)
                                    {
                                        // VRV_PATCH BEGIN
                                        if (extendedMaterialSize)
                                        {
                                            multiTextureStateSet->setTextureAttribute(firstSlot, texenv);
                                        }
                                        else
                                        {
                                            multiTextureStateSet->setTextureAttribute(layer, texenv);
                                        }
                                        // VRV_PATCH END
                                    }
                                }
                                break;
                                case BUMP_MAP:
                                {
                                    // Dot3 bumpmap
                                    //osg::TexEnvCombine* texEnvCombine = new osg::TexEnvCombine;
                                    //texEnvCombine->setCombine_RGB(osg::TexEnvCombine::DOT3_RGB);
                                    //texEnvCombine->setSource0_RGB(osg::TexEnvCombine::PRIMARY_COLOR);
                                    //texEnvCombine->setSource1_RGB(osg::TexEnvCombine::TEXTURE);
                                    //multiTextureStateSet->setTextureAttribute(layer,texEnvCombine);
                                }
                                break;
                            }
                        }

                        // VRV_PATCH BEGIN
                        // Can have slot: 0,1,x,x,x,x,x,7
                        // Extended would make it 0,1,2
                        if (setTexture)
                        {
                            ++firstSlot;
                        }
                        // VRV_PATCH END
                     }
                  }
               }
               _parent->setMultitexture(*multiTextureStateSet);
            }
        }
};

REGISTER_FLTRECORD(Multitexture, MULTITEXTURE_OP)



/** UVList - Texture coordinates used with multitexture.
    UVList is an ancillary to VertexList.
  */
class UVList : public Record
{
    public:

        UVList() {}

        META_Record(UVList)

    protected:

        virtual ~UVList() {}

        // count number of 1's in mask.
        int bitCount(uint32 mask)
        {
            int count = 0;
            while (mask)
            { 
                if (mask & 0x0001)
                    ++count; 
                mask >>= 1; 
            }
            return count;
        }

        virtual void readRecord(RecordInputStream& in, Document& /*document*/)
        {
            uint32 mask = in.readUInt32(0);

            int numLayers = bitCount(mask);
            int numVertices = (in.getRecordSize()-8) / (8 * numLayers);
            for (int n=0; n < numVertices; ++n)
            {
                for (unsigned int layer=1; layer<8; layer++)
                {
                    uint32 layerBit = 0x80000000u >> (layer-1);
                    if (mask & layerBit)
                    {
                        float32    u = in.readFloat32();
                        float32    v = in.readFloat32();

                        // Add texture coordinates to geometry.
                        if (_parent.valid())
                            _parent->addVertexUV(layer,osg::Vec2(u,v));
                    }
                }
            }
        }
};

REGISTER_FLTRECORD(UVList, UV_LIST_OP)


/** Replicate -
  */
class Replicate : public Record
{
    public:

        Replicate() {}

        META_Record(Replicate)

    protected:

        virtual ~Replicate() {}

        virtual void readRecord(RecordInputStream& in, Document& /*document*/)
        {
            int16 replicate = in.readInt16();

            if (_parent.valid())
                _parent->setNumberOfReplications((int)replicate);
        }
};

REGISTER_FLTRECORD(Replicate, REPLICATE_OP)


/** IndexedString -
  */
class IndexedString : public Record
{
    public:

        IndexedString() {}

        META_Record(IndexedString)

    protected:

        virtual ~IndexedString() {}

        virtual void readRecord(RecordInputStream& in, Document& /*document*/)
        {
            std::streamsize size = in.getRecordSize();
            uint32 index = in.readUInt32();
            std::string name = in.readString(size-8);

            if (_parent.valid())
                _parent->setMultiSwitchValueName(index, name);
        }
};

REGISTER_FLTRECORD(IndexedString, INDEXED_STRING_OP)


// Prevent "unknown record" message for the following ancillary records:
REGISTER_FLTRECORD(DummyRecord, OLD_TRANSLATE2_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_ROTATE_ABOUT_POINT_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_ROTATE_ABOUT_EDGE_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_SCALE_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_TRANSLATE_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_NONUNIFORM_SCALE_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_ROTATE_ABOUT_POINT2_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_ROTATE_SCALE_TO_POINT_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_PUT_TRANSFORM_OP)
REGISTER_FLTRECORD(DummyRecord, OLD_BOUNDING_BOX_OP)
REGISTER_FLTRECORD(DummyRecord, ROAD_ZONE_OP)
REGISTER_FLTRECORD(DummyRecord, ROTATE_ABOUT_EDGE_OP)
REGISTER_FLTRECORD(DummyRecord, TRANSLATE_OP)
REGISTER_FLTRECORD(DummyRecord, NONUNIFORM_SCALE_OP)
REGISTER_FLTRECORD(DummyRecord, ROTATE_ABOUT_POINT_OP)
REGISTER_FLTRECORD(DummyRecord, ROTATE_SCALE_TO_POINT_OP)
REGISTER_FLTRECORD(DummyRecord, PUT_TRANSFORM_OP)
REGISTER_FLTRECORD(DummyRecord, GENERAL_MATRIX_OP)
REGISTER_FLTRECORD(DummyRecord, VECTOR_OP)
REGISTER_FLTRECORD(DummyRecord, BOUNDING_BOX_OP)
REGISTER_FLTRECORD(DummyRecord, BOUNDING_SPHERE_OP)
REGISTER_FLTRECORD(DummyRecord, BOUNDING_CYLINDER_OP)
REGISTER_FLTRECORD(DummyRecord, BOUNDING_CONVEX_HULL_OP)
REGISTER_FLTRECORD(DummyRecord, BOUNDING_HISTOGRAM)
REGISTER_FLTRECORD(DummyRecord, BOUNDING_VOLUME_CENTER_OP)
REGISTER_FLTRECORD(DummyRecord, BOUNDING_VOLUME_ORIENTATION_OP)
REGISTER_FLTRECORD(DummyRecord, HISTOGRAM_BOUNDING_VOLUME_OP)


} // end namespace

