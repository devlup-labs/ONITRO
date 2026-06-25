cmake_minimum_required(VERSION 3.10)
project(QuantLibModule3Examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(QUANTLIB_PKGCONFIG quantlib)

if(QUANTLIB_PKGCONFIG_FOUND)
    message(STATUS "Found QuantLib via pkg-config: ${QUANTLIB_PKGCONFIG_LIBRARIES}")
    set(QUANTLIB_INCLUDE_DIRS ${QUANTLIB_PKGCONFIG_INCLUDE_DIRS})
    set(QUANTLIB_LIBRARIES ${QUANTLIB_PKGCONFIG_LIBRARIES})
else()
    find_path(QUANTLIB_INCLUDE_DIR ql/quantlib.hpp /usr/include /usr/local/include)
    find_library(QUANTLIB_LIBRARY NAMES QuantLib libQuantLib PATHS /usr/lib /usr/local/lib /lib)
    if(NOT QUANTLIB_INCLUDE_DIR OR NOT QUANTLIB_LIBRARY)
        message(FATAL_ERROR "QuantLib not found. Set CMAKE_PREFIX_PATH or QuantLib_DIR, or install QuantLib development files.")
    endif()
    set(QUANTLIB_INCLUDE_DIRS ${QUANTLIB_INCLUDE_DIR})
    set(QUANTLIB_LIBRARIES ${QUANTLIB_LIBRARY})
endif()

# 1. FIXED: Added explicit STATIC declaration for clean localized object bundling
add_library(module3 STATIC
    module_3_forwards_futures.cpp
    module_3_bonds.cpp
    module_3_ytm_solver.cpp
)

target_compile_definitions(module3 PRIVATE MODULE3_LIBRARY_BUILD)

target_include_directories(module3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${QUANTLIB_INCLUDE_DIRS})

# 2. FIXED: Changed to PUBLIC link propagation so executables pick up the library symbols properly
target_link_libraries(module3 PUBLIC ${QUANTLIB_LIBRARIES})

# Target Execution Definitions
add_executable(quick_start_examples quick_start_examples.cpp)
target_link_libraries(quick_start_examples PRIVATE module3)

add_executable(module_3_integrated_example module_3_integrated_example.cpp)
target_link_libraries(module_3_integrated_example PRIVATE module3)