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
include(CheckSymbolExists)
include(cmake/FetchDependencies.cmake)
include(cmake/ConfigureFeatures.cmake)
include(cmake/libretro-common.cmake)
include(embed-binaries)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(mctaylor_libretro MODULE
    libretro.cpp
    pntr/pntr.c
)

add_common_definitions(mctaylor_libretro)
add_common_definitions(libretro-common)

add_compile_definitions(
    PNTR_ENABLE_TTF
    PNTR_ENABLE_FILTER_SMOOTH
    PNTR_ENABLE_JPEG
    PNTR_ENABLE_MATH
    PNTR_ENABLE_VARGS
    PNTR_PIXELFORMAT_RGBA
    STB_IMAGE_STATIC # To avoid using any dynamically-linked STB implementations
)

embed_binaries(libretro-assets
    ASSET
        NAME "mctaylor_freezer"
        PATH "assets/freezer.wav"
    ASSET
        NAME "mctaylor_bg"
        PATH "assets/bg-steel.png"
    ASSET
        NAME "mctaylor_bg_matte"
        PATH "assets/bg-matte.jpg"
    ASSET
        NAME "mctaylor_lcd_font"
        PATH "assets/enhanced_led_board-7.ttf"
    ASSET
        NAME "mctaylor_panel"
        PATH "assets/panel.png"
)


target_include_directories(mctaylor_libretro SYSTEM PUBLIC
    "${libretro-common_SOURCE_DIR}/include"
    "${pntr_SOURCE_DIR}"
    "${pntr_nuklear_SOURCE_DIR}/include"
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

target_link_libraries(mctaylor_libretro PUBLIC libretro-common pntr pntr_nuklear libretro-assets)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Defining DEBUG in mctaylor_libretro and libretro-common targets")
    target_compile_definitions(mctaylor_libretro PUBLIC DEBUG)
    target_compile_definitions(libretro-common PUBLIC DEBUG)
endif ()