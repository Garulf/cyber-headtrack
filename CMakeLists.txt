cmake_minimum_required(VERSION 3.24)
project(CyberHeadtrack VERSION 0.1.0 LANGUAGES CXX) # x-release-please-version

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CHT_BUILD_PLUGIN "Build the Windows-only RED4ext plugin target" OFF)

add_subdirectory(src/portable)

enable_testing()
add_subdirectory(tests)

if(CHT_BUILD_PLUGIN)
    add_subdirectory(src/plugin)

    # `cpack -C Release` in the build directory produces CyberHeadtrack-<version>.zip, laid
    # out like the game's install folder. Only the "mod" component is packaged, which keeps
    # files installed by fetched dependencies out of the archive.
    set(CPACK_GENERATOR ZIP)
    set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_ALL mod)
    set(CPACK_ARCHIVE_MOD_FILE_NAME "CyberHeadtrack-${PROJECT_VERSION}")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    set(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    include(CPack)
endif()
