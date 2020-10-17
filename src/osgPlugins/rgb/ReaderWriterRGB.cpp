// Released under the OSGPL license, as part of the OpenSceneGraph distribution.
//
// ReaderWriter for sgi's .rgb format.
// specification can be found at http://local.wasp.uwa.edu.au/~pbourke/dataformats/sgirgb/sgiversion.html

#include <osg/Image>
#include <osg/Notify>

#include <osg/Geode>

#include <osg/GL>

#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/Registry>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SEEK_SET
#  define SEEK_SET 0
#endif

#if defined(OSG_GLES1_AVAILABLE) || defined(OSG_GLES2_AVAILABLE)
    #define GL_BITMAP               0x1A00
    #define GL_RED                  0x1903
    #define GL_GREEN                0x1904
    #define GL_BLUE                 0x1905
    #define GL_COLOR_INDEX          0x1900
#endif

#if defined(OSG_GL3_AVAILABLE)
    #define GL_BITMAP               0x1A00
    #define GL_COLOR_INDEX          0x1900
#endif

using namespace osg;

typedef unsigned int size_pos;

struct rawImageRec
{
    rawImageRec():
        imagic(0),
        type(0),
        dim(0),
        sizeX(0), sizeY(0), sizeZ(0),
        min(0), max(0),
        wasteBytes(0),
        colorMap(0),
        file(0),
        tmp(0), tmpR(0), tmpG(0), tmpB(0), tmpA(0),
        rleEnd(0),
        rowStart(0),
        rowSize(0),
        swapFlag(0),
        bpc(0)
    {
    }

    ~rawImageRec()
    {
        if (tmp) delete [] tmp;
        if (tmpR) delete [] tmpR;
        if (tmpG) delete [] tmpG;
        if (tmpB) delete [] tmpB;
        if (tmpA) delete [] tmpA;

        if (rowStart) delete [] rowStart;
        if (rowSize) delete [] rowSize;
    }

    unsigned short imagic;
    unsigned short type;
    unsigned short dim;
    unsigned short sizeX, sizeY, sizeZ;
    unsigned long min, max;
    unsigned long wasteBytes;
    char name[80];
    unsigned long colorMap;
    std::istream *file;
    unsigned char *tmp, *tmpR, *tmpG, *tmpB, *tmpA;
    unsigned long rleEnd;
    GLuint *rowStart;
    GLint *rowSize;
    GLenum swapFlag;
    short bpc;

    typedef unsigned char * BytePtr;

    bool needsBytesSwapped()
    {
        union {
            int testWord;
            char testByte[sizeof(int)];
        }endianTest;
        endianTest.testWord = 1;
        if( endianTest.testByte[0] == 1 )
            return true;
        else
            return false;
    }

    template <class T>
    inline void swapBytes(  T &s )
    {
        if( sizeof( T ) == 1 )
            return;

        T d = s;
        BytePtr sptr = (BytePtr)&s;
        BytePtr dptr = &(((BytePtr)&d)[sizeof(T)-1]);

        for( unsigned int i = 0; i < sizeof(T); i++ )
            *(sptr++) = *(dptr--);
    }

    void swapBytes()
    {
        swapBytes( imagic );
        swapBytes( type );
        swapBytes( dim );
        swapBytes( sizeX );
        swapBytes( sizeY );
        swapBytes( sizeZ );
        swapBytes( wasteBytes );
        swapBytes( min );
        swapBytes( max );
        swapBytes( colorMap );
    }
};

struct refImageRec : public rawImageRec, public osg::Referenced
{
};


static void ConvertShort(unsigned short *array, long length)
{
    unsigned long b1, b2;
    unsigned char *ptr;

    ptr = (unsigned char *)array;
    while (length--)
    {
        b1 = *ptr++;
        b2 = *ptr++;
        *array++ = (unsigned short) ((b1 << 8) | (b2));
    }
}

static void ConvertLong(GLuint *array, long length)
{
    unsigned long b1, b2, b3, b4;
    unsigned char *ptr;

    ptr = (unsigned char *)array;
    while (length--)
    {
        b1 = *ptr++;
        b2 = *ptr++;
        b3 = *ptr++;
        b4 = *ptr++;
        *array++ = (b1 << 24) | (b2 << 16) | (b3 << 8) | (b4);
    }
}

