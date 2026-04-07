cmake_minimum_required(VERSION 3.20)
project(cosmosim_stellar_evolution LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cosmosim_stellar_evolution
    src/stellar_evolution_bookkeeping.cpp
)

target_include_directories(cosmosim_stellar_evolution PUBLIC include)

add_executable(test_stellar_evolution tests/test_stellar_evolution.cpp)
target_link_libraries(test_stellar_evolution PRIVATE cosmosim_stellar_evolution)
target_compile_definitions(test_stellar_evolution PRIVATE COSMOSIM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_executable(test_ssp_integration tests/test_ssp_integration.cpp)
target_link_libraries(test_ssp_integration PRIVATE cosmosim_stellar_evolution)
target_compile_definitions(test_ssp_integration PRIVATE COSMOSIM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_executable(bench_stellar_evolution bench/bench_stellar_evolution.cpp)
target_link_libraries(bench_stellar_evolution PRIVATE cosmosim_stellar_evolution)
target_compile_definitions(bench_stellar_evolution PRIVATE COSMOSIM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

enable_testing()
add_test(NAME test_stellar_evolution COMMAND test_stellar_evolution)
add_test(NAME test_ssp_integration COMMAND test_ssp_integration)
