#ifndef IVE_DATAINPUTSTREAM
#define IVE_DATAINPUTSTREAM 1


#include <iostream>        // for ifstream
#include <string>
#include <map>
#include <vector>



#include <osg/Vec2>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/Vec2d>
#include <osg/Vec3d>
#include <osg/Vec4d>
#include <osg/Quat>
#include <osg/Array>
#include <osg/Matrix>
#include <osg/Image>
#include <osg/Geometry>
#include <osg/StateSet>
#include <osg/Uniform>
#include <osg/ref_ptr>

#include <osgTerrain/TerrainTile>
#include <osgVolume/VolumeTile>
//VRV Patch
#include <osgAnimation/StackedQuaternionElement>
#include <osgAnimation/StackedRotateAxisElement>
#include <osgAnimation/StackedScaleElement>
#include <osgAnimation/StackedTransformElement>
#include <osgAnimation/StackedTranslateElement>
#include <osgAnimation/StackedMatrixElement>
#include <osgAnimation/AnimationManagerBase>
//End VRV Patch
#include <osgDB/ReaderWriter>

#include "IveVersion.h"
#include "DataTypeSize.h"
#include "Exception.h"



namespace ive{

class DataInputStream{

public:
    DataInputStream(std::istream* istream, const osgDB::ReaderWriter::Options* options);
    ~DataInputStream();

    const osgDB::ReaderWriter::Options* getOptions() const { return _options.get(); }

    inline unsigned int getVersion() const { return _version; }
    bool readBool();
    char readChar();
    char peekChar();        //VRV Patch  //End VRV Patch
    unsigned char readUChar();
    unsigned short readUShort();
    short readShort();      //VRV Patch  //End VRV Patch
    unsigned int readUInt();
    int readInt();
    int peekInt();
    float readFloat();
    long readLong();
    unsigned long readULong();
    double readDouble();
    std::string readString();
    void readCharArray(char* data, int size);    
    osg::Vec2 readVec2();
    osg::Vec3 readVec3();
    osg::Vec4 readVec4();
    //VRV Patch
    osg::Vec2b readVec2b();
    osg::Vec3b readVec3b();
    osg::Vec4b readVec4b();
    osg::Vec2ub readVec2ub();
    osg::Vec3ub readVec3ub();
    osg::Vec4ub readVec4ub();
    osg::Vec2s readVec2s();
    osg::Vec3s readVec3s();
    osg::Vec4s readVec4s();
    osg::Vec2us readVec2us();
    osg::Vec3us readVec3us();
    osg::Vec4us readVec4us();
    osg::Vec2i readVec2i();
    osg::Vec3i readVec3i();
    osg::Vec4i readVec4i();
    osg::Vec2ui readVec2ui();
    osg::Vec3ui readVec3ui();
    osg::Vec4ui readVec4ui();
    osg::Vec2f readVec2f();
    osg::Vec3f readVec3f();
    osg::Vec4f readVec4f();
    //End VRV Patch
    osg::Vec2d readVec2d();
    osg::Vec3d readVec3d();
    osg::Vec4d readVec4d();
    osg::Plane readPlane();
    osg::Quat readQuat();
    osg::Matrixf readMatrixf();
    osg::Matrixd readMatrixd();
    osg::Array::Binding readBinding();
    osg::Array* readArray();
    osg::IntArray* readIntArray();
    osg::UByteArray* readUByteArray();
    osg::UShortArray* readUShortArray();
    osg::UIntArray* readUIntArray();
    osg::Vec4ubArray* readVec4ubArray();
    bool readPackedFloatArray(osg::FloatArray* floatArray);
    osg::FloatArray* readFloatArray();
    osg::Vec2Array* readVec2Array();
    osg::Vec3Array* readVec3Array();
    osg::Vec4Array* readVec4Array();
    osg::Vec2bArray* readVec2bArray();
    osg::Vec3bArray* readVec3bArray();
    osg::Vec4bArray* readVec4bArray();
    osg::Vec2sArray* readVec2sArray();
    osg::Vec3sArray* readVec3sArray();
    osg::Vec4sArray* readVec4sArray();
    osg::Vec2dArray* readVec2dArray();
    osg::Vec3dArray* readVec3dArray();
    osg::Vec4dArray* readVec4dArray();

