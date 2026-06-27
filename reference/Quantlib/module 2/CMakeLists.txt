cmake_minimum_required(VERSION 3.20)

project(ImpliedVolatility)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(QuantLib_DIR "C:/Users/parth/OneDrive/Desktop/vcpkg/installed/x64-windows-static/share/QuantLib")
find_package(QuantLib CONFIG REQUIRED)

add_executable(impliedvolatility
    src/impliedvolatility.cpp
)

target_link_libraries(impliedvolatility PRIVATE QuantLib::QuantLib)