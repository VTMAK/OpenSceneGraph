# CMake package for finding concurrency view macros
FIND_PATH(
    CVMACROS_INCLUDE_DIR "cvmarkers.h"
    PATH_SUFFIXES "include/cvmarkers"
)

SET(CVMACROS_FOUND "NO")
IF (CVMACROS_INCLUDE_DIR)
    SET(CVMACROS_FOUND "YES")
ENDIF(CVMACROS_INCLUDE_DIR)
