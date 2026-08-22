add_library(portable STATIC
    head_pose.cpp
    adaptive_filter.cpp
    config.cpp
)
target_include_directories(portable PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(portable PUBLIC cxx_std_20)
