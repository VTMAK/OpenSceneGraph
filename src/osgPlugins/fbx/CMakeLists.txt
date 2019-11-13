INCLUDE_DIRECTORIES(${FBX_INCLUDE_DIR})

SET(TARGET_SRC
    fbxRAnimation.cpp
    fbxRCamera.cpp
    fbxRLight.cpp
    fbxRMesh.cpp
    fbxRNode.cpp
    ReaderWriterFBX.cpp
    WriterCompareTriangle.cpp
    WriterNodeVisitor.cpp
    fbxMaterialToOsgStateSet.cpp
    fbxTextureUnitMap.cpp
    fbxUtil.cpp
    fbxMatrixTransformVisitor.cpp
)

SET(TARGET_H
    fbxMaterialToOsgStateSet.h
    fbxReader.h
    ReaderWriterFBX.h
    WriterCompareTriangle.h
    WriterNodeVisitor.h
    fbxTextureUnitMap.h
    fbxUtil.h
    fbxMatrixTransformVisitor.h
)

INCLUDE_DIRECTORIES(${BOOST_INCLUDE_DIR})

link_directories(${BOOST_LIB_DIR})

IF(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    REMOVE_CXX_FLAG(-pedantic)
    REMOVE_CXX_FLAG(-Wall)
    REMOVE_CXX_FLAG(-Wunused)
ENDIF()

IF(WIN32)
    SET(TARGET_EXTERNAL_LIBRARIES wininet)
ENDIF(WIN32)

IF(APPLE)
    FIND_LIBRARY(SYS_CONFIG_LIBRARY SystemConfiguration )
    MARK_AS_ADVANCED(SYS_CONFIG_LIBRARY)
    SET(TARGET_EXTERNAL_LIBRARIES iconv ${SYS_CONFIG_LIBRARY})
ENDIF(APPLE)

if(${MAK_PLATFORM} STREQUAL "linux-rhentws7" OR "${MAK_PLATFORM}" STREQUAL "win32-msvc++15.0" )
   set(BOOST_DEFINITION_FLAGS ${BOOST_DEFINITION_FLAGS} -DBOOST_FILESYSTEM_VERSION=2)
endif(${MAK_PLATFORM} STREQUAL "linux-rhentws7" OR "${MAK_PLATFORM}" STREQUAL "win32-msvc++15.0" )

SET(TARGET_LIBRARIES_VARS FBX_LIBRARY)
SET(TARGET_ADDED_LIBRARIES osgAnimation osgSim)

#### end var setup  ###
SETUP_PLUGIN(fbx)
