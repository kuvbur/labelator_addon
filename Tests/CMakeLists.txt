cmake_minimum_required(VERSION 3.19)

project(LayoutOptimizerTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W3 /WX /EHsc /D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

add_executable(layout_optimizer_tests
    Sources/AddOn/LayoutOptimizerTests.cpp
    Sources/AddOn/LayoutOptimizer.hpp
    Sources/AddOn/CandidateGenerator.hpp
    Sources/AddOn/CandidateGenerator.cpp
    Sources/AddOn/GreedySolver.cpp
)

target_compile_features(layout_optimizer_tests PRIVATE cxx_std_17)