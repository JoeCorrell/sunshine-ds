# windows specific dependencies

# MinHook setup - use installed minhook for AMD64, otherwise download minhook-detours for ARM64
#
# Tested for ARM64 rather than for AMD64, and defaulting to the installed
# MinHook. `CMAKE_SYSTEM_PROCESSOR` is empty under MSYS2/MinGW with Ninja --
# `project()` recomputes it and finds nothing to set it from -- so an exact
# "AMD64" match fails on a perfectly ordinary x64 host and quietly takes the
# ARM64 branch. The failure surfaces much later, at link, as
#
#   minhook-detours.ARM64.Release.lib: file format not recognized
#
# which points at MinHook rather than at the empty variable that chose it.
# Inverting the test means the fallback is the common case rather than the
# exotic one, so an unknown processor links the library that matches the
# compiler instead of one that cannot.
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
    # Make sure MinHook is installed for x86/x64
    find_library(MINHOOK_LIBRARY libMinHook.a REQUIRED)
    find_path(MINHOOK_INCLUDE_DIR MinHook.h PATH_SUFFIXES include REQUIRED)

    add_library(minhook::minhook STATIC IMPORTED)
    set_property(TARGET minhook::minhook PROPERTY IMPORTED_LOCATION ${MINHOOK_LIBRARY})
    target_include_directories(minhook::minhook INTERFACE ${MINHOOK_INCLUDE_DIR})
else()
    # Download pre-built minhook-detours for ARM64
    message(STATUS "Downloading minhook-detours pre-built binaries for ARM64")
    include(FetchContent)

    FetchContent_Declare(
        minhook-detours
        URL      https://github.com/m417z/minhook-detours/releases/download/v1.0.6/minhook-detours-1.0.6.zip
        URL_HASH SHA256=E719959D824511E27395A82AEDA994CAAD53A67EE5894BA5FC2F4BF1FA41E38E
    )
    FetchContent_MakeAvailable(minhook-detours)

    # Create imported library for the pre-built DLL
    set(_MINHOOK_DLL
        "${minhook-detours_SOURCE_DIR}/Release/minhook-detours.ARM64.Release.dll"
        CACHE INTERNAL "Path to minhook-detours DLL")
    add_library(minhook::minhook SHARED IMPORTED GLOBAL)
    set_property(TARGET minhook::minhook PROPERTY IMPORTED_LOCATION "${_MINHOOK_DLL}")
    set_property(TARGET minhook::minhook PROPERTY IMPORTED_IMPLIB
        "${minhook-detours_SOURCE_DIR}/Release/minhook-detours.ARM64.Release.lib")
    set_target_properties(minhook::minhook PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${minhook-detours_SOURCE_DIR}/src"
    )
endif()
