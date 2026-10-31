include(FetchContent)
FetchContent_Declare(
    red4ext_sdk
    GIT_REPOSITORY https://github.com/WopsS/RED4ext.SDK.git
    GIT_TAG 5246e659d783e5834cc08a90e56c8932c9ed3449
)
FetchContent_MakeAvailable(red4ext_sdk)

# Path to an extracted Tobii Stream Engine SDK (the gated download from
# https://developer.tobii.com/product-integration/stream-engine/ or the
# Tobii.StreamEngine.Native NuGet package), laid out as include/tobii/*.h plus the
# import library and runtime DLL. The SDK is not vendored in this repo; its runtime
# tobii_stream_engine.dll is copied next to the plugin and shipped in the package, which
# the SDK license permits (see README.md).
set(CHT_TOBII_SDK_DIR "" CACHE PATH "Root of the Tobii Stream Engine SDK (contains include/ and lib dirs)")
if(NOT CHT_TOBII_SDK_DIR OR NOT EXISTS "${CHT_TOBII_SDK_DIR}/include/tobii/tobii.h")
    message(FATAL_ERROR
        "CHT_TOBII_SDK_DIR must point at an extracted Tobii Stream Engine SDK "
        "containing include/tobii/tobii.h (Tobii Developer Zone account or the "
        "Tobii.StreamEngine.Native NuGet package required; see README.md).")
endif()

find_library(CHT_TOBII_STREAM_ENGINE_LIB
    NAMES tobii_stream_engine
    PATHS "${CHT_TOBII_SDK_DIR}" "${CHT_TOBII_SDK_DIR}/lib"
    PATH_SUFFIXES x64 lib/x64
    REQUIRED
)

find_file(CHT_TOBII_STREAM_ENGINE_DLL
    NAMES tobii_stream_engine.dll
    PATHS "${CHT_TOBII_SDK_DIR}" "${CHT_TOBII_SDK_DIR}/lib"
    PATH_SUFFIXES x64 lib/x64
    NO_DEFAULT_PATH
    REQUIRED
)

add_library(CyberHeadtrack SHARED main.cpp aim_hook.cpp code_patch.cpp scan_hook.cpp tobii_source.cpp tracking_state.cpp)
target_include_directories(CyberHeadtrack PRIVATE "${CHT_TOBII_SDK_DIR}/include")
target_link_libraries(CyberHeadtrack PRIVATE portable RED4ext.SDK "${CHT_TOBII_STREAM_ENGINE_LIB}")
target_compile_features(CyberHeadtrack PRIVATE cxx_std_20)
target_compile_definitions(CyberHeadtrack PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
    CHT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CHT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CHT_VERSION_PATCH=${PROJECT_VERSION_PATCH})

set_target_properties(CyberHeadtrack PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/red4ext/plugins/CyberHeadtrack/$<0:>"
)

if(MSVC)
    target_compile_options(CyberHeadtrack PRIVATE /W4 /permissive- /EHsc)
endif()

add_custom_command(TARGET CyberHeadtrack POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CHT_TOBII_STREAM_ENGINE_DLL}"
            "$<TARGET_FILE_DIR:CyberHeadtrack>")

set(CHT_PLUGIN_DIR red4ext/plugins/CyberHeadtrack)
install(TARGETS CyberHeadtrack RUNTIME DESTINATION ${CHT_PLUGIN_DIR} COMPONENT mod)
# config.toml is not packaged: the plugin writes a default one on first launch, so
# extracting a newer package over an existing install keeps the user's settings.
install(FILES "${CHT_TOBII_STREAM_ENGINE_DLL}" DESTINATION ${CHT_PLUGIN_DIR} COMPONENT mod)
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION ${CHT_PLUGIN_DIR} RENAME LICENSE.txt COMPONENT mod)
# NuGet layout: the SDK license sits in content/Tobii, two levels above build/native.
set(CHT_TOBII_LICENSE "${CHT_TOBII_SDK_DIR}/../../content/Tobii/SDK-License_en.pdf")
if(EXISTS "${CHT_TOBII_LICENSE}")
    install(FILES "${CHT_TOBII_LICENSE}"
        DESTINATION ${CHT_PLUGIN_DIR} RENAME tobii_stream_engine_LICENSE.pdf COMPONENT mod)
endif()
install(DIRECTORY "${PROJECT_SOURCE_DIR}/r6/scripts/CyberHeadtrack"
    DESTINATION r6/scripts COMPONENT mod)
install(FILES "${PROJECT_SOURCE_DIR}/r6/input/CyberHeadtrack.xml"
    DESTINATION r6/input COMPONENT mod)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/bin/x64/plugins/cyber_engine_tweaks/mods/CyberHeadtrack"
    DESTINATION bin/x64/plugins/cyber_engine_tweaks/mods COMPONENT mod)

# If configuration fails with "target not found" here, the SDK may export the
# target as RED4ext::SDK instead of RED4ext.SDK; confirm with
# `cmake --build build --target help` on Windows and adjust the link line.
