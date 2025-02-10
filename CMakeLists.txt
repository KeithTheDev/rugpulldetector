# Root CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(rug_pull_detector VERSION 1.0)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)  # Add this line to enable PIC globally

# Build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -march=native  # Enable CPU-specific optimizations
        -fPIC         # Add PIC flag explicitly
    )
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Python 3.8 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# Use pkg-config to find hiredis
pkg_check_modules(HIREDIS REQUIRED hiredis)

# Check for redis++
pkg_check_modules(REDIS_PLUS_PLUS REQUIRED redis++)

# Find other dependencies
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${HIREDIS_INCLUDE_DIRS}
    ${REDIS_PLUS_PLUS_INCLUDE_DIRS}
)

# Link directories
link_directories(
    ${HIREDIS_LIBRARY_DIRS}
    ${REDIS_PLUS_PLUS_LIBRARY_DIRS}
)

# Create library from core files
set(LIB_SOURCES
    src/rug_pull_detector.cpp
    src/redis_client.cpp
)

# Add library with position independent code
add_library(rugpull_core STATIC ${LIB_SOURCES})
set_target_properties(rugpull_core PROPERTIES 
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(rugpull_core
    PUBLIC              # Changed from PRIVATE to PUBLIC
    Threads::Threads
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Main executable
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    rugpull_core
)

# Python module
pybind11_add_module(pyrug_detector src/python_bindings.cpp)
target_link_libraries(pyrug_detector
    PRIVATE
    rugpull_core
)
set_target_properties(pyrug_detector PROPERTIES
    OUTPUT_NAME "rugpull_detector"
)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(TARGETS pyrug_detector
    LIBRARY DESTINATION ${Python_SITEARCH}
)

# Add tests subdirectory if it exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    enable_testing()
    add_subdirectory(tests)
endif()