    osg::Image* readImage(std::string s);
    osg::Image* readImage(IncludeImageMode mode);
    osg::Image* readImage();
    osg::StateSet* readStateSet();
    osg::StateAttribute* readStateAttribute();
    osg::Uniform* readUniform();
    osg::Shader* readShader();
    osg::Drawable* readDrawable();
    osg::Shape* readShape();
    osg::Node* readNode();

    osgTerrain::Layer* readLayer();
    osgTerrain::Locator* readLocator();

    osgVolume::Layer* readVolumeLayer();
    osgVolume::Locator* readVolumeLocator();
    osgVolume::Property* readVolumeProperty();

    osg::Object* readObject();

    // Set and get if must be generated external reference ive files
    void setLoadExternalReferenceFiles(bool b) {_loadExternalReferenceFiles=b;};
    bool getLoadExternalReferenceFiles() {return _loadExternalReferenceFiles;};


    typedef std::map<std::string, osg::ref_ptr<osg::Image> >    ImageMap;
    typedef std::map<int,osg::ref_ptr<osg::StateSet> >          StateSetMap;
    typedef std::map<int,osg::ref_ptr<osg::StateAttribute> >    StateAttributeMap;
    typedef std::map<int,osg::ref_ptr<osg::Uniform> >           UniformMap;
    typedef std::map<int,osg::ref_ptr<osg::Shader> >            ShaderMap;
    typedef std::map<int,osg::ref_ptr<osg::Drawable> >          DrawableMap;
    typedef std::map<int,osg::ref_ptr<osg::Shape> >             ShapeMap;
    typedef std::map<int,osg::ref_ptr<osg::Node> >              NodeMap;
    typedef std::map<int,osg::ref_ptr<osgTerrain::Layer> >      LayerMap;
    typedef std::map<int,osg::ref_ptr<osgTerrain::Locator> >    LocatorMap;
    typedef std::map<int,osg::ref_ptr<osgVolume::Layer> >       VolumeLayerMap;
    typedef std::map<int,osg::ref_ptr<osgVolume::Locator> >     VolumeLocatorMap;
    typedef std::map<int,osg::ref_ptr<osgVolume::Property> >    VolumePropertyMap;

    bool                _verboseOutput;
    std::istream*       _istream;
    int                 _byteswap;

    bool                _owns_istream;

    bool uncompress(std::istream& fin, std::string& destination) const;

    void throwException(const std::string& message) { _exception = new Exception(message); }
    void throwException(Exception* exception) { _exception = exception; }
    const Exception* getException() const { return _exception.get(); }

