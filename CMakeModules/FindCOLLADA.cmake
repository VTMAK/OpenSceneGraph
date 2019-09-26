# Locate Collada
# This module defines:
# COLLADA_INCLUDE_DIR, where to find the headers
#
# COLLADA_LIBRARY, COLLADA_LIBRARY_DEBUG
# COLLADA_FOUND, if false, do not try to link to Collada dynamically
#
# COLLADA_LIBRARY_STATIC, COLLADA_LIBRARY_STATIC_DEBUG
# COLLADA_STATIC_FOUND, if false, do not try to link to Collada statically
#
# $COLLADA_DIR is an environment variable that would
# correspond to the ./configure --prefix=$COLLADA_DIR
#
# Created by Robert Osfield. 


# Check if COLLADA_DIR is set, otherwise use ACTUAL_3DPARTY_DIR:
SET(COLLADA_DOM_ROOT "${COLLADA_BASE_DIR}" CACHE PATH "Location of Collada DOM directory" FORCE)

IF(APPLE)
    SET(COLLADA_BUILDNAME "mac")
    SET(COLLADA_BOOST_BUILDNAME ${COLLADA_BUILDNAME})
ELSEIF(MINGW)
    SET(COLLADA_BUILDNAME "mingw")
    SET(COLLADA_BOOST_BUILDNAME ${COLLADA_BUILDNAME})
ELSEIF((MSVC_VERSION GREATER 1910) OR (MSVC_VERSION EQUAL 1910))
    SET(COLLADA_BUILDNAME "vc14")
    SET(COLLADA_BOOST_BUILDNAME "vc141")
ELSEIF(MSVC_VERSION EQUAL 1900)
    SET(COLLADA_BUILDNAME "vc14")
    SET(COLLADA_BOOST_BUILDNAME "vc140")
ELSEIF(MSVC_VERSION EQUAL 1800)
    SET(COLLADA_BUILDNAME "vc12")
    SET(COLLADA_BOOST_BUILDNAME "vc120")
ELSEIF(MSVC_VERSION EQUAL 1700)
    SET(COLLADA_BUILDNAME "vc11")
    SET(COLLADA_BOOST_BUILDNAME "vc110")
ELSEIF(MSVC_VERSION EQUAL 1600)
    SET(COLLADA_BUILDNAME "vc10")
    SET(COLLADA_BOOST_BUILDNAME "vc100")
ELSEIF(MSVC_VERSION EQUAL 1500)
    SET(COLLADA_BUILDNAME "vc9")
    SET(COLLADA_BOOST_BUILDNAME "vc90")
ELSEIF(MSVC_VERSION EQUAL 1400)
    SET(COLLADA_BUILDNAME "vc8")
    SET(COLLADA_BOOST_BUILDNAME "vc80")
ELSE()
  SET(COLLADA_BUILDNAME "linux")
  SET(COLLADA_BOOST_BUILDNAME ${COLLADA_BUILDNAME})
ENDIF()

IF(${CMAKE_VS_PLATFORM_TOOLSET})
    string(REPLACE "v" "vc" COLLADA_BOOST_BUILDNAME ${CMAKE_VS_PLATFORM_TOOLSET})
ENDIF()

FIND_LIBRARY(COLLADA_DYNAMIC_LIBRARY 
    NAMES collada_dom collada14dom Collada14Dom libcollada14dom21 libcollada14dom22 collada-dom2.5-dp collada-dom2.5-dp-${COLLADA_BOOST_BUILDNAME}-mt collada-dom2.4-dp collada-dom2.4-dp-${COLLADA_BOOST_BUILDNAME}-mt
    PATHS
    ${COLLADA_LIB_DIR}
    ${COLLADA_DOM_ROOT}/build/${COLLADA_BUILDNAME}-1.4
    $ENV{COLLADA_DIR}/build/${COLLADA_BUILDNAME}-1.4
    $ENV{COLLADA_DIR}/lib
    $ENV{COLLADA_DIR}/lib-dbg
    $ENV{COLLADA_DIR}
    ${COLLADA_DOM_ROOT}/lib
    ~/Library/Frameworks
    /Library/Frameworks    
    /opt/local/Library/Frameworks #macports
    /usr/local/lib
    /usr/local/lib64
    /usr/lib
    /usr/lib64
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
    /usr/freeware/lib64
    ${ACTUAL_3DPARTY_DIR}/lib
)

