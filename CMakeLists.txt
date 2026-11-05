cmake_minimum_required(VERSION 3.20)          # oldest CMake version this file supports
project(safety_monitor LANGUAGES C CXX)       # project name; C and C++

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)                    # use C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)           # fail if the compiler can't do C++20

find_package(OpenCV REQUIRED)                                   # after project(...)
find_package(Threads REQUIRED)

#--------------------------------------------
# safety_monitor
#--------------------------------------------
add_executable(safety_monitor src/main.cpp 
                              src/person_detector.cpp 
                              src/report.cpp)
target_include_directories(safety_monitor PRIVATE include third_party/vl53l1x_uld)
target_link_libraries(safety_monitor PRIVATE ${OpenCV_LIBS})
target_compile_options(safety_monitor PRIVATE -Wall -Wextra -Wconversion)

#--------------------------------------------
# test_frame_queue
#--------------------------------------------
add_executable(test_frame_queue tests/test_frame_queue.cpp)
target_include_directories(test_frame_queue PRIVATE include)
target_compile_options(test_frame_queue PRIVATE -Wall -Wextra -Wconversion)

#--------------------------------------------
# test_thread_safe_queue
#--------------------------------------------
add_executable(test_thread_safe_queue tests/test_thread_safe_queue.cpp)
target_include_directories(test_thread_safe_queue PRIVATE include)
target_link_libraries(test_thread_safe_queue PRIVATE Threads::Threads)
target_compile_options(test_thread_safe_queue PRIVATE -Wall -Wextra -Wconversion)

#--------------------------------------------
# hw_test_i2c_device
#--------------------------------------------
add_executable(hw_test_i2c_device tests/hw_test_i2c_device.cpp 
                                  src/i2c_device.cpp)
target_include_directories(hw_test_i2c_device PRIVATE include third_party/vl53l1x_uld)
target_compile_options(hw_test_i2c_device PRIVATE -Wall -Wextra -Wconversion)

#--------------------------------------------
# detect_image
#--------------------------------------------
add_executable(detect_image src/detect_image.cpp 
                            src/person_detector.cpp)
target_include_directories(detect_image PRIVATE include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(detect_image PRIVATE ${OpenCV_LIBS})
target_compile_options(detect_image PRIVATE -Wall -Wextra -Wconversion)

#--------------------------------------------
# sensor_check
#--------------------------------------------
add_executable(sensor_check src/sensor_check.cpp 
                            src/i2c_device.cpp)
target_include_directories(sensor_check PRIVATE include)
target_compile_options(sensor_check PRIVATE -Wall -Wextra -Wconversion)

#--------------------------------------------
# vl53l1x_uld    library
#--------------------------------------------
add_library(vl53l1x_uld STATIC third_party/vl53l1x_uld/VL53L1X_api.c
                               third_party/vl53l1x_uld/VL53L1X_calibration.c)
target_include_directories(vl53l1x_uld PUBLIC third_party/vl53l1x_uld)

#--------------------------------------------
# range_check
#--------------------------------------------
add_executable(range_check src/range_check.cpp 
                           src/vl53l1_platform.cpp 
                           src/i2c_device.cpp)
target_include_directories(range_check PRIVATE include)
target_link_libraries(range_check PRIVATE vl53l1x_uld)
target_compile_options(range_check PRIVATE -Wall -Wextra -Wconversion)