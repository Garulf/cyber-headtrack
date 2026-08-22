include(FetchContent)
FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_executable(portable_tests test_main.cpp test_head_pose.cpp test_adaptive_filter.cpp)
target_link_libraries(portable_tests PRIVATE portable Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(portable_tests)
