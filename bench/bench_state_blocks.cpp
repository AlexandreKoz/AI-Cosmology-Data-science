#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "cosmosim/core/simulation_state.hpp"

int main() {
  constexpr std::size_t particle_count = 2'000'000;
  constexpr std::size_t cell_count = 500'000;

  cosmosim::core::SimulationState state;

  const auto start_alloc = std::chrono::steady_clock::now();
  auto& dm = state.speciesState(cosmosim::core::ParticleSpecies::dark_matter);
  dm.resize(particle_count);
  state.cells.resize(cell_count);
  const auto stop_alloc = std::chrono::steady_clock::now();

  const auto start_resize = std::chrono::steady_clock::now();
  dm.resize(particle_count / 2);
  state.cells.resize(cell_count / 2);
  dm.resize(particle_count);
  state.cells.resize(cell_count);
  const auto stop_resize = std::chrono::steady_clock::now();

  const auto alloc_us =
      std::chrono::duration_cast<std::chrono::microseconds>(stop_alloc - start_alloc).count();
  const auto resize_us =
      std::chrono::duration_cast<std::chrono::microseconds>(stop_resize - start_resize).count();

  std::cout << "bench_state_blocks alloc_us=" << alloc_us << " resize_us=" << resize_us
            << " particles=" << dm.size() << " cells=" << state.cells.size() << '\n';

  return state.validateOwnershipInvariants() ? 0 : 1;
}
