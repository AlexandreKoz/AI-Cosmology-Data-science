cmake_minimum_required(VERSION 3.24)

project(
  cosmosim
  VERSION 0.1.0
  DESCRIPTION "CosmoSim: desktop-first cosmological simulation framework skeleton"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cosmosim_feature_summary)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(COSMOSIM_ENABLE_MPI "Enable MPI backend" OFF)
option(COSMOSIM_ENABLE_HDF5 "Enable HDF5 I/O backend" OFF)
option(COSMOSIM_ENABLE_FFTW "Enable FFTW backend" OFF)
option(COSMOSIM_ENABLE_CUDA "Enable CUDA/cuFFT backend" OFF)
option(COSMOSIM_ENABLE_PYTHON "Enable Python tooling integration" OFF)
option(COSMOSIM_ENABLE_TESTS "Build unit and integration tests" ON)
option(COSMOSIM_ENABLE_BENCHMARKS "Build benchmark/profiling hooks" ON)
option(COSMOSIM_ENABLE_LTO "Enable interprocedural optimization (LTO)" OFF)

# Backward-compatibility aliases for earlier build toggles.
if(DEFINED COSMOSIM_BUILD_TESTS)
  set(COSMOSIM_ENABLE_TESTS
      "${COSMOSIM_BUILD_TESTS}"
      CACHE BOOL "Build unit and integration tests" FORCE)
endif()

if(DEFINED COSMOSIM_BUILD_BENCHMARKS)
  set(COSMOSIM_ENABLE_BENCHMARKS
      "${COSMOSIM_BUILD_BENCHMARKS}"
      CACHE BOOL "Build benchmark/profiling hooks" FORCE)
endif()

if(COSMOSIM_ENABLE_CUDA)
  enable_language(CUDA)
endif()

if(COSMOSIM_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT cosmosim_lto_supported OUTPUT cosmosim_lto_error)
  if(NOT cosmosim_lto_supported)
    message(FATAL_ERROR "COSMOSIM_ENABLE_LTO=ON requested but IPO/LTO is not supported: ${cosmosim_lto_error}")
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "")
  message(STATUS "No CMAKE_BUILD_TYPE selected; use presets for explicit Debug/Release configuration.")
endif()

# Dependencies gated by explicit feature options.
if(COSMOSIM_ENABLE_MPI)
  find_package(MPI REQUIRED COMPONENTS CXX)
endif()

if(COSMOSIM_ENABLE_HDF5)
  find_package(HDF5 REQUIRED COMPONENTS C CXX)
endif()

if(COSMOSIM_ENABLE_FFTW)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)
endif()

if(COSMOSIM_ENABLE_CUDA)
  find_package(CUDAToolkit REQUIRED)
endif()

if(COSMOSIM_ENABLE_PYTHON)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()

# Build metadata and generated configuration header.
set(COSMOSIM_SELECTED_PRESET "${COSMOSIM_PRESET_NAME}")
if(COSMOSIM_SELECTED_PRESET STREQUAL "")
  if(DEFINED ENV{CMAKE_PRESET_NAME})
    set(COSMOSIM_SELECTED_PRESET "$ENV{CMAKE_PRESET_NAME}")
  else()
    set(COSMOSIM_SELECTED_PRESET "manual")
  endif()
endif()

set(COSMOSIM_GENERATED_INCLUDE_DIR "${CMAKE_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${COSMOSIM_GENERATED_INCLUDE_DIR}/cosmosim/core")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/build_config.hpp.in"
  "${COSMOSIM_GENERATED_INCLUDE_DIR}/cosmosim/core/build_config.hpp"
  @ONLY)

add_library(cosmosim_core STATIC
  src/core/version.cpp
  src/core/module_registry.cpp
  src/core/config.cpp
  src/core/provenance.cpp
  src/core/simulation_state.cpp
  src/core/soa_storage.cpp)

target_include_directories(cosmosim_core
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${COSMOSIM_GENERATED_INCLUDE_DIR}
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src)

target_compile_features(cosmosim_core PUBLIC cxx_std_20)

target_compile_definitions(cosmosim_core
  PUBLIC
    COSMOSIM_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    COSMOSIM_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    COSMOSIM_VERSION_PATCH=${PROJECT_VERSION_PATCH})

