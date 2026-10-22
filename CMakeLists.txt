cmake_minimum_required(VERSION 3.20)          # oldest CMake version this file supports
project(safety_monitor LANGUAGES CXX)         # project name; C++ only

set(CMAKE_CXX_STANDARD 20)                    # use C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)           # fail if the compiler can't do C++20

add_executable(safety_monitor src/main.cpp)   # build a program called safety_monitor from main.cpp
target_compile_options(safety_monitor PRIVATE -Wall -Wextra -Wconversion)   # strict warnings, as before

find_package(OpenCV REQUIRED)                                   # after project(...)
# ...
target_link_libraries(safety_monitor PRIVATE ${OpenCV_LIBS})    # after add_executable(...)