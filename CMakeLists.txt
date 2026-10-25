cmake_minimum_required(VERSION 3.20)          # oldest CMake version this file supports
project(safety_monitor LANGUAGES CXX)         # project name; C++ only

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)                    # use C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)           # fail if the compiler can't do C++20

find_package(OpenCV REQUIRED)                                   # after project(...)
find_package(Threads REQUIRED)

add_executable(safety_monitor src/main.cpp src/person_detector.cpp)
target_include_directories(safety_monitor PRIVATE include)
target_link_libraries(safety_monitor PRIVATE ${OpenCV_LIBS})    # after add_executable(...)
target_compile_options(safety_monitor PRIVATE -Wall -Wextra -Wconversion)   # strict warnings

add_executable(test_frame_queue tests/test_frame_queue.cpp)
target_include_directories(test_frame_queue PRIVATE include)

add_executable(test_thread_safe_queue tests/test_thread_safe_queue.cpp)
target_include_directories(test_thread_safe_queue PRIVATE include)
target_link_libraries(test_thread_safe_queue PRIVATE Threads::Threads)

add_executable(detect_image src/detect_image.cpp src/person_detector.cpp)
target_include_directories(detect_image PRIVATE include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(detect_image PRIVATE ${OpenCV_LIBS})
target_compile_options(detect_image PRIVATE -Wall -Wextra -Wconversion)