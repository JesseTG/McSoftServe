# Detects the presence of various libraries or functions and sets the appropriate HAVE_* variables.
# Other files are used to actually configure the build.



option(ENABLE_DYNAMIC "Build with dynamic library support, if supported by the target." ON)
option(ENABLE_EGL "Build with EGL support, if supported by the target." OFF)
option(ENABLE_NETWORKING "Build with networking support, if supported by the target." ON)
option(ENABLE_SCCACHE "Build with sccache instead of ccache, if available." OFF)
option(ENABLE_ZLIB "Build with zlib support, if supported by the target." ON)
option(ENABLE_GLSM_DEBUG "Enable debug output for GLSM." OFF)

if (ENABLE_SCCACHE)
    find_program(SCCACHE "sccache" PATHS "$ENV{HOME}/.cargo/bin")
    if (SCCACHE)
        message(STATUS "Using sccache (instead of ccache) to speed up compilation")
        set(CMAKE_C_COMPILER_LAUNCHER ${SCCACHE})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE})
    else()
        message(WARNING "ENABLE_SCCACHE is on, but sccache was not found.")
    endif()
endif ()

find_package(Threads REQUIRED)

if (Threads_FOUND)
    set(HAVE_THREADS ON)
endif ()

if (ENABLE_ZLIB)
    set(HAVE_ZLIB ON)
endif ()

if (ENABLE_GLSM_DEBUG)
    set(HAVE_GLSM_DEBUG ON)
endif ()

if (ENABLE_OPENGL)
    # ENABLE_OGLRENDERER is defined by melonDS's CMakeLists.txt
    if (OPENGL_PROFILE STREQUAL "OpenGL")
        if (ENABLE_EGL)
            find_package(OpenGL OPTIONAL_COMPONENTS EGL)
        else ()
            find_package(OpenGL)
        endif ()
    elseif (OPENGL_PROFILE STREQUAL "OpenGLES2")
        # Built-in support for finding OpenGL ES isn't available until CMake 3.27,
        # so we use an external module.
        find_package(OpenGLES OPTIONAL_COMPONENTS V2)
    elseif (OPENGL_PROFILE STREQUAL "OpenGLES3")
        find_package(OpenGLES OPTIONAL_COMPONENTS V3)
    elseif (OPENGL_PROFILE STREQUAL "OpenGLES31")
        find_package(OpenGLES OPTIONAL_COMPONENTS V31)
    elseif (OPENGL_PROFILE STREQUAL "OpenGLES32")
        find_package(OpenGLES OPTIONAL_COMPONENTS V32)
    else()
        get_property(OpenGLProfiles CACHE OPENGL_PROFILE PROPERTY STRINGS)
        message(FATAL_ERROR "Expected an OpenGL profile in '${OpenGLProfiles}', got '${OPENGL_PROFILE}'")
    endif()

    if (ENABLE_EGL AND OpenGL_EGL_FOUND)
        set(HAVE_EGL ON)
    endif()

    if (OpenGL_OpenGL_FOUND)
        set(HAVE_OPENGL ON)
    endif()

    if (OpenGLES_V1_FOUND)
        set(HAVE_OPENGLES ON)
        set(HAVE_OPENGLES1 ON)
    endif ()

    if (OpenGL::GLES2 OR OpenGLES_V2_FOUND)
        set(HAVE_OPENGLES ON)
        set(HAVE_OPENGLES2 ON)
    endif ()

    if (OpenGL::GLES3 OR OpenGLES_V3_FOUND)
        set(HAVE_OPENGLES ON)
        set(HAVE_OPENGLES3 ON)
    endif ()

    if (OpenGLES_V31_FOUND)
        set(HAVE_OPENGLES ON)
        set(HAVE_OPENGLES31 ON)
    endif ()

    if (OpenGLES_V32_FOUND)
        set(HAVE_OPENGLES ON)
        set(HAVE_OPENGLES32 ON)
    endif ()

    check_include_files("GL3/gl3.h;GL3/gl3ext.h" HAVE_OPENGL_MODERN)

    if (HAVE_OPENGL OR HAVE_OPENGLES)
        set(ENABLE_OGLRENDERER ON)
    else()
        set(ENABLE_OGLRENDERER OFF CACHE BOOL "Enable OpenGL renderer" FORCE)
        set(ENABLE_OPENGL OFF CACHE BOOL "Enable OpenGL renderer" FORCE)
    endif()
else()
    set(ENABLE_OGLRENDERER OFF CACHE BOOL "Enable OpenGL renderer" FORCE)
    set(ENABLE_OPENGL OFF CACHE BOOL "Enable OpenGL renderer" FORCE)
endif()

if (ENABLE_NETWORKING)
    set(HAVE_NETWORKING ON)

    if (WIN32)
        list(APPEND CMAKE_REQUIRED_LIBRARIES ws2_32)
        check_symbol_exists(getaddrinfo "winsock2.h;ws2tcpip.h" HAVE_GETADDRINFO)
    else()
        check_symbol_exists(getaddrinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETADDRINFO)
    endif ()

    if (NOT HAVE_GETADDRINFO)
        set(HAVE_SOCKET_LEGACY ON)
    endif()
endif ()

check_symbol_exists(strlcpy "bsd/string.h;string.h" HAVE_STRL)
check_symbol_exists(mmap "sys/mman.h" HAVE_MMAP)
check_include_file("sys/mman.h" HAVE_MMAN)

