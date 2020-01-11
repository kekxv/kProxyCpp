

#[[
# use this cmake file
find_library(PkgConfig REQUIRED)
add_subdirectory(clangTools)
set(libTools_DIR "${CMAKE_CURRENT_SOURCE_DIR}/clangTools")
include(clangTools/libTools.cmake)
include(clangTools/cmake/MSVC.cmake)
include_directories(${libTools_INCLUDE_DIR})
]]

set(libkProxyCpp_LIBRARIES kProxy)

find_path(libkProxy_DIR "libkProxy.cmake" DOC "Root directory of libkProxy")
if (EXISTS "${libkProxy_DIR}/libkProxy.cmake")
    set(libkProxy_INCLUDE_DIR
            ${libkProxy_DIR}/kProxy/
            ${libkProxy_DIR}/kProxy/kHttpd/
    )

else ()
    message(FATAL_ERROR "cannot find libkProxy")
endif ()