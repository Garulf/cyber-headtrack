include(FetchContent)
FetchContent_Declare(
    red4ext_sdk
    GIT_REPOSITORY https://github.com/WopsS/RED4ext.SDK.git
    GIT_TAG master
)
FetchContent_MakeAvailable(red4ext_sdk)

add_library(CyberHeadtrack SHARED main.cpp)
target_link_libraries(CyberHeadtrack PRIVATE portable RED4ext.SDK)
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