if (ENABLE_DYNAMIC)
    get_cmake_property(HAVE_DYNAMIC TARGET_SUPPORTS_SHARED_LIBS)
    message(STATUS "Target supports shared libraries")
endif ()

if (IOS)
    set(HAVE_COCOATOUCH ON)
endif ()

if (NINTENDO_SWITCH OR ("${CMAKE_SYSTEM_NAME}" STREQUAL "NintendoSwitch"))
    set(HAVE_LIBNX ON)
    set(SWITCH ON)
    message(STATUS "Building for Nintendo Switch")
endif()

if ("${CMAKE_ANDROID_ARM_NEON}" OR "${HAVE_LIBNX}" OR ("arm64" IN_LIST "${CMAKE_OSX_ARCHITECTURES}") OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64") OR ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64"))
    # The Switch's CPU supports NEON, as does iOS
    set(HAVE_NEON ON)
    message(STATUS "Building with ARM NEON optimizations")
endif ()

if (ENABLE_NETWORKING AND (WIN32 OR UNIX) AND HAVE_DYNAMIC)
    set(HAVE_NETWORKING_DIRECT_MODE ON)
    message(STATUS "Building with support for direct-mode networking")
endif()


function(add_common_definitions TARGET)
    if (APPLE)
        target_compile_definitions(${TARGET} PUBLIC GL_SILENCE_DEPRECATION)
        # macOS has deprecated OpenGL, and its headers spit out a lot of warnings
    endif()

    if (HAVE_NEON)
        target_compile_definitions(${TARGET} PUBLIC HAVE_NEON)
    endif ()

    if (HAVE_ARM_NEON_ASM_OPTIMIZATIONS)
        target_compile_definitions(${TARGET} PUBLIC HAVE_ARM_NEON_ASM_OPTIMIZATIONS)
    endif ()

    if (HAVE_COCOATOUCH)
        target_compile_definitions(${TARGET} PUBLIC HAVE_COCOATOUCH)
    endif ()

    if (HAVE_NETWORKING_DIRECT_MODE AND HAVE_DYNAMIC)
        target_compile_definitions(${TARGET} PUBLIC HAVE_NETWORKING_DIRECT_MODE)
    endif ()

    if (HAVE_DYNAMIC)
        target_compile_definitions(${TARGET} PUBLIC HAVE_DYNAMIC HAVE_DYLIB)
    endif ()

    if (HAVE_EGL)
        target_compile_definitions(${TARGET} PUBLIC HAVE_EGL)
    endif ()

    if (HAVE_GLSM_DEBUG)
        target_compile_definitions(${TARGET} PUBLIC GLSM_DEBUG)
    endif ()

    if (HAVE_LIBNX)
        target_compile_definitions(${TARGET} PUBLIC HAVE_LIBNX)
    endif ()

    if (HAVE_MMAP)
        target_compile_definitions(${TARGET} PUBLIC HAVE_MMAP)
    endif ()

    if (HAVE_NETWORKING)
        target_compile_definitions(${TARGET} PUBLIC HAVE_NETWORKING)

        if (HAVE_GETADDRINFO)
            target_compile_definitions(${TARGET} PUBLIC HAVE_GETADDRINFO)
        endif ()

        if (HAVE_SOCKET_LEGACY)
            target_compile_definitions(${TARGET} PUBLIC HAVE_SOCKET_LEGACY)
        endif ()
    endif ()

    if (HAVE_OPENGL)
        target_compile_definitions(${TARGET} PUBLIC HAVE_OPENGL OGLRENDERER_ENABLED CORE ENABLE_OGLRENDERER PLATFORMOGL_H)
    endif ()

    if (HAVE_OPENGL_MODERN)
        target_compile_definitions(${TARGET} PUBLIC HAVE_OPENGL_MODERN)
    endif ()

    if (HAVE_OPENGLES)
        target_compile_definitions(${TARGET} PUBLIC HAVE_OPENGLES)
    endif ()

    if (HAVE_OPENGLES1)
        target_compile_definitions(${TARGET} PUBLIC HAVE_OPENGLES1 HAVE_OPENGLES_1)
    endif ()

    if (HAVE_OPENGLES2)
        target_compile_definitions(${TARGET} PUBLIC HAVE_OPENGLES2 HAVE_OPENGLES_2)
    endif ()

    if (HAVE_OPENGLES3)
        target_compile_definitions(${TARGET} PUBLIC HAVE_OPENGLES3 HAVE_OPENGLES_3)
    endif ()

    if (HAVE_OPENGLES31)
        target_compile_definitions(${TARGET} PUBLIC HAVE_OPENGLES31 HAVE_OPENGLES_31 HAVE_OPENGLES_3_1)
    endif ()

    if (HAVE_OPENGLES32)
        target_compile_definitions(${TARGET} PUBLIC HAVE_OPENGLES32 HAVE_OPENGLES_32 HAVE_OPENGLES_3_2)
    endif ()

    if (HAVE_STRL)
        target_compile_definitions(${TARGET} PUBLIC HAVE_STRL)
    endif ()

    if (HAVE_THREADS)
        target_compile_definitions(${TARGET} PUBLIC HAVE_THREADS)
    endif ()

    if (HAVE_ZLIB)
        target_compile_definitions(${TARGET} PUBLIC HAVE_ZLIB)
    endif ()

    if (SWITCH)
        target_compile_definitions(${TARGET} PUBLIC SWITCH __SWITCH__)
    endif ()
endfunction()

# TODO: Detect if SSL is available; if so, define HAVE_SSL