FIND_LIBRARY(COLLADA_DYNAMIC_LIBRARY_DEBUG 
    NAMES collada_dom-d collada14dom-d Collada14Dom-d libcollada14dom21-d libcollada14dom22-d collada14dom2-${COLLADA_BUILDNAME}-mtd
    PATHS
    ${COLLADA_LIB_DIR}
    ${COLLADA_DOM_ROOT}/build/${COLLADA_BUILDNAME}-1.4-d
    $ENV{COLLADA_DIR}/build/${COLLADA_BUILDNAME}-1.4-d
    $ENV{COLLADA_DIR}/lib
    $ENV{COLLADA_DIR}/lib-dbg
    $ENV{COLLADA_DIR}
    ${COLLADA_DOM_ROOT}/lib
    ~/Library/Frameworks
    /Library/Frameworks
    /opt/local/Library/Frameworks #macports
    /usr/local/lib
    /usr/local/lib64
    /usr/lib
    /usr/lib64
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
    /usr/freeware/lib64
    ${ACTUAL_3DPARTY_DIR}/lib
)

    # find extra libraries that the static linking requires
    SET(COLLADA_LIBXML_LIBRARY "${LIBXML2_LIBRARIES}" CACHE FILEPATH "" FORCE)
    SET(COLLADA_ZLIB_LIBRARY "${ZLIB_LIBRARY}" CACHE FILEPATH "" FORCE)
    FIND_LIBRARY(COLLADA_PCRECPP_LIBRARY
        NAMES pcrecpp
        PATHS
        ${PCRE_LIB_DIR}
    )

    FIND_LIBRARY(COLLADA_PCRECPP_LIBRARY_DEBUG
        NAMES pcrecpp-d pcrecppd
        PATHS
        ${PCRE_LIB_DIR}
    )

    FIND_LIBRARY(COLLADA_PCRE_LIBRARY
        NAMES pcre
        PATHS
        ${PCRE_LIB_DIR}
    )

    FIND_LIBRARY(COLLADA_PCRE_LIBRARY_DEBUG
        NAMES pcre-d pcred
        PATHS
        ${PCRE_LIB_DIR}
    )

    FIND_LIBRARY(COLLADA_MINIZIP_LIBRARY
        NAMES minizip
        PATHS
        ${MINIZIP_LIB_DIR}
    )

    FIND_LIBRARY(COLLADA_MINIZIP_LIBRARY_DEBUG
        NAMES minizip-d minizipD
        PATHS
        ${MINIZIP_LIB_DIR}
    )

    SET (COLLADA_BOOST_FILESYSTEM_LIBRARY 
         ${Boost_FILESYSTEM_LIBRARY_RELEASE}
         CACHE FILEPATH "path to release boost filesystem library")
    SET (COLLADA_BOOST_FILESYSTEM_LIBRARY_DEBUG 
         ${Boost_FILESYSTEM_LIBRARY_DEBUG}
         CACHE FILEPATH "Path to debug boost filesystem library")
    SET (COLLADA_BOOST_SYSTEM_LIBRARY 
         ${Boost_SYSTEM_LIBRARY_RELEASE}
         CACHE FILEPATH "Path to release boost system library")
    SET (COLLADA_BOOST_SYSTEM_LIBRARY_DEBUG 
         ${Boost_SYSTEM_LIBRARY_DEBUG}
         CACHE FILEPATH "Path to debug boost system library")
    SET (COLLADA_BOOST_INCLUDE_DIR
         ${Boost_INCLUDE_DIR}
         CACHE FILEPATH "Path to boost includes")

SET(COLLADA_FOUND "NO")
IF(COLLADA_DYNAMIC_LIBRARY OR COLLADA_STATIC_LIBRARY)
    IF   (COLLADA_INCLUDE_DIR)
        SET(COLLADA_FOUND "YES")
    ENDIF(COLLADA_INCLUDE_DIR)
ENDIF(COLLADA_DYNAMIC_LIBRARY OR COLLADA_STATIC_LIBRARY)
