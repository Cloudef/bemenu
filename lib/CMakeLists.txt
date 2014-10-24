# Sources
SET(BEMENU_SOURCE
    menu.c
    item.c
    list.c
    util.c
    filter.c
    library.c
    3rdparty/cdl.c
    )

# Configure
CONFIGURE_FILE(version.h.in version.h @ONLY)

# Warnings
IF (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -Wextra -Wno-variadic-macros -Wno-long-long")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-variadic-macros -Wno-long-long")
ENDIF ()

IF (UNIX AND CMAKE_COMPILER_IS_GNUCC)
    SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
    IF (${CMAKE_VERSION} VERSION_LESS 2.8.9)
        ADD_DEFINITIONS(-fPIC)
    ENDIF ()
ENDIF ()

# Parse soversion version
STRING(REGEX MATCHALL "[0-9]+" VERSION_COMPONENTS ${BEMENU_VERSION})
LIST(GET VERSION_COMPONENTS 0 SOVERSION)

# Add include directories
INCLUDE_DIRECTORIES(${BEMENU_INCLUDE} ${CMAKE_CURRENT_BINARY_DIR})

# Compile renderer plugins
ADD_SUBDIRECTORY(renderers)

# Compile
ADD_LIBRARY(bemenu SHARED ${BEMENU_SOURCE})
SET_TARGET_PROPERTIES(bemenu PROPERTIES
    VERSION ${BEMENU_VERSION}
    SOVERSION ${SOVERSION})
TARGET_LINK_LIBRARIES(bemenu dl)

# Install
INSTALL(TARGETS bemenu DESTINATION lib)
INSTALL(FILES bemenu.h DESTINATION include)

# Unexport
SET(BEMENU_INCLUDES)
SET(BEMENU_INCLUDE_DIRS)
SET(BEMENU_LIBRARIES)

# Export
SET(BEMENU_INCLUDES "bemenu.h" CACHE STRING "bemenu includes exported from CMake")
SET(BEMENU_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "bemenu public header include path exported from CMake")
SET(BEMENU_LIBRARIES "bemenu" "dl" CACHE STRING "bemenu libraries exported from CMake")

# vim: set ts=8 sw=4 tw=0 :
