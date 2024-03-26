cmake_minimum_required(VERSION 3.19)

cmake_policy(VERSION 3.19)
message(STATUS "Using CMake ${CMAKE_VERSION} from ${CMAKE_COMMAND}" )
if (CMAKE_MESSAGE_LOG_LEVEL)
    message(STATUS "Message log level is set to ${CMAKE_MESSAGE_LOG_LEVEL}")
endif ()

project("McTaylor"
        VERSION "1.0.0"
        DESCRIPTION "An experimental libretro core for the Taylor series of soft-serve ice cream machines, popularized by a well-known American fast food chain. Provides an accurate user experience without the need for firmware dumps."
        HOMEPAGE_URL "https://jesse.tg"
        LANGUAGES C CXX)

include(FetchContent)
include(cmake/FetchDependencies.cmake)
include(cmake/libretro-common.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(mctaylor_libretro MODULE
    libretro.cpp
    pntr/pntr.c
)

target_include_directories(mctaylor_libretro SYSTEM PUBLIC
    "${libretro-common_SOURCE_DIR}/include"
    "${pntr_SOURCE_DIR}"
)

# libretro cores do not start with "lib"
set_target_properties(mctaylor_libretro PROPERTIES PREFIX "")

# Some platforms or compilers don't use the expected suffixes for shared libraries
if (APPLE)
    set_target_properties(mctaylor_libretro PROPERTIES SUFFIX ".dylib")
elseif(ANDROID)
    set_target_properties(mctaylor_libretro PROPERTIES SUFFIX "_android.so")
elseif (UNIX)
    set_target_properties(mctaylor_libretro PROPERTIES SUFFIX ".so")
elseif (WIN32)
    set_target_properties(mctaylor_libretro PROPERTIES SUFFIX ".dll")
endif ()

if (WIN32 AND MINGW)
    target_link_options(mctaylor_libretro PUBLIC -static-libgcc -static-libstdc++ -static)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "(.+)?Clang")
    target_compile_options(mctaylor_libretro PUBLIC -Werror=return-type)
    # For some reason, C++ allows functions to not return values in all code paths.
    # This has tripped me up before, so I'm forcing it to be an error.
endif()

target_link_libraries(mctaylor_libretro PUBLIC libretro-common)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Defining DEBUG in mctaylor_libretro and libretro-common targets")
    target_compile_definitions(mctaylor_libretro PUBLIC DEBUG)
    target_compile_definitions(libretro-common PUBLIC DEBUG)
endif ()