if(COSMOSIM_ENABLE_LTO)
  set_property(TARGET cosmosim_core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

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
      ${COSMOSIM_GENERATED_INCLUDE_DIR}
    PRIVATE
      ${PROJECT_SOURCE_DIR}/src)
  target_link_libraries(${module_lib} PUBLIC cosmosim_core)
  target_compile_features(${module_lib} PUBLIC cxx_std_20)

  if(COSMOSIM_ENABLE_LTO)
    set_property(TARGET ${module_lib} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endforeach()

if(COSMOSIM_ENABLE_MPI)
  target_link_libraries(cosmosim_parallel PUBLIC MPI::MPI_CXX)
endif()

if(COSMOSIM_ENABLE_HDF5)
  if(TARGET HDF5::HDF5)
    target_link_libraries(cosmosim_io PUBLIC HDF5::HDF5)
  else()
    target_include_directories(cosmosim_io PUBLIC ${HDF5_INCLUDE_DIRS})
    target_link_libraries(cosmosim_io PUBLIC ${HDF5_LIBRARIES})
  endif()
endif()

if(COSMOSIM_ENABLE_FFTW)
  target_link_libraries(cosmosim_gravity PUBLIC PkgConfig::FFTW)
endif()

if(COSMOSIM_ENABLE_CUDA)
  target_link_libraries(cosmosim_gravity PUBLIC CUDA::cufft)
endif()

add_library(cosmosim_all INTERFACE)
target_link_libraries(cosmosim_all
  INTERFACE
    cosmosim_core
    ${COSMOSIM_MODULE_LIBS})

add_executable(cosmosim_harness src/core/harness_main.cpp)
target_link_libraries(cosmosim_harness PRIVATE cosmosim_all)

if(COSMOSIM_ENABLE_TESTS)
  enable_testing()

  add_executable(test_unit_header_smoke tests/unit/test_header_smoke.cpp)
  target_link_libraries(test_unit_header_smoke PRIVATE cosmosim_all)
  add_test(NAME unit_header_smoke COMMAND test_unit_header_smoke)

  add_executable(test_integration_harness tests/integration/test_integration_harness.cpp)
  target_link_libraries(test_integration_harness PRIVATE cosmosim_all)
  add_test(NAME integration_harness COMMAND test_integration_harness)

  add_executable(test_unit_simulation_state tests/unit/test_simulation_state.cpp)
  target_link_libraries(test_unit_simulation_state PRIVATE cosmosim_all)
  add_test(NAME unit_simulation_state COMMAND test_unit_simulation_state)

  add_executable(test_unit_config_parser tests/unit/test_config_parser.cpp)
  target_link_libraries(test_unit_config_parser PRIVATE cosmosim_all)
  add_test(NAME unit_config_parser COMMAND test_unit_config_parser)

  add_executable(test_unit_soa_storage tests/unit/test_soa_storage.cpp)
  target_link_libraries(test_unit_soa_storage PRIVATE cosmosim_core)
  add_test(NAME unit_soa_storage COMMAND test_unit_soa_storage)

  add_executable(test_unit_species_state_organization tests/unit/test_species_state_organization.cpp)
  target_link_libraries(test_unit_species_state_organization PRIVATE cosmosim_core)
  add_test(NAME unit_species_state_organization COMMAND test_unit_species_state_organization)

  add_executable(test_integration_simulation_state_pipeline tests/integration/test_simulation_state_pipeline.cpp)
  target_link_libraries(test_integration_simulation_state_pipeline PRIVATE cosmosim_all)
  add_test(NAME integration_simulation_state_pipeline COMMAND test_integration_simulation_state_pipeline)


  add_executable(test_integration_soa_species_pipeline tests/integration/test_soa_species_pipeline.cpp)
  target_link_libraries(test_integration_soa_species_pipeline PRIVATE cosmosim_core)
  add_test(NAME integration_soa_species_pipeline COMMAND test_integration_soa_species_pipeline)

  add_executable(test_integration_species_mixed_state tests/integration/test_species_mixed_state.cpp)
  target_link_libraries(test_integration_species_mixed_state PRIVATE cosmosim_core)
  add_test(NAME integration_species_mixed_state COMMAND test_integration_species_mixed_state)

  add_executable(test_integration_config_examples tests/integration/test_config_examples.cpp)
  target_link_libraries(test_integration_config_examples PRIVATE cosmosim_all)
  target_compile_definitions(test_integration_config_examples PRIVATE COSMOSIM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME integration_config_examples COMMAND test_integration_config_examples)

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_feature_summary.cmake.in"
    "${CMAKE_BINARY_DIR}/test_feature_summary.cmake"
    @ONLY)
  add_test(NAME integration_feature_summary
           COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/test_feature_summary.cmake")

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_configure_presets.cmake.in"
    "${CMAKE_BINARY_DIR}/test_configure_presets.cmake"
    @ONLY)
  add_test(NAME integration_configure_presets
           COMMAND ${CMAKE_COMMAND} -P "${CMAKE_BINARY_DIR}/test_configure_presets.cmake")
endif()

if(COSMOSIM_ENABLE_BENCHMARKS)
  add_executable(bench_layout_smoke bench/bench_layout_smoke.cpp)
  target_link_libraries(bench_layout_smoke PRIVATE cosmosim_core)
  target_include_directories(bench_layout_smoke PRIVATE ${PROJECT_SOURCE_DIR}/src)
  target_compile_definitions(bench_layout_smoke PRIVATE COSMOSIM_BENCHMARK_BUILD=1)

  add_executable(bench_config_parser bench/bench_config_parser.cpp)
  target_link_libraries(bench_config_parser PRIVATE cosmosim_core)

  add_executable(bench_simulation_state_resize bench/bench_simulation_state_resize.cpp)
  target_link_libraries(bench_simulation_state_resize PRIVATE cosmosim_core)

  add_executable(bench_soa_storage bench/bench_soa_storage.cpp)
  target_link_libraries(bench_soa_storage PRIVATE cosmosim_core)

  add_executable(bench_species_hot_cold_access bench/bench_species_hot_cold_access.cpp)
  target_link_libraries(bench_species_hot_cold_access PRIVATE cosmosim_core)
endif()

cosmosim_generate_feature_summary(
  OUTPUT_TEXT "${CMAKE_BINARY_DIR}/cosmosim_feature_summary.txt"
  OUTPUT_JSON "${CMAKE_BINARY_DIR}/cosmosim_build_metadata.json"
  TARGETS
    cosmosim_core
    ${COSMOSIM_MODULE_LIBS}
    cosmosim_harness)

message(STATUS "\nCosmoSim feature summary\n-----------------------")
file(STRINGS "${CMAKE_BINARY_DIR}/cosmosim_feature_summary.txt" cosmosim_summary_lines)
foreach(summary_line IN LISTS cosmosim_summary_lines)
  message(STATUS "${summary_line}")
endforeach()
message(STATUS "Build metadata JSON: ${CMAKE_BINARY_DIR}/cosmosim_build_metadata.json")
