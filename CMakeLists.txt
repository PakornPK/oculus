cmake_minimum_required(VERSION 3.15)
project(Oculus CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(oculus apps/oculus-sensor/main.cpp)