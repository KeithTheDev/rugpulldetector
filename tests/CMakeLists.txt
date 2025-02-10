# tests/CMakeLists.txt
find_package(GTest REQUIRED)
include(GoogleTest)

# Add test executable
add_executable(run_tests
    test_rug_pull_detector.cpp
)

# Link test dependencies
target_link_libraries(run_tests
    PRIVATE
    GTest::GTest
    GTest::Main
    Threads::Threads
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Discover tests
gtest_discover_tests(run_tests)