static osg::ref_ptr<refImageRec> RawImageOpen(std::istream& fin)
{
    union
    {
        int testWord;
        char testByte[4];
    } endianTest;

    int x;

    osg::ref_ptr<refImageRec> raw = new refImageRec;
    if (raw == NULL)
    {
        OSG_WARN<< "Out of memory!"<< std::endl;
        return NULL;
    }

    //Set istream pointer
    raw->file = &fin;

    endianTest.testWord = 1;
    if (endianTest.testByte[0] == 1)
    {
        raw->swapFlag = GL_TRUE;
    }
    else
    {
        raw->swapFlag = GL_FALSE;
    }

    fin.read((char*)&(raw->imagic),12);
    if (!fin.good())
    {
        return NULL;
    }

    if (raw->swapFlag)
    {
        ConvertShort(&raw->imagic, 6);
    }

    raw->tmp = raw->tmpR = raw->tmpG = raw->tmpB = raw->tmpA = 0L;
    raw->rowStart = 0;
    raw->rowSize = 0;
    raw->bpc = (raw->type & 0x00FF);

    raw->tmp = new unsigned char [static_cast<size_pos>(raw->sizeX) * 256 * static_cast<size_pos>(raw->bpc)];
    if (raw->tmp == NULL )
    {
        OSG_FATAL<< "Out of memory!"<< std::endl;
        return NULL;
    }

    if( raw->sizeZ >= 1 )
    {
        if( (raw->tmpR = new unsigned char [static_cast<size_pos>(raw->sizeX) * static_cast<size_pos>(raw->bpc)]) == NULL )
        {
            OSG_FATAL<< "Out of memory!"<< std::endl;
            return NULL;
        }
    }
    if( raw->sizeZ >= 2 )
    {
        if( (raw->tmpG = new unsigned char [static_cast<size_pos>(raw->sizeX) * static_cast<size_pos>(raw->bpc)]) == NULL )
        {
            OSG_FATAL<< "Out of memory!"<< std::endl;
            return NULL;
        }
    }
    if( raw->sizeZ >= 3 )
    {
        if( (raw->tmpB = new unsigned char [static_cast<size_pos>(raw->sizeX) * static_cast<size_pos>(raw->bpc)]) == NULL )
        {
            OSG_FATAL<< "Out of memory!"<< std::endl;
            return NULL;
        }
    }
    if (raw->sizeZ >= 4)
    {
        if( (raw->tmpA = new unsigned char [static_cast<size_pos>(raw->sizeX) * static_cast<size_pos>(raw->bpc)]) == NULL )
        {
            OSG_FATAL<< "Out of memory!"<< std::endl;
            return NULL;
        }
    }

    if ((raw->type & 0xFF00) == 0x0100)
    {
        unsigned int ybyz = static_cast<size_pos>(raw->sizeY) * static_cast<size_pos>(raw->sizeZ);
        if ( (raw->rowStart = new GLuint [ybyz]) == NULL )
        {
            OSG_FATAL<< "Out of memory!"<< std::endl;
            return NULL;
        }

        if ( (raw->rowSize = new GLint [ybyz]) == NULL )
        {
            OSG_FATAL<< "Out of memory!"<< std::endl;
            return NULL;
        }
        x = ybyz * sizeof(GLuint);
        raw->rleEnd = 512 + (2 * x);
        fin.seekg(512,std::ios::beg);
        fin.read((char*)raw->rowStart,x);
        fin.read((char*)raw->rowSize,x);
        if (raw->swapFlag)
        {
           ConvertLong(raw->rowStart, (long) (x/sizeof(GLuint)));
           ConvertLong((GLuint *)raw->rowSize, (long) (x/sizeof(GLint)));
        }
    }
    return raw;
}

