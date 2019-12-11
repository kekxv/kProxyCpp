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
else ()
    find_library(dl dl)
    set(dl_LIBRARIES dl)
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
if (APPLE)
    find_package(libevent REQUIRED)
    find_package(openssl REQUIRED)
else ()
    find_package(libevent)
    find_library(libevent libevent)
    find_package(openssl)
    find_library(openssl openssl)
endif ()
include_directories(
        ${LIBEVENT_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
)
if (APPLE)
    link_directories(/usr/local/lib/)
endif ()
if (LIBEVENT_LIBRARIES MATCHES "")
    # message("${LIBEVENT_STATIC_LIBRARIES} ${LIBEVENT_SHARED_LIBRARIES}")
    set(LIBEVENT_LIBRARIES event)
endif ()


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

add_executable(kProxyCpp main.cpp)


target_compile_definitions(
        kProxy
        PUBLIC -D_kProxy_HEADER_
)
target_link_libraries(kProxy
        ${dl_LIBRARIES}
        ${OPENSSL_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
        ${LIBEVENT_LIBRARIES} ${LIBEVENT_SHARED_LIBRARIES}
        ${libevhtp_LIBRARIES}
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
