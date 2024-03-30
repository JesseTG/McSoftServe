cmake_minimum_required(VERSION 3.11)

cmake_policy(VERSION 3.11)
message(STATUS "Using CMake ${CMAKE_VERSION} from ${CMAKE_COMMAND}" )
if (CMAKE_MESSAGE_LOG_LEVEL)
    message(STATUS "Message log level is set to ${CMAKE_MESSAGE_LOG_LEVEL}")
endif ()

project("McSoftServe"
        VERSION "1.0.0"
        DESCRIPTION "An experimental libretro core for the Taylor series of soft-serve ice cream machines, popularized by a well-known American fast food chain. Provides an accurate user experience without the need for firmware dumps."
        HOMEPAGE_URL "https://jesse.tg"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
include(CheckSymbolExists)
include(cmake/FetchDependencies.cmake)
include(cmake/ConfigureFeatures.cmake)
include(cmake/libretro-common.cmake)
include(embed-binaries)


add_library(mcsoftserve_libretro MODULE
    libretro.cpp
    pntr/pntr.c
)

add_common_definitions(mcsoftserve_libretro)
add_common_definitions(libretro-common)

add_compile_definitions(
    PNTR_ENABLE_TTF
    PNTR_ENABLE_FILTER_SMOOTH
    PNTR_ENABLE_MATH
    PNTR_ENABLE_VARGS
    PNTR_PIXELFORMAT_ARGB
    STB_IMAGE_STATIC # To avoid using any dynamically-linked STB implementations
    STBI_NO_THREAD_LOCALS
)

embed_binaries(libretro-assets
    ASSET
        NAME "mcsoftserve_freezer"
        PATH "assets/freezer.wav"
    ASSET
        NAME "mcsoftserve_beep"
        PATH "assets/beep.wav"
    ASSET
        NAME "mcsoftserve_bg"
        PATH "assets/bg.png"
    ASSET
        NAME "mcsoftserve_auto_button"
        PATH "assets/auto-button-l.png"
    ASSET
        NAME "mcsoftserve_sel_button"
        PATH "assets/sel-button.png"
    ASSET
        NAME "mcsoftserve_up_button"
        PATH "assets/up-button.png"
    ASSET
        NAME "mcsoftserve_topping_button"
        PATH "assets/topping-button-l.png"
    ASSET
        NAME "mcsoftserve_wash_button"
        PATH "assets/wash-button-l.png"
    ASSET
        NAME "mcsoftserve_standby_button"
        PATH "assets/standby-button-l.png"
    ASSET
        NAME "mcsoftserve_bg"
        PATH "assets/bg.png"
    ASSET
        NAME "mcsoftserve_lcd_font"
        PATH "assets/DOTMATRI.TTF"
)


target_include_directories(mcsoftserve_libretro SYSTEM PUBLIC
    "${libretro-common_SOURCE_DIR}/include"
    "${pntr_SOURCE_DIR}"
    "${pntr_nuklear_SOURCE_DIR}/include"
)

# libretro cores do not start with "lib"
set_target_properties(mcsoftserve_libretro PROPERTIES PREFIX "")

# Some platforms or compilers don't use the expected suffixes for shared libraries
if(EMSCRIPTEN AND LIBRETRO_STATIC)
    set_target_properties(mcsoftserve_libretro PROPERTIES SUFFIX "${LIBRETRO_SUFFIX}.bc")
elseif (LIBRETRO_SUFFIX AND LIBRETRO_STATIC)
    set_target_properties(mcsoftserve_libretro PROPERTIES SUFFIX "${LIBRETRO_SUFFIX}${CMAKE_STATIC_LIBRARY_SUFFIX}")
elseif (LIBRETRO_SUFFIX)
    set_target_properties(mcsoftserve_libretro PROPERTIES SUFFIX "${LIBRETRO_SUFFIX}${CMAKE_SHARED_LIBRARY_SUFFIX}")
elseif (APPLE)
    set_target_properties(mcsoftserve_libretro PROPERTIES SUFFIX ".dylib")
elseif(ANDROID)
    set_target_properties(mcsoftserve_libretro PROPERTIES SUFFIX "_android.so")
elseif (UNIX)
    set_target_properties(mcsoftserve_libretro PROPERTIES SUFFIX ".so")
elseif (WIN32)
    set_target_properties(mcsoftserve_libretro PROPERTIES SUFFIX ".dll")
endif ()

if (WIN32 AND MINGW)
    target_link_options(mcsoftserve_libretro PUBLIC -static-libgcc -static-libstdc++ -static)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "(.+)?Clang")
    target_compile_options(mcsoftserve_libretro PUBLIC -Werror=return-type)
    # For some reason, C++ allows functions to not return values in all code paths.
    # This has tripped me up before, so I'm forcing it to be an error.
endif()

target_link_libraries(mcsoftserve_libretro PUBLIC libretro-common pntr pntr_nuklear libretro-assets)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Defining DEBUG in mcsoftserve_libretro and libretro-common targets")
    target_compile_definitions(mcsoftserve_libretro PUBLIC DEBUG)
    target_compile_definitions(libretro-common PUBLIC DEBUG)
endif ()