static bool RawImageGetRow(rawImageRec *raw, const unsigned char *sourceBuf, unsigned int sourceBufSize, unsigned char *buf, int y, int z)
{
   unsigned char *iPtr, *oPtr;
   unsigned short pixel;
   int count, done = 0;
   unsigned short *tempShort;

   if ((raw->type & 0xFF00) == 0x0100)
   {
      unsigned int row_start = raw->rowStart[y + z*raw->sizeY];
      unsigned int row_size = raw->rowSize[y + z*raw->sizeY];      
      if ((row_start+row_size) <= sourceBufSize)
      {
         memcpy((char*)raw->tmp, (unsigned char *)sourceBuf + row_start, row_size);
      }
      else
      {
         OSG_WARN << "Trying to copy data outside memory bound" << std::endl;
         return false;
      }

      iPtr = raw->tmp;
      oPtr = buf;

      if (raw->bpc == 1)
      {
         while (!done)
         {
            pixel = *iPtr++;
            count = (int)(pixel & 0x7F);
            // limit the count value to the remaining row size
            if (raw->sizeX*raw->bpc <= (oPtr - buf))
            {
               count = raw->sizeX - (oPtr - buf) / raw->bpc;
            }
            if (count <= 0)
            {
               done = 1;
               return true;
            }
            if (pixel & 0x80)
            {
               while (count--)
               {
                 *oPtr++ = *iPtr++;
               }
            }
            else
            {
               pixel = *iPtr++;
               while (count--)
               {
                  *oPtr++ = pixel;
               }
            }
         }
      }
      else
      {
         while (!done)
         {
            tempShort = reinterpret_cast<unsigned short*>(iPtr);
            pixel = *tempShort;
            tempShort++;
            iPtr = reinterpret_cast<unsigned char *>(tempShort);
            ConvertShort(&pixel, 1);
            count = (int)(pixel & 0x7F);
            // limit the count value to the remaining row size
            if (raw->sizeX*raw->bpc <= (oPtr - buf))
            {
               count = raw->sizeX - (oPtr - buf) / raw->bpc;
            }

            if (count <= 0)
            {
               done = 1;
               return true;
            }
            if (pixel & 0x80)
            {
               while (count--)
               {
                  tempShort = reinterpret_cast<unsigned short*>(iPtr);
                  pixel = *tempShort;
                  tempShort++;
                  iPtr = reinterpret_cast<unsigned char *>(tempShort);

                  ConvertShort(&pixel, 1);

                  tempShort = reinterpret_cast<unsigned short*>(oPtr);
                  *tempShort = pixel;
                  tempShort++;
                  oPtr = reinterpret_cast<unsigned char *>(tempShort);
               }
            }
            else
            {
               tempShort = reinterpret_cast<unsigned short*>(iPtr);
               pixel = *tempShort;
               tempShort++;
               iPtr = reinterpret_cast<unsigned char *>(tempShort);
               ConvertShort(&pixel, 1);
               while (count--)
               {
                  tempShort = reinterpret_cast<unsigned short*>(oPtr);
                  *tempShort = pixel;
                  tempShort++;
                  oPtr = reinterpret_cast<unsigned char *>(tempShort);
               }
            }
         }
      }     
   }
   else
   {
      unsigned int offset = 512 + (y*raw->sizeX*raw->bpc) + (z*raw->sizeX*raw->sizeY*raw->bpc);
      if (offset <= sourceBufSize)
      {
         unsigned char *sourcePtrOffset = (unsigned char *)sourceBuf + offset;
         memcpy((char*)buf, sourcePtrOffset, raw->sizeX*raw->bpc);
         if (raw->swapFlag && raw->bpc != 1){
            ConvertShort(reinterpret_cast<unsigned short*>(buf), raw->sizeX);
         }
      }
      else
      {
         OSG_WARN << "Trying to copy data outside memory bound" << std::endl;
         return false;
      }
   }
   return true;
}

