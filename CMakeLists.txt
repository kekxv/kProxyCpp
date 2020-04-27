cmake_minimum_required(VERSION 3.0)

if ("${PROJECT_NAME}" STREQUAL "Project")
    set(kProxyCpp_Other_Project ON)
elseif ("${PROJECT_NAME}" STREQUAL "")
    set(kProxyCpp_Other_Project ON)
endif ()

project(kProxyCpp)
set(CMAKE_DEBUG_POSTFIX "d")

set(CMAKE_CXX_STANDARD 14)

add_definitions(-D_LOGGER_USE_THREAD_POOL_)

find_library(PkgConfig REQUIRED)
add_subdirectory(clangTools)
set(libTools_DIR "${CMAKE_CURRENT_SOURCE_DIR}/clangTools")
include(clangTools/libTools.cmake)
include(clangTools/cmake/MSVC.cmake)
include_directories(${libTools_INCLUDE_DIR})

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



find_library(dl dl)
find_library(pthread pthread)


find_package(OpenCV)
if (OpenCV_FOUND)
    add_definitions(-DENABLE_OPENCV)

    if ("${kProxyCpp_Other_Project}" STREQUAL "ON")
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
    endif ()
    link_directories(${OpenCV_DIR})
    include_directories(
            ${OpenCV_INCLUDE_DIRS}
    )

endif ()

find_package(PkgConfig)
if (NOT "${PKG_CONFIG_PATH}" STREQUAL "")
    set(ENV{PKG_CONFIG_PATH} ${PKG_CONFIG_PATH})
endif ()

pkg_search_module(OPENSSL openssl)
if (OPENSSL_FOUND)
    add_definitions(-DENABLE_OPENSSL)

    include_directories(${OPENSSL_INCLUDE_DIRS})

    if ("${kProxyCpp_Other_Project}" STREQUAL "ON")
        message(STATUS "OPENSSL library status:")
        message(STATUS "    ${OPENSSL_VERSION}")
        message(STATUS "    libraries: ${OPENSSL_LIBRARIES}")
        message(STATUS "    lib_dir: ${OPENSSL_LIBRARY_DIRS}")
        message(STATUS "    include path: ${OPENSSL_INCLUDE_DIRS}")
    endif ()
    include_directories(${OPENSSL_INCLUDE_DIRS})
    link_directories(${OPENSSL_LIBRARY_DIRS})

endif ()
if (APPLE)
    link_directories(/usr/local/lib/)
endif ()



if (WIN32)
    set(getopt_src "tests/getopt.c" tests/test_opencv.cpp)
endif ()


if ("${kProxyCpp_Other_Project}" STREQUAL "ON")
    add_executable(kProxyCpp main.cpp ${getopt_src})
    target_link_libraries(kProxyCpp
            ${dl_LIBRARIES}
            ${libTools_LIBRARIES}
            ${OPENSSL_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
            ${OpenCV_LIBS}
            ${libTools_LIBRARIES}
            ${wsock32_LIBS}
            )
    option(ENABLE_FILENAME "option for FILENAME" ON)
    if (ENABLE_FILENAME)
        #重新定义当前目标的源文件的 __FILENAME__ 宏
        redefine_file_macro(kProxyCpp)
    endif ()

endif ()

option(ENABLE_TEST "option for ENABLE_TEST" OFF)
if (ENABLE_TEST AND OPENSSL_FOUND)
    add_executable(openssl_server tests/openssl_server.cpp)
    add_executable(openssl_client tests/openssl_client.cpp)
    add_executable(ssl_client_nonblock tests/openssl_examples-master/ssl_client_nonblock.c)
    add_executable(ssl_server_nonblock tests/openssl_examples-master/ssl_server_nonblock.c)
    redefine_file_macro(openssl_server)
    redefine_file_macro(openssl_client)
    redefine_file_macro(ssl_client_nonblock)
    redefine_file_macro(ssl_server_nonblock)
    target_link_libraries(openssl_server
            ${OPENSSL_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
            )
    target_link_libraries(openssl_client
            ${OPENSSL_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
            )
    target_link_libraries(ssl_server_nonblock
            ${OPENSSL_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
            )
    target_link_libraries(ssl_client_nonblock
            ${OPENSSL_LIBRARIES} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
            )
endif ()


########安装脚本##########
install(FILES
        ${include_header}
        DESTINATION include)

if ("${kProxyCpp_Other_Project}" STREQUAL "ON")
    install(TARGETS kProxyCpp
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib)
    install(FILES
            main.cpp
            DESTINATION Example)


    # 复制 lib 代码到 lib 下
    install(DIRECTORY Example/ DESTINATION Example
            PATTERN "Example/*"
            PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
            GROUP_EXECUTE GROUP_READ)
endif ()

