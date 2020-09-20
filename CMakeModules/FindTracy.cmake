# prefer FindTracy from cmake distribution (if it exists)
if(EXISTS ${CMAKE_ROOT}/Modules/FindTracy.cmake)
  include(${CMAKE_ROOT}/Modules/FindTracy.cmake)

  if(TRACY_FOUND)
    return()
  endif()
endif()

SET(TRACY_FOUND "NO")
if (WIN32) # Only supported on Windows
  FIND_PATH(TRACY_INCLUDE_DIR TracyClient/Tracy.hpp
      ~/Library/Frameworks
      /Library/Frameworks
      /usr/local/include
      /usr/include
      /sw/include # Fink
      /opt/local/include # DarwinPorts
      /opt/csw/include # Blastwave
      /opt/include
      /usr/freeware/include
  )

  FIND_LIBRARY(TRACY_LIBRARY 
      NAMES TracyProfiler
      PATHS
      ~/Library/Frameworks
      /Library/Frameworks
      /usr/local/lib
      /usr/lib
      /sw/lib
      /opt/local/lib
      /opt/csw/lib
      /opt/lib
      /usr/freeware/lib64
      PATH_SUFFIXES
      lib64
  )

  IF(TRACY_LIBRARY AND TRACY_INCLUDE_DIR)
      SET(TRACY_FOUND "YES")
  ENDIF(TRACY_LIBRARY AND TRACY_INCLUDE_DIR)
endif()
