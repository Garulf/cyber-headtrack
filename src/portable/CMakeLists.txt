include(FetchContent)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

add_library(portable STATIC
    head_pose.cpp
    adaptive_filter.cpp
    config.cpp
    body_visibility.cpp
)
target_include_directories(portable PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(portable PUBLIC cxx_std_20)
target_link_libraries(portable PUBLIC tomlplusplus::tomlplusplus)