    //VRV Patch
    // bellow are functions taken from 
    // Serialization related functions osgDb::InputStream
    DataInputStream& operator>>(bool& b) { b=readBool(); return *this; }
    DataInputStream& operator>>(char& c) { c=readChar(); return *this; }
    DataInputStream& operator>>(unsigned char& c) { c=readUChar(); return *this; }
    DataInputStream& operator>>(short& s) { s=readShort(); return *this; }
    DataInputStream& operator>>(unsigned short& s) { s=readUShort(); return *this; }
    DataInputStream& operator>>(int& i) { i=readInt(); return *this; }
    DataInputStream& operator>>(unsigned int& i) { i=readUInt(); return *this; }
    DataInputStream& operator>>(long& l) { l=readLong(); return *this; }
    DataInputStream& operator>>(unsigned long& l) { l=readULong(); return *this; }
    DataInputStream& operator>>(float& f) { f=readFloat(); return *this; }
    DataInputStream& operator>>(double& d) { d=readDouble(); return *this; }
    DataInputStream& operator>>(std::string& s) { s=readString(); return *this; }
    DataInputStream& operator>>(osg::Vec2b& v) { v=readVec2b(); return *this; };
    DataInputStream& operator>>(osg::Vec3b& v) { v = readVec3b(); return *this; };
    DataInputStream& operator>>(osg::Vec4b& v) { v = readVec4b(); return *this; };
    DataInputStream& operator>>(osg::Vec2ub& v) { v = readVec2ub(); return *this; };
    DataInputStream& operator>>(osg::Vec3ub& v) { v = readVec3ub(); return *this; };
    DataInputStream& operator>>(osg::Vec4ub& v) { v = readVec4ub(); return *this; };
    DataInputStream& operator>>(osg::Vec2s& v) { v = readVec2s(); return *this; };
    DataInputStream& operator>>(osg::Vec3s& v) { v = readVec3s(); return *this; };
    DataInputStream& operator>>(osg::Vec4s& v) { v = readVec4s(); return *this; };
    DataInputStream& operator>>(osg::Vec2us& v) { v = readVec2us(); return *this; };
    DataInputStream& operator>>(osg::Vec3us& v) { v = readVec3us(); return *this; };
    DataInputStream& operator>>(osg::Vec4us& v) { v = readVec4us(); return *this; };
    DataInputStream& operator>>(osg::Vec2i& v) { v = readVec2i(); return *this; };
    DataInputStream& operator>>(osg::Vec3i& v) { v = readVec3i(); return *this; };
    DataInputStream& operator>>(osg::Vec4i& v) { v = readVec4i(); return *this; };
    DataInputStream& operator>>(osg::Vec2ui& v) { v = readVec2ui(); return *this; };
    DataInputStream& operator>>(osg::Vec3ui& v) { v = readVec3ui(); return *this; };
    DataInputStream& operator>>(osg::Vec4ui& v) { v = readVec4ui(); return *this; };
    DataInputStream& operator>>(osg::Vec2f& v) { v = readVec2f(); return *this; };
    DataInputStream& operator>>(osg::Vec3f& v) { v = readVec3f(); return *this; };
    DataInputStream& operator>>(osg::Vec4f& v) { v = readVec4f(); return *this; };
    DataInputStream& operator>>(osg::Vec2d& v) { v = readVec2d(); return *this; };
    DataInputStream& operator>>(osg::Vec3d& v) { v = readVec3d(); return *this; };
    DataInputStream& operator>>(osg::Vec4d& v) { v = readVec4d(); return *this; };
    DataInputStream& operator>>(osg::Quat& q)  { q = readQuat(); return *this; }
    DataInputStream& operator>>(osg::Plane& p) { p = readPlane(); return *this; }
    DataInputStream& operator>>(osg::Matrixf& mat) { mat = readMatrixf(); return *this; }
    DataInputStream& operator>>(osg::Matrixd& mat) { mat = readMatrixd(); return *this; }
    DataInputStream& operator>>(osg::BoundingBoxf& bb);
    DataInputStream& operator>>(osg::BoundingBoxd& bb);
    DataInputStream& operator>>(osg::BoundingSpheref& bs);
    DataInputStream& operator>>(osg::BoundingSphered& bs);

    template<typename T> DataInputStream& operator>>(osg::ref_ptr<T>& ptr)
    {
       ptr = static_cast<T*>(readObject()); return *this;
    }

    // readSize() use unsigned int for all sizes.
    unsigned int readSize() { unsigned int size; *this >> size; return size; }

    osg::Object* readStackedTransformElement();
    
    osgAnimation::AnimationManagerBase* animationManager() { return  pAnimationManager.get(); };

    // Keep a list of animation that are read from the file so we don't duplicate
    std::vector<osgAnimation::Animation*> animationList;

    // *****************
    // *****************
    //End VRV Patch

private:


    int                 _version;
    bool                _peeking;
    bool                _peekingChar;    //VRV Patch //End VRV Patch
    int                 _peekValue;
    char                _peekCharValue;  //VRV Patch //End VRV Patch
    ImageMap            _imageMap;
    StateSetMap         _statesetMap;
    StateAttributeMap   _stateAttributeMap;
    UniformMap          _uniformMap;
    ShaderMap           _shaderMap;
    DrawableMap         _drawableMap;
    ShapeMap            _shapeMap;
    NodeMap             _nodeMap;
    LayerMap            _layerMap;
    LocatorMap          _locatorMap;
    VolumeLayerMap      _volumeLayerMap;
    VolumeLocatorMap    _volumeLocatorMap;
    VolumePropertyMap   _volumePropertyMap;

    bool _loadExternalReferenceFiles;

    osg::ref_ptr<const osgDB::ReaderWriter::Options> _options;

    osg::ref_ptr<Exception> _exception;

    //! Animation manager 
    osg::ref_ptr<osgAnimation::AnimationManagerBase> pAnimationManager; //VRV Patch //End VRV Patch
};

}
#endif // IVE_DATAINPUTSTREAM
