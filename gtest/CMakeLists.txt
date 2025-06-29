cmake_minimum_required(VERSION 3.16)
project(bestex_tests)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent module
include(FetchContent)

# Fetch Google Test
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0  # Use a stable release tag
)

# Configure Google Test
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
set(BUILD_GTEST ON CACHE BOOL "" FORCE)

# Make Google Test available
FetchContent_MakeAvailable(googletest)

# Define the parent directory path
set(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Create the test executable
add_executable(mmf_tests
        mmf_test.cpp
)

# Set include directories for the target
target_include_directories(mmf_tests PRIVATE
        ${PARENT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against Google Test libraries
target_link_libraries(mmf_tests
        gtest
        gtest_main
        gmock
        gmock_main
)

# Add compiler flags
target_compile_options(mmf_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -g
)

# Enable testing
enable_testing()

# Add the test
add_test(NAME MMFTests COMMAND mmf_tests)

# Set test properties
set_tests_properties(MMFTests PROPERTIES
        TIMEOUT 60
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Add custom target to run tests
add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --verbose
        DEPENDS mmf_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Optionally, add a target to run tests with more detailed output
add_custom_target(run_tests_verbose
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
        DEPENDS mmf_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Add a target to run specific test patterns
add_custom_target(run_specific_test
        COMMAND ./mmf_tests --gtest_filter="*Constructor*"
        DEPENDS mmf_tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Run constructor tests only"
)