static bool RawImageGetData(rawImageRec *raw, unsigned char **data)
{
   unsigned char *ptr;
   int i, j;
   unsigned short *tempShort;

   //     // round the width to a factor 4
   //     int width = (int)(floorf((float)raw->sizeX/4.0f)*4.0f);
   //     if (width!=raw->sizeX) width += 4;

   // byte aligned.
   OSG_INFO << "raw->sizeX = " << raw->sizeX << std::endl;
   OSG_INFO << "raw->sizeY = " << raw->sizeY << std::endl;
   OSG_INFO << "raw->sizeZ = " << raw->sizeZ << std::endl;
   OSG_INFO << "raw->bpc = " << raw->bpc << std::endl;

   // Output data buffer
   long totalImageSize = (raw->sizeX)*(raw->sizeY)*(raw->sizeZ)*(raw->bpc);
   *data = new unsigned char[totalImageSize];
   if (data == NULL)
   {
      OSG_FATAL << "Out of memory!" << std::endl;      
      return false;
   }
   ptr = *data;

   // read the file at once instead of line by line  
   raw->file->seekg(0, raw->file->end);   
   long fileSize = raw->file->tellg();  // get the size of the file
   raw->file->seekg(0);
   unsigned char *theImageBuffer = new unsigned char[fileSize];
   if (theImageBuffer == NULL)
   {
      OSG_FATAL << "Out of memory!" << std::endl;
      return false;
   }
   raw->file->read((char*)theImageBuffer, fileSize);  // read the file in memory
   
   // copy data in temp buffer because we need to byte swap or do specific process on rgb data (RawImageGetRow)
   for (i = 0; i < (int)(raw->sizeY); i++)
   {
      // copy line one by one into the rgb temp buffer   
      if (raw->sizeZ >= 1)
      {
         if (!RawImageGetRow(raw, theImageBuffer, fileSize, raw->tmpR, i, 0))
            return false;
      }
      if (raw->sizeZ >= 2)
      {
         if (!RawImageGetRow(raw, theImageBuffer, fileSize, raw->tmpG, i, 1))
            return false;
      }
      if (raw->sizeZ >= 3)
      {
         if (!RawImageGetRow(raw, theImageBuffer, fileSize, raw->tmpB, i, 2))
            return false;
      }
      if (raw->sizeZ >= 4)
      {
         if (!RawImageGetRow(raw, theImageBuffer, fileSize, raw->tmpA, i, 3))
         {
            return false;
         }
      }

      if (raw->bpc == 1)
      {
         for (j = 0; j < (int)(raw->sizeX); j++)
         {            
            if (raw->sizeZ >= 1)
               *ptr++ = *(raw->tmpR + j);
            if (raw->sizeZ >= 2)
               *ptr++ = *(raw->tmpG + j);
            if (raw->sizeZ >= 3)
               *ptr++ = *(raw->tmpB + j);
            if (raw->sizeZ >= 4)
               *ptr++ = *(raw->tmpA + j);
         }
      }
      else
      {
         for (j = 0; j < (int)(raw->sizeX); j++)
         {
            if (raw->sizeZ >= 1)
            {
               tempShort = reinterpret_cast<unsigned short*>(ptr);
               *tempShort = *(reinterpret_cast<unsigned short*>(raw->tmpR) + j);
               tempShort++;
               ptr = reinterpret_cast<unsigned char *>(tempShort);
            }
            if (raw->sizeZ >= 2)
            {
               tempShort = reinterpret_cast<unsigned short*>(ptr);
               *tempShort = *(reinterpret_cast<unsigned short*>(raw->tmpG) + j);
               tempShort++;
               ptr = reinterpret_cast<unsigned char *>(tempShort);
            }
            if (raw->sizeZ >= 3)
            {
               tempShort = reinterpret_cast<unsigned short*>(ptr);
               *tempShort = *(reinterpret_cast<unsigned short*>(raw->tmpB) + j);
               tempShort++;
               ptr = reinterpret_cast<unsigned char *>(tempShort);
            }
            if (raw->sizeZ >= 4)
            {
               tempShort = reinterpret_cast<unsigned short*>(ptr);
               *tempShort = *(reinterpret_cast<unsigned short*>(raw->tmpA) + j);
               tempShort++;
               ptr = reinterpret_cast<unsigned char *>(tempShort);
            }            
         }
      }
   }
   //         // pad the image width with blanks to bring it up to the rounded width.
   //         for(;j<width;++j) *ptr++ = 0;
     
   if (theImageBuffer) 
      delete [] theImageBuffer;

   return true;
}

class ReaderWriterRGB : public osgDB::ReaderWriter
{
    public:

        ReaderWriterRGB()
        {
            supportsExtension("rgb","rgb image format");
            supportsExtension("rgba","rgba image format");
            supportsExtension("sgi","sgi image format");
            supportsExtension("int","int image format");
            supportsExtension("inta","inta image format");
            supportsExtension("bw","bw image format");
        }

        virtual const char* className() const { return "RGB Image Reader/Writer"; }

        ReadResult readRGBStream(std::istream& fin) const
        {
            osg::ref_ptr<refImageRec> raw;

            if( (raw = RawImageOpen(fin)) == NULL )
            {
                return ReadResult::ERROR_IN_READING_FILE;
            }

            if (raw->imagic != 474)
            {
               OSG_INFO << "RGB magic number=" << raw->imagic << " and should be 474. Error reading the file." << std::endl;
               return ReadResult::ERROR_IN_READING_FILE;
            }

            int s = raw->sizeX;
            int t = raw->sizeY;
            int r = 1;

            unsigned int pixelFormat =
                raw->sizeZ == 1 ? GL_LUMINANCE :
                raw->sizeZ == 2 ? GL_LUMINANCE_ALPHA :
                raw->sizeZ == 3 ? GL_RGB :
                raw->sizeZ == 4 ? GL_RGBA : (GLenum)-1;

            int internalFormat = pixelFormat;

            unsigned int dataType = raw->bpc == 1 ? GL_UNSIGNED_BYTE :
              GL_UNSIGNED_SHORT;

            unsigned char *data;
            if (!RawImageGetData(raw, &data))
            {
               OSG_INFO << "Unable to read RGB file correctly (possibly a corrupted file)" << std::endl;
               return ReadResult::ERROR_IN_READING_FILE;
            }
			// VRV_PATCH: start
			// PPP: Prevent double deletion
			// removed RawImageClose.
			// Found and fixed this while debugging.
			// Later found that this is not present in osg 3.6 either.
			// VRV_PATCH: end

            Image* image = new Image();
            image->setImage(s,t,r,
                internalFormat,
                pixelFormat,
                dataType,
                data,
                osg::Image::USE_NEW_DELETE);

            OSG_INFO << "image read ok "<<s<<"  "<<t<< std::endl;
            return image;
        }

        virtual ReadResult readObject(std::istream& fin,const osgDB::ReaderWriter::Options* options =NULL) const
        {
            return readImage(fin, options);
        }

