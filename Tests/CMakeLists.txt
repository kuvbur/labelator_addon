cmake_minimum_required(VERSION 3.19)

project(LayoutOptimizerTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W3 /WX /EHsc /Zc:wchar_t- /wd4499 /wd4003 /wd4996)
else()
    add_compile_options(-Wall -Wextra -Werror -fvisibility=hidden)
endif()

include_directories(${CMAKE_SOURCE_DIR}/../Sources/AddOn)

add_executable(layout_optimizer_tests ../Sources/AddOn/LayoutOptimizerTests.cpp)

# Note: This test executable does not link against Archicad libraries.
# It only tests the pure C++17 core (LayoutOptimizer namespace).