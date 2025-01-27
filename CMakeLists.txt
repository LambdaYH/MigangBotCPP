cmake_minimum_required(VERSION 3.12)
project(MigangBot)
set(CMAKE_CXX_STANDARD 20)

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb") #debug模式下开启-g选项
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

option(BUILD_TEST "build test" ON)

# threads
find_package(Threads REQUIRED)

# mysql
include(FindPkgConfig)
pkg_check_modules(LIBMYSQLCLIENT REQUIRED mysqlclient)
link_directories(${LIBMYSQLCLIENT_LIBRARY_DIRS})

# opencv
find_package(OpenCV REQUIRED)
IF(OpenCV_FOUND)
    include_directories(${OpenCV_INCLUDE_DIRS})
ENDIF(OpenCV_FOUND)

# yaml-cpp
add_subdirectory(third-party/yaml-cpp)

# nlohmann/json
set(JSON_Install OFF CACHE INTERNAL "")
set(JSON_ImplicitConversions OFF CACHE INTERNAL "Enable implicit conversions.")
add_subdirectory(third-party/json)

# fmt
add_subdirectory(third-party/fmt)

# spdlog
set(SPDLOG_FMT_EXTERNAL ON CACHE INTERNAL "Use external fmt library instead of bundled")
add_subdirectory(third-party/spdlog)

# libhv
set(BUILD_SHARED OFF CACHE INTERNAL "build shared library")
set(BUILD_EXAMPLES OFF CACHE INTERNAL "build examples")
set(BUILD_UNITTEST OFF CACHE INTERNAL "build unittest")
set(WITH_OPENSSL ON CACHE INTERNAL "with openssl library")
add_subdirectory(third-party/libhv)

# oneTBB
set(TBB_TEST OFF CACHE INTERNAL "Enable testing")
add_subdirectory(third-party/oneTBB)

# utfcpp
add_subdirectory(third-party/utfcpp)

# zlib
add_subdirectory(third-party/zlib)

# hiredis
add_subdirectory(third-party/hiredis)

# pugixml
add_subdirectory(third-party/pugixml)

# libxml2
set(LIBXML2_WITH_PYTHON OFF CACHE INTERNAL "Build Python bindings")
add_subdirectory(third-party/libxml2)

# sqlpp11
set(BUILD_MYSQL_CONNECTOR ON CACHE INTERNAL "Build MySQL Connector")
add_subdirectory(third-party/sqlpp11)

# pcre2 (for fast regex)
set(PCRE2_SUPPORT_JIT ON CACHE INTERNAL "")
add_subdirectory(third-party/pcre2)

# cocoyaxi(coroutines library)
add_subdirectory(third-party/cocoyaxi)

# gumbo_query
add_subdirectory(third-party/gumbo-query)

# tencentcloud-sdk-cpp
set(BUILD_MODULES CACHE INTERNAL "nlp")
set(Tencentcloud-sdk-cpp_LIBS
    ${CMAKE_SOURCE_DIR}/libs/libtencentcloud-sdk-cpp-nlp.a
    ${CMAKE_SOURCE_DIR}/libs/libtencentcloud-sdk-cpp-core.a
    crypto
    curl
    uuid
)

file(GLOB_RECURSE SRCS source/modules/module/*cpp)

add_executable(${PROJECT_NAME} 
                source/main.cpp
                ${SRCS}
                third-party/cpp-base64/base64.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
                            source 
                            third-party/include
                            third-party/cpp-base64
                            third-party/cocoyaxi/include
                            ${CMAKE_BINARY_DIR}/third-party/libhv/include
                            third-party/jpcre2/src
                            third-party/gumbo-query/src
                            ${LIBMYSQLCLIENT_INCLUDE_DIRS}
)    

target_link_libraries(${PROJECT_NAME} PRIVATE 
                        Threads::Threads 
                        yaml-cpp 
                        nlohmann_json::nlohmann_json 
                        hv_static 
                        spdlog
                        fmt::fmt
                        ${OpenCV_LIBS}
                        TBB::tbb
                        libmysqlclient.a
                        cocoyaxi::co
                        zlibstatic
                        utf8::cpp
                        ${Tencentcloud-sdk-cpp_LIBS}
                        hiredis_static
                        pugixml-static
                        pcre2-8-static
                        gumbo_query_static
                        libgumbo.a
                        LibXml2
                        sqlpp11
)

IF(BUILD_TEST)
    add_subdirectory(test)
ENDIF()