        virtual ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options =NULL) const
        {
            return readImage(file, options);
        }

        virtual ReadResult readImage(std::istream& fin,const osgDB::ReaderWriter::Options* =NULL) const
        {
            return readRGBStream(fin);
        }

        virtual ReadResult readImage(const std::string& file, const osgDB::ReaderWriter::Options* options) const
        {
            std::string ext = osgDB::getLowerCaseFileExtension(file);
            if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

            std::string fileName = osgDB::findDataFile( file, options );
            if (fileName.empty()) return ReadResult::FILE_NOT_FOUND;

            osgDB::ifstream istream(fileName.c_str(), std::ios::in | std::ios::binary);
            if(!istream) return ReadResult::FILE_NOT_HANDLED;
            ReadResult rr = readRGBStream(istream);
            if(rr.validImage()) rr.getImage()->setFileName(file);

            return rr;
        }

        WriteResult writeRGBStream(const osg::Image& img, std::ostream &fout, const std::string& name) const
        {
            rawImageRec raw;
            raw.imagic = 0732;

            GLenum dataType = img.getDataType();

            raw.type  = dataType == GL_UNSIGNED_BYTE ? 1 :
                dataType == GL_BYTE ? 1 :
                dataType == GL_BITMAP ? 1 :
                dataType == GL_UNSIGNED_SHORT ? 2 :
                dataType == GL_SHORT ? 2 :
                dataType == GL_UNSIGNED_INT ? 4 :
                dataType == GL_INT ? 4 :
                dataType == GL_FLOAT ? 4 :
                dataType == GL_UNSIGNED_BYTE_3_3_2 ? 1 :
                dataType == GL_UNSIGNED_BYTE_2_3_3_REV ? 1 :
                dataType == GL_UNSIGNED_SHORT_5_6_5 ? 2 :
                dataType == GL_UNSIGNED_SHORT_5_6_5_REV ? 2 :
                dataType == GL_UNSIGNED_SHORT_4_4_4_4 ? 2 :
                dataType == GL_UNSIGNED_SHORT_4_4_4_4_REV ? 2 :
                dataType == GL_UNSIGNED_SHORT_5_5_5_1 ? 2 :
                dataType == GL_UNSIGNED_SHORT_1_5_5_5_REV ? 2 :
                dataType == GL_UNSIGNED_INT_8_8_8_8 ? 4 :
                dataType == GL_UNSIGNED_INT_8_8_8_8_REV ? 4 :
                dataType == GL_UNSIGNED_INT_10_10_10_2 ? 4 :
                dataType == GL_UNSIGNED_INT_2_10_10_10_REV ? 4 : 4;

            GLenum pixelFormat = img.getPixelFormat();

            raw.dim    = 3;
            raw.sizeX = img.s();
            raw.sizeY = img.t();
            raw.sizeZ =
                pixelFormat == GL_COLOR_INDEX? 1 :
                pixelFormat == GL_RED? 1 :
                pixelFormat == GL_GREEN? 1 :
                pixelFormat == GL_BLUE? 1 :
                pixelFormat == GL_ALPHA? 1 :
                pixelFormat == GL_RGB? 3 :
                pixelFormat == GL_BGR ? 3 :
                pixelFormat == GL_RGBA? 4 :
                pixelFormat == GL_BGRA? 4 :
                pixelFormat == GL_LUMINANCE? 1 :
                pixelFormat == GL_LUMINANCE_ALPHA ? 2 : 1;
            raw.min = 0;
            raw.max = 0xFF;
            raw.wasteBytes = 0;

            size_t name_size = sizeof(raw.name)-1;
            strncpy( raw.name, name.c_str(), name_size);
            raw.name[name_size] = 0;

            raw.colorMap = 0;
            raw.bpc = (img.getPixelSizeInBits()/raw.sizeZ)/8;

            int isize = img.getImageSizeInBytes();
            unsigned char *buffer = new unsigned char[isize];
            if(raw.bpc == 1)
            {
                unsigned char *dptr = buffer;
                int i, j;
                for( i = 0; i < raw.sizeZ; ++i )
                {
                    const unsigned char *ptr = img.data();
                    ptr += i;
                    for( j = 0; j < isize/raw.sizeZ; ++j )
                    {
                        *(dptr++) = *ptr;
                        ptr += raw.sizeZ;
                    }
                }
            }
            else
            { // bpc == 2
                unsigned short *dptr = reinterpret_cast<unsigned short*>(buffer);
                int i, j;
                for( i = 0; i < raw.sizeZ; ++i )
                {
                    const unsigned short *ptr = reinterpret_cast<const unsigned short*>(img.data());
                    ptr += i;
                    for( j = 0; j < isize/(raw.sizeZ*2); ++j )
                    {
                        *dptr = *ptr;
                        ConvertShort(dptr++, 1);
                        ptr += raw.sizeZ;
                    }
                }
            }


            if( raw.needsBytesSwapped() )
                raw.swapBytes();

            /*
            swapBytes( raw.imagic );
            swapBytes( raw.type );
            swapBytes( raw.dim );
            swapBytes( raw.sizeX );
            swapBytes( raw.sizeY );
            swapBytes( raw.sizeZ );
            swapBytes( raw.min );
            swapBytes( raw.max );
            swapBytes( raw.colorMap );
            */


            char pad[512 - sizeof(rawImageRec)];
            memset( pad, 0, sizeof(pad));

            fout.write((const char*)&raw,sizeof(rawImageRec));
            fout.write((const char*)pad,sizeof(pad));
            fout.write((const char*)buffer,isize);

            delete [] buffer;

            return WriteResult::FILE_SAVED;
        }

        virtual WriteResult writeImage(const osg::Image& img,std::ostream& fout,const osgDB::ReaderWriter::Options*) const
        {
            if (img.isCompressed())
            {
                OSG_NOTICE<<"Warning: RGB plugin does not supporting writing compressed imagery."<<std::endl;
                return WriteResult::ERROR_IN_WRITING_FILE;
            }
            if (!img.isDataContiguous())
            {
                OSG_NOTICE<<"Warning: RGB plugin does not supporting writing non contiguous imagery."<<std::endl;
                return WriteResult::ERROR_IN_WRITING_FILE;
            }

            return writeRGBStream(img,fout,"");
        }

        virtual WriteResult writeImage(const osg::Image &img,const std::string& fileName, const osgDB::ReaderWriter::Options*) const
        {
            std::string ext = osgDB::getFileExtension(fileName);
            if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

            if (img.isCompressed())
            {
                OSG_NOTICE<<"Warning: RGB plugin does not supporting writing compressed imagery."<<std::endl;
                return WriteResult::ERROR_IN_WRITING_FILE;
            }
            if (!img.isDataContiguous())
            {
                OSG_NOTICE<<"Warning: RGB plugin does not supporting writing non contiguous imagery."<<std::endl;
                return WriteResult::ERROR_IN_WRITING_FILE;
            }

            osgDB::ofstream fout(fileName.c_str(), std::ios::out | std::ios::binary);
            if(!fout) return WriteResult::ERROR_IN_WRITING_FILE;

            return writeRGBStream(img,fout,fileName);
        }

};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(rgb, ReaderWriterRGB)
