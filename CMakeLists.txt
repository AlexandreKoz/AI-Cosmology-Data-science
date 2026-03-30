cmake_minimum_required(VERSION 3.24)
project(CosmoSim VERSION 0.1.0 LANGUAGES CXX)

option(COSMOSIM_ENABLE_OPENMP "Enable OpenMP CPU parallel backend" ON)
option(COSMOSIM_ENABLE_TESTS "Build tests" ON)
option(COSMOSIM_ENABLE_PROVENANCE "Embed build/run provenance metadata" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(COSMOSIM_ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
endif()

add_library(cosmosim_core STATIC
  src/core/sim_state.cpp
  src/core/soa_layout.cpp
  src/runtime/scheduler_cpu.cpp
  src/utils/provenance.cpp
)

target_include_directories(cosmosim_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(cosmosim_core PUBLIC cxx_std_20)

if(COSMOSIM_ENABLE_OPENMP)
  target_link_libraries(cosmosim_core PUBLIC OpenMP::OpenMP_CXX)
  target_compile_definitions(cosmosim_core PUBLIC COSMOSIM_OPENMP_ENABLED=1)
endif()

if(COSMOSIM_ENABLE_PROVENANCE)
  target_compile_definitions(cosmosim_core PUBLIC COSMOSIM_PROVENANCE_ENABLED=1)
endif()

add_executable(cosmosim_driver src/main.cpp)
target_link_libraries(cosmosim_driver PRIVATE cosmosim_core)

if(COSMOSIM_ENABLE_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()
