FIND_PACKAGE(Boost REQUIRED)

ADD_DEFINITIONS(${BOOST_DEFINITION_FLAGS})

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
    link_directories(${XML_LIB_DIR})
ENDIF(WIN32)

IF(APPLE)
    FIND_LIBRARY(SYS_CONFIG_LIBRARY SystemConfiguration )
    MARK_AS_ADVANCED(SYS_CONFIG_LIBRARY)
    SET(TARGET_EXTERNAL_LIBRARIES iconv ${SYS_CONFIG_LIBRARY})
ENDIF(APPLE)

if(${MAK_PLATFORM} STREQUAL "linux-rhentws7")
    ADD_DEFINITIONS( -DBOOST_FILESYSTEM_VERSION=2 )
else(${MAK_PLATFORM} STREQUAL "linux-rhentws7")
    ADD_DEFINITIONS( -DBOOST_FILESYSTEM_VERSION=3 )
endif(${MAK_PLATFORM} STREQUAL "linux-rhentws7")

SET(TARGET_LIBRARIES_VARS FBX_LIBRARY)

IF(WIN32)
    get_filename_component(FBX_LIBRARY_DIR_RELEASE "${FBX_LIBRARY}" DIRECTORY)
    get_filename_component(FBX_LIBRARY_DIR_DEBUG "${FBX_LIBRARY_DEBUG}" DIRECTORY)

    SET(FBX_XML_LIBRARY_RELEASE "${FBX_LIBRARY_DIR_RELEASE}/libxml2-md.lib")
    SET(FBX_XML_LIBRARY_DEBUG "${FBX_LIBRARY_DIR_DEBUG}/libxml2-md.lib")

    IF(NOT EXISTS "${FBX_XML_LIBRARY_RELEASE}" OR NOT EXISTS "${FBX_XML_LIBRARY_DEBUG}")
        MESSAGE(FATAL_ERROR
            "The FBX plugin must link against the FBX SDK's bundled libxml2-md libraries. "
            "Expected ${FBX_XML_LIBRARY_RELEASE} and ${FBX_XML_LIBRARY_DEBUG}.")
    ENDIF()

    LIST(APPEND TARGET_LIBRARIES_VARS FBX_XML_LIBRARY)
ENDIF(WIN32)

SET(TARGET_ADDED_LIBRARIES osgAnimation osgSim)

#### end var setup  ###
SETUP_PLUGIN(fbx)
