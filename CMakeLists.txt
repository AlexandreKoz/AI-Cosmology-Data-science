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
  src/core/cosmology.cpp
  src/core/units.cpp
  src/core/simulation_state.cpp
  src/core/soa_storage.cpp
  src/core/simulation_mode.cpp
  src/core/time_integration.cpp)

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

add_library(cosmosim_gravity STATIC
  src/gravity/gravity_module.cpp
  src/gravity/pm_solver.cpp
  src/gravity/tree_ordering.cpp
  src/gravity/tree_gravity.cpp
  src/gravity/tree_pm_coupling.cpp)
add_library(cosmosim_hydro STATIC
  src/hydro/hydro_module.cpp
  src/hydro/hydro_core_solver.cpp)
add_library(cosmosim_amr STATIC src/amr/amr_module.cpp)
add_library(cosmosim_physics STATIC src/physics/physics_module.cpp)
add_library(cosmosim_io STATIC
  src/io/io_module.cpp
  src/io/ic_reader.cpp
  src/io/snapshot_hdf5.cpp
  src/io/restart_checkpoint.cpp)
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


  add_executable(test_unit_time_integration tests/unit/test_time_integration.cpp)
  target_link_libraries(test_unit_time_integration PRIVATE cosmosim_core)
  add_test(NAME unit_time_integration COMMAND test_unit_time_integration)

  add_executable(test_unit_simulation_mode tests/unit/test_simulation_mode.cpp)
  target_link_libraries(test_unit_simulation_mode PRIVATE cosmosim_core)
  add_test(NAME unit_simulation_mode COMMAND test_unit_simulation_mode)
  add_executable(test_unit_species_state_organization tests/unit/test_species_state_organization.cpp)
  target_link_libraries(test_unit_species_state_organization PRIVATE cosmosim_core)
  add_test(NAME unit_species_state_organization COMMAND test_unit_species_state_organization)

  add_executable(test_unit_hot_cold_sidecar_layout tests/unit/test_hot_cold_sidecar_layout.cpp)
  target_link_libraries(test_unit_hot_cold_sidecar_layout PRIVATE cosmosim_core)
  add_test(NAME unit_hot_cold_sidecar_layout COMMAND test_unit_hot_cold_sidecar_layout)

  add_executable(test_integration_simulation_state_pipeline tests/integration/test_simulation_state_pipeline.cpp)
  target_link_libraries(test_integration_simulation_state_pipeline PRIVATE cosmosim_all)
  add_test(NAME integration_simulation_state_pipeline COMMAND test_integration_simulation_state_pipeline)


  add_executable(test_integration_soa_species_pipeline tests/integration/test_soa_species_pipeline.cpp)
  target_link_libraries(test_integration_soa_species_pipeline PRIVATE cosmosim_core)
  add_test(NAME integration_soa_species_pipeline COMMAND test_integration_soa_species_pipeline)

  add_executable(test_integration_species_mixed_state tests/integration/test_species_mixed_state.cpp)
  target_link_libraries(test_integration_species_mixed_state PRIVATE cosmosim_core)
  add_test(NAME integration_species_mixed_state COMMAND test_integration_species_mixed_state)

  add_executable(test_integration_reorder_compaction_sidecars tests/integration/test_reorder_compaction_sidecars.cpp)
  target_link_libraries(test_integration_reorder_compaction_sidecars PRIVATE cosmosim_core)
  add_test(NAME integration_reorder_compaction_sidecars COMMAND test_integration_reorder_compaction_sidecars)


  add_executable(test_integration_time_integration_loop tests/integration/test_time_integration_loop.cpp)
  target_link_libraries(test_integration_time_integration_loop PRIVATE cosmosim_all)
  add_test(NAME integration_time_integration_loop COMMAND test_integration_time_integration_loop)

  add_executable(test_integration_hierarchical_time_bins tests/integration/test_hierarchical_time_bins.cpp)
  target_link_libraries(test_integration_hierarchical_time_bins PRIVATE cosmosim_core)
  add_test(NAME integration_hierarchical_time_bins COMMAND test_integration_hierarchical_time_bins)

  add_executable(test_integration_hierarchical_timestep_regression tests/integration/test_hierarchical_timestep_regression.cpp)
  target_link_libraries(test_integration_hierarchical_timestep_regression PRIVATE cosmosim_core)
  add_test(NAME integration_hierarchical_timestep_regression COMMAND test_integration_hierarchical_timestep_regression)


  add_executable(test_unit_pm_solver tests/unit/test_pm_solver.cpp)
  target_link_libraries(test_unit_pm_solver PRIVATE cosmosim_gravity)
  add_test(NAME unit_pm_solver COMMAND test_unit_pm_solver)

  add_executable(test_unit_tree_gravity tests/unit/test_tree_gravity.cpp)
  target_link_libraries(test_unit_tree_gravity PRIVATE cosmosim_gravity)
  add_test(NAME unit_tree_gravity COMMAND test_unit_tree_gravity)

  add_executable(test_unit_hydro_core_solver tests/unit/test_hydro_core_solver.cpp)
  target_link_libraries(test_unit_hydro_core_solver PRIVATE cosmosim_hydro)
  add_test(NAME unit_hydro_core_solver COMMAND test_unit_hydro_core_solver)

  add_executable(test_integration_hydro_sod_like tests/integration/test_hydro_sod_like.cpp)
  target_link_libraries(test_integration_hydro_sod_like PRIVATE cosmosim_hydro)
  add_test(NAME integration_hydro_sod_like COMMAND test_integration_hydro_sod_like)


  add_executable(test_integration_simulation_mode_toy_runs tests/integration/test_simulation_mode_toy_runs.cpp)
  target_link_libraries(test_integration_simulation_mode_toy_runs PRIVATE cosmosim_core)
  add_test(NAME integration_simulation_mode_toy_runs COMMAND test_integration_simulation_mode_toy_runs)

  add_executable(test_integration_pm_periodic_mode tests/integration/test_pm_periodic_mode.cpp)
  target_link_libraries(test_integration_pm_periodic_mode PRIVATE cosmosim_gravity)
  add_test(NAME integration_pm_periodic_mode COMMAND test_integration_pm_periodic_mode)


  add_executable(test_unit_ic_reader tests/unit/test_ic_reader.cpp)
  target_link_libraries(test_unit_ic_reader PRIVATE cosmosim_io)
  add_test(NAME unit_ic_reader COMMAND test_unit_ic_reader)

  add_executable(test_unit_snapshot_hdf5_schema tests/unit/test_snapshot_hdf5_schema.cpp)
  target_link_libraries(test_unit_snapshot_hdf5_schema PRIVATE cosmosim_io)
  add_test(NAME unit_snapshot_hdf5_schema COMMAND test_unit_snapshot_hdf5_schema)


  add_executable(test_unit_restart_checkpoint_schema tests/unit/test_restart_checkpoint_schema.cpp)
  target_link_libraries(test_unit_restart_checkpoint_schema PRIVATE cosmosim_io)
  add_test(NAME unit_restart_checkpoint_schema COMMAND test_unit_restart_checkpoint_schema)

  add_executable(test_unit_tree_pm_split_kernel tests/unit/test_tree_pm_split_kernel.cpp)
  target_link_libraries(test_unit_tree_pm_split_kernel PRIVATE cosmosim_gravity)
  add_test(NAME unit_tree_pm_split_kernel COMMAND test_unit_tree_pm_split_kernel)

  add_executable(test_integration_tree_pm_coupling_periodic tests/integration/test_tree_pm_coupling_periodic.cpp)
  target_link_libraries(test_integration_tree_pm_coupling_periodic PRIVATE cosmosim_gravity)
  add_test(NAME integration_tree_pm_coupling_periodic COMMAND test_integration_tree_pm_coupling_periodic)


  add_executable(test_integration_ic_generated_pipeline tests/integration/test_ic_generated_pipeline.cpp)
  target_link_libraries(test_integration_ic_generated_pipeline PRIVATE cosmosim_all)
  add_test(NAME integration_ic_generated_pipeline COMMAND test_integration_ic_generated_pipeline)

  add_executable(test_integration_snapshot_hdf5_roundtrip tests/integration/test_snapshot_hdf5_roundtrip.cpp)
  target_link_libraries(test_integration_snapshot_hdf5_roundtrip PRIVATE cosmosim_all)
  add_test(NAME integration_snapshot_hdf5_roundtrip COMMAND test_integration_snapshot_hdf5_roundtrip)


  add_executable(test_integration_restart_checkpoint_roundtrip tests/integration/test_restart_checkpoint_roundtrip.cpp)
  target_link_libraries(test_integration_restart_checkpoint_roundtrip PRIVATE cosmosim_all)
  add_test(NAME integration_restart_checkpoint_roundtrip COMMAND test_integration_restart_checkpoint_roundtrip)

  add_executable(test_integration_tree_gravity_vs_direct tests/integration/test_tree_gravity_vs_direct.cpp)
  target_link_libraries(test_integration_tree_gravity_vs_direct PRIVATE cosmosim_gravity)
  add_test(NAME integration_tree_gravity_vs_direct COMMAND test_integration_tree_gravity_vs_direct)
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


  add_executable(bench_scheduler_overhead bench/bench_scheduler_overhead.cpp)
  target_link_libraries(bench_scheduler_overhead PRIVATE cosmosim_core)

  add_executable(bench_mode_dispatch bench/bench_mode_dispatch.cpp)
  target_link_libraries(bench_mode_dispatch PRIVATE cosmosim_core)
  add_executable(bench_active_kernel_compact_vs_full bench/bench_active_kernel_compact_vs_full.cpp)
  target_link_libraries(bench_active_kernel_compact_vs_full PRIVATE cosmosim_core)
  add_executable(bench_time_bin_scheduler bench/bench_time_bin_scheduler.cpp)
  target_link_libraries(bench_time_bin_scheduler PRIVATE cosmosim_core)


  add_executable(bench_ic_reader_streaming bench/bench_ic_reader_streaming.cpp)
  target_link_libraries(bench_ic_reader_streaming PRIVATE cosmosim_io)

  add_executable(bench_snapshot_hdf5_io bench/bench_snapshot_hdf5_io.cpp)
  target_link_libraries(bench_snapshot_hdf5_io PRIVATE cosmosim_io)

  add_executable(bench_restart_checkpoint_latency bench/bench_restart_checkpoint_latency.cpp)
  target_link_libraries(bench_restart_checkpoint_latency PRIVATE cosmosim_io)

  add_executable(bench_pm_solver bench/bench_pm_solver.cpp)
  target_link_libraries(bench_pm_solver PRIVATE cosmosim_gravity)

  add_executable(bench_tree_gravity bench/bench_tree_gravity.cpp)
  target_link_libraries(bench_tree_gravity PRIVATE cosmosim_gravity)

  add_executable(bench_tree_pm_coupling bench/bench_tree_pm_coupling.cpp)
  target_link_libraries(bench_tree_pm_coupling PRIVATE cosmosim_gravity)

  add_executable(bench_hydro_face_sweep bench/bench_hydro_face_sweep.cpp)
  target_link_libraries(bench_hydro_face_sweep PRIVATE cosmosim_hydro)
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
