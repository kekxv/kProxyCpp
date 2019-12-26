cmake_minimum_required(VERSION 3.0)
project(kProxyCpp)
set(CMAKE_DEBUG_POSTFIX "d")

set(CMAKE_CXX_STANDARD 14)

add_definitions(-D_LOGGER_USE_THREAD_POOL_)

find_library(PkgConfig REQUIRED)
add_subdirectory(clangTools)
get_property(libTools_LIBRARIES GLOBAL PROPERTY "libTools_LIBRARIES")
INCLUDE_DIRECTORIES(clangTools/src)

if (WIN32)
    include_directories(tests)
else ()
    find_library(dl dl)
    set(dl_LIBRARIES dl pthread)
endif ()


include(CheckSymbolExists)
include(CheckCXXSymbolExists)
try_compile(HAVE_EXPERIMENTAL_FILESYSTEM "${CMAKE_BINARY_DIR}/temp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/has_filesystem.cpp"
        CMAKE_FLAGS -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON
        LINK_LIBRARIES stdc++fs
        )

if (HAVE_EXPERIMENTAL_FILESYSTEM)
    message(STATUS "Compiler has filesystem support")

    add_definitions(-DHAVE_EXPERIMENTAL_FILESYSTEM)
    set(dl_LIBRARIES ${dl_LIBRARIES} stdc++fs)
endif ()


if (MSVC)
    # 设置为 v140_xp 编译工具编译
    # set(CMAKE_GENERATOR_TOOLSET "v140_xp")
    # set(CMAKE_GENERATOR_PLATFORM "Win32")
    set(CompilerFlags
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            )
    foreach (CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach ()
    # It should be set to 1 for the static MFC library, and 2 for the shared MFC library.
    set(CMAKE_MFC_FLAG 1)
    #add_definitions(-DUNICODE -D_UNICODE)
    add_definitions(-D_AFXDLL)
endif (MSVC)

include_directories(
        .
        kProxy
        kProxy/kHttpd
)


find_library(dl dl)
find_library(pthread pthread)


find_package(OpenCV)
if (OpenCV_FOUND)
    add_definitions(-DENABLE_OPENCV)
    # If the package has been found, several variables will
    # be set, you can find the full list with descriptions
    # in the OpenCVConfig.cmake file.
    # Print some message showing some of them
    message(STATUS "OpenCV library status:")
    message(STATUS "    version: ${OpenCV_VERSION}")
    message(STATUS "    libraries: ${OpenCV_LIBS}")
    message(STATUS "    libraries: ${OpenCV_LIBRARIES}")
    message(STATUS "    lib_dir: ${OpenCV_LIB_DIR}")
    message(STATUS "    include path: ${OpenCV_INCLUDE_DIRS}")
    link_directories(${OpenCV_DIR})
    include_directories(
            ${OpenCV_INCLUDE_DIRS}
    )

endif ()

find_package(PkgConfig)
pkg_search_module(OPENSSL openssl)
if (OPENSSL_FOUND)
    add_definitions(-DENABLE_OPENSSL)

    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "OPENSSL library status:")
    message(STATUS "    ${OPENSSL_VERSION}")
    message(STATUS "    libraries: ${OPENSSL_LIBRARIES}")
    message(STATUS "    lib_dir: ${OPENSSL_LIBRARY_DIRS}")
    message(STATUS "    include path: ${OPENSSL_INCLUDE_DIRS}")
    include_directories(${OPENSSL_INCLUDE_DIRS})
    link_directories(${OPENSSL_LIBRARY_DIRS})

endif ()
if (APPLE)
    link_directories(/usr/local/lib/)
endif ()


#    get_cmake_property(_variableNames VARIABLES)
#    foreach (_variableName ${_variableNames})
#        message(STATUS "${_variableName}=${${_variableName}}")
#    endforeach()

#aux_source_directory(kHttpd srcSource)
FILE(GLOB_RECURSE srcSource
        "kProxy/*.c"
        "kProxy/*.cc"
        "kProxy/*.cpp"
        "kProxy/*.h"
        "kProxy/*.hpp"
        "kProxy/*/*"
        )

add_library(kProxy STATIC ${srcSource})

if (WIN32)
    set(getopt_src "tests/getopt.c")
endif ()
add_executable(kProxyCpp main.cpp ${getopt_src})


target_compile_definitions(
        kProxy
        PUBLIC -D_kProxy_HEADER_
)
target_link_libraries(kProxy
        ${dl_LIBRARIES}
        ${OPENSSL_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
        ${OpenCV_LIBS}
        ${libTools_LIBRARIES}
        ${wsock32_LIBS}
        )
target_link_libraries(kProxyCpp kProxy)


option(ENABLE_FILENAME "option for FILENAME" ON)
if (ENABLE_FILENAME)
    #重新定义当前目标的源文件的 __FILENAME__ 宏
    redefine_file_macro(kProxy)
    redefine_file_macro(kProxyCpp)
endif ()

########安装脚本##########
install(TARGETS kProxyCpp kProxy
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

FILE(GLOB_RECURSE include_header kProxy/*.h kProxy/*/*.h)
install(FILES
        ${include_header}
        DESTINATION include)

install(FILES
        main.cpp
        DESTINATION Example)


# 复制 lib 代码到 lib 下
install(DIRECTORY Example/ DESTINATION Example
        PATTERN "Example/*"
        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
        GROUP_EXECUTE GROUP_READ)

