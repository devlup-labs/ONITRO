cmake_minimum_required(VERSION 3.15)
project(OnitroQuantLibModule1 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QuantLib REQUIRED)
include_directories(${QuantLib_INCLUDE_DIRS})

add_executable(module_1_integrated_example
    module_1_integrated_example.cpp
    module_1_greeks_solver.cpp
)

target_link_libraries(module_1_integrated_example PRIVATE ${QuantLib_LIBRARIES})