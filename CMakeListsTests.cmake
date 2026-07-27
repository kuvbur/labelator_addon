cmake_minimum_required(VERSION 3.19)
project(LayoutOptimizerTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Test executable - pure C++17, no Archicad dependencies
add_executable(LayoutOptimizerTests
    Sources/AddOn/LayoutOptimizerTests.cpp
    Sources/AddOn/LayoutOptimizer.hpp
    Sources/AddOn/CandidateGenerator.hpp
    Sources/AddOn/CandidateGenerator.cpp
    Sources/AddOn/GreedySolver.cpp
)

target_compile_features(LayoutOptimizerTests PUBLIC cxx_std_17)
target_compile_options(LayoutOptimizerTests PRIVATE /W3 /WX /EHsc /D_CRT_SECURE_NO_WARNINGS)

# Copy test CSV files to build directory for runtime access
file(COPY Test_file/debug_labels_before.csv DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Test_file)
file(COPY Test_file/debug_labels_after.csv DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Test_file)

# Add test
add_test(NAME LayoutOptimizerTests COMMAND LayoutOptimizerTests)