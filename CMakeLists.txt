cmake_minimum_required(VERSION 3.24)

project(
  cosmosim
  VERSION 0.1.0
  DESCRIPTION "CosmoSim: desktop-first cosmological simulation framework skeleton"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COSMOSIM_ENABLE_MPI "Enable MPI backend (placeholder)" OFF)
option(COSMOSIM_ENABLE_OPENMP "Enable OpenMP backend (placeholder)" OFF)
option(COSMOSIM_ENABLE_CUDA "Enable CUDA backend (placeholder)" OFF)
option(COSMOSIM_ENABLE_HDF5 "Enable HDF5 I/O backend (placeholder)" OFF)
option(COSMOSIM_ENABLE_FFTW "Enable FFTW backend (placeholder)" OFF)
option(COSMOSIM_BUILD_TESTS "Build unit and integration tests" ON)
option(COSMOSIM_BUILD_BENCHMARKS "Build benchmark hooks" ON)

if(COSMOSIM_ENABLE_MPI)
  find_package(MPI REQUIRED)
endif()

if(COSMOSIM_ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
endif()

if(COSMOSIM_ENABLE_CUDA)
  enable_language(CUDA)
endif()

if(COSMOSIM_ENABLE_HDF5)
  find_package(HDF5 REQUIRED COMPONENTS C CXX)
endif()

if(COSMOSIM_ENABLE_FFTW)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)
endif()

add_library(cosmosim_core STATIC
  src/core/version.cpp
  src/core/module_registry.cpp
)

target_include_directories(cosmosim_core
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src)

target_compile_features(cosmosim_core PUBLIC cxx_std_20)

target_compile_definitions(cosmosim_core
  PUBLIC
    COSMOSIM_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    COSMOSIM_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    COSMOSIM_VERSION_PATCH=${PROJECT_VERSION_PATCH})

add_library(cosmosim_gravity STATIC src/gravity/gravity_module.cpp)
add_library(cosmosim_hydro STATIC src/hydro/hydro_module.cpp)
add_library(cosmosim_amr STATIC src/amr/amr_module.cpp)
add_library(cosmosim_physics STATIC src/physics/physics_module.cpp)
add_library(cosmosim_io STATIC src/io/io_module.cpp)
add_library(cosmosim_analysis STATIC src/analysis/analysis_module.cpp)
add_library(cosmosim_parallel STATIC src/parallel/parallel_module.cpp)
add_library(cosmosim_utils STATIC src/utils/utils_module.cpp)

set(COSMOSIM_MODULE_LIBS
  cosmosim_gravity
  cosmosim_hydro
  cosmosim_amr
  cosmosim_physics
  cosmosim_io
  cosmosim_analysis
  cosmosim_parallel
  cosmosim_utils)

foreach(module_lib IN LISTS COSMOSIM_MODULE_LIBS)
  target_include_directories(${module_lib}
    PUBLIC
      ${PROJECT_SOURCE_DIR}/include
    PRIVATE
      ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(${module_lib} PUBLIC cosmosim_core)
  target_compile_features(${module_lib} PUBLIC cxx_std_20)
endforeach()

add_library(cosmosim_all INTERFACE)
target_link_libraries(cosmosim_all
  INTERFACE
    cosmosim_core
    ${COSMOSIM_MODULE_LIBS})

add_executable(cosmosim_harness src/core/harness_main.cpp)
target_link_libraries(cosmosim_harness PRIVATE cosmosim_all)

if(COSMOSIM_ENABLE_MPI)
  target_link_libraries(cosmosim_parallel PUBLIC MPI::MPI_CXX)
endif()

if(COSMOSIM_ENABLE_OPENMP)
  target_link_libraries(cosmosim_parallel PUBLIC OpenMP::OpenMP_CXX)
endif()

if(COSMOSIM_ENABLE_HDF5)
  target_link_libraries(cosmosim_io PUBLIC HDF5::HDF5)
endif()

if(COSMOSIM_ENABLE_FFTW)
  target_link_libraries(cosmosim_gravity PUBLIC PkgConfig::FFTW)
endif()

if(COSMOSIM_BUILD_TESTS)
  enable_testing()

  add_executable(test_unit_header_smoke tests/unit/test_header_smoke.cpp)
  target_link_libraries(test_unit_header_smoke PRIVATE cosmosim_all)
  add_test(NAME unit_header_smoke COMMAND test_unit_header_smoke)

  add_executable(test_integration_harness tests/integration/test_integration_harness.cpp)
  target_link_libraries(test_integration_harness PRIVATE cosmosim_all)
  add_test(NAME integration_harness COMMAND test_integration_harness)
endif()

if(COSMOSIM_BUILD_BENCHMARKS)
  add_executable(bench_layout_smoke bench/bench_layout_smoke.cpp)
  target_link_libraries(bench_layout_smoke PRIVATE cosmosim_core)
  target_include_directories(bench_layout_smoke PRIVATE ${PROJECT_SOURCE_DIR}/src)
endif()
