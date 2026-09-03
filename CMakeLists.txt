cmake_minimum_required(VERSION 3.10)

project(calicam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)

add_executable(calicam calicam.cpp)

target_include_directories(calicam PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(calicam PRIVATE ${OpenCV_LIBS})

if(WIN32)
  target_link_libraries(calicam PRIVATE comdlg32)
endif()
