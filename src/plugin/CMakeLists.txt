include(FetchContent)
FetchContent_Declare(
    red4ext_sdk
    GIT_REPOSITORY https://github.com/WopsS/RED4ext.SDK.git
    GIT_TAG master
)
FetchContent_MakeAvailable(red4ext_sdk)

# Path to an extracted Tobii Stream Engine SDK (the gated download from
# https://developer.tobii.com/product-integration/stream-engine/ or the
# Tobii.StreamEngine.Native NuGet package), laid out as include/tobii/*.h plus the
# import library. Required because Tobii does not permit redistributing the SDK here.
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

add_library(CyberHeadtrack SHARED main.cpp tobii_source.cpp tracking_state.cpp)
target_include_directories(CyberHeadtrack PRIVATE "${CHT_TOBII_SDK_DIR}/include")
target_link_libraries(CyberHeadtrack PRIVATE portable RED4ext.SDK "${CHT_TOBII_STREAM_ENGINE_LIB}")
target_compile_features(CyberHeadtrack PRIVATE cxx_std_20)
target_compile_definitions(CyberHeadtrack PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

set_target_properties(CyberHeadtrack PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/red4ext/plugins/CyberHeadtrack"
)

if(MSVC)
    target_compile_options(CyberHeadtrack PRIVATE /W4 /permissive- /EHsc)
endif()

# If configuration fails with "target not found" here, the SDK may export the
# target as RED4ext::SDK instead of RED4ext.SDK; confirm with
# `cmake --build build --target help` on Windows and adjust the link line.
