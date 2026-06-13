cmake_minimum_required(VERSION 3.20)

project(onitro
    VERSION 0.1.0
    DESCRIPTION "Optimized Numerical Infrastructure for Trading, Risk and Options"
    LANGUAGES CXX)

# ---- Options -------------------------------------------------------------
option(ONITRO_BUILD_TESTS      "Build C++ unit tests"  ON)
option(ONITRO_BUILD_BENCHMARKS "Build benchmarks"      OFF)
option(ONITRO_BUILD_EXAMPLES   "Build examples"        ON)
option(ONITRO_ENABLE_AVX2      "Enable AVX2 SIMD paths" ON)

# ---- Standard ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Export compile_commands.json for clangd / tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- The library ---------------------------------------------------------
# All module sources compile into a single static lib. Mentees decide how to
# break their module into files; as sources are written, list them in
# ONITRO_SOURCES below (grouped by module/owner). The `math/` primitive is the
# dependency root and the natural first file to exist.
set(ONITRO_SOURCES
    # --- math: shared primitive, depends on nothing (FIRST FILE TO WRITE) ---
    # src/math/normal.cpp
    #
    # --- Module 1: pricing core (Parrva) ---
    # src/pricing/...   src/greeks/...
    #
    # --- Module 2: implied vol (Parrva) ---
    # src/iv/...
    #
    # --- Module 3: instruments (Arnav) ---
    # src/instruments/...
    #
    # --- Module 4: monte carlo (Parrva) ---
    # src/montecarlo/...
    #
    # --- Module 5: risk (Arnav) ---
    # src/risk/...
    #
    # --- Module 6: stats (Arnav) ---
    # src/stats/...
)

# A STATIC lib needs at least one source. Until the first module file lands,
# the library target is skipped and the dependent subdirectories are too.
if(ONITRO_SOURCES)
    add_library(onitro STATIC ${ONITRO_SOURCES})
    target_include_directories(onitro PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_compile_features(onitro PUBLIC cxx_std_20)
    if(ONITRO_ENABLE_AVX2)
        if(MSVC)
            target_compile_options(onitro PRIVATE /arch:AVX2)
        else()
            target_compile_options(onitro PRIVATE -mavx2 -mfma)
        endif()
    endif()
    add_library(onitro::onitro ALIAS onitro)
    set(ONITRO_LIB_READY ON)
else()
    message(STATUS "ONITRO: no sources listed yet — library target skipped. "
                   "Add files to ONITRO_SOURCES in CMakeLists.txt to enable it.")
    set(ONITRO_LIB_READY OFF)
endif()

# ---- Subdirectories ------------------------------------------------------
# These only build once the library has at least one source (ONITRO_LIB_READY).
if(ONITRO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ONITRO_BUILD_EXAMPLES AND ONITRO_LIB_READY)
    add_subdirectory(examples)
endif()

if(ONITRO_BUILD_BENCHMARKS AND ONITRO_LIB_READY)
    add_subdirectory(benchmarks)
endif()
