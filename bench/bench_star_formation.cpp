#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

#include "cosmosim/core/build_config.hpp"
#include "cosmosim/core/simulation_state.hpp"
#include "cosmosim/physics/star_formation.hpp"

int main() {
  constexpr std::size_t k_cells = 1 << 18;
  constexpr std::size_t k_iterations = 30;

  cosmosim::core::SimulationState state;
  state.resizeCells(k_cells);
  for (std::size_t i = 0; i < k_cells; ++i) {
    state.cells.mass_code[i] = 1.0;
    state.cells.center_x_comoving[i] = static_cast<double>(i);
    state.cells.center_y_comoving[i] = 0.0;
    state.cells.center_z_comoving[i] = 0.0;
    state.cells.time_bin[i] = 0;
    state.gas_cells.density_code[i] = 100.0;
    state.gas_cells.temperature_code[i] = (i % 8 == 0) ? 5.0e4 : 100.0;
  }

  std::vector<std::uint32_t> active_cell_indices(k_cells);
  std::iota(active_cell_indices.begin(), active_cell_indices.end(), 0U);

  cosmosim::physics::StarFormationConfig config;
  config.spawning_model = "stochastic";
  config.epsilon_ff = 0.02;
  config.min_star_particle_mass_code = 0.05;
  config.random_seed_base = 42;
  cosmosim::physics::StarFormationModel model(config);

  const auto setup_start = std::chrono::steady_clock::now();
  auto warmup = model.applyToActiveGasCells(state, active_cell_indices, {}, 0.02, 0.25, 0);
  const auto setup_end = std::chrono::steady_clock::now();

  std::uint64_t total_spawn_events = warmup.spawn_events;
  std::uint64_t total_eligible = warmup.eligible_cells;
  const auto steady_start = std::chrono::steady_clock::now();
  for (std::size_t step = 0; step < k_iterations; ++step) {
    const auto counters = model.applyToActiveGasCells(
        state,
        active_cell_indices,
        {},
        0.02,
        0.3 + 0.001 * static_cast<double>(step),
        static_cast<std::uint64_t>(step + 1));
    total_spawn_events += counters.spawn_events;
    total_eligible += counters.eligible_cells;
  }
  const auto steady_end = std::chrono::steady_clock::now();

  const double setup_ms = std::chrono::duration<double, std::milli>(setup_end - setup_start).count();
  const double steady_ms = std::chrono::duration<double, std::milli>(steady_end - steady_start).count();
  const double steady_s = steady_ms * 1.0e-3;
  const double scanned_cells = static_cast<double>(k_cells * k_iterations);

  std::cout << "bench_star_formation"
            << " build_type=" << COSMOSIM_BUILD_TYPE
            << " hardware=cpu"
            << " threads=1"
            << " features=schmidt_kennicutt+stochastic_spawn"
            << " setup_ms=" << setup_ms
            << " steady_ms=" << steady_ms
            << " scanned_cells=" << scanned_cells
            << " eligible_cells=" << total_eligible
            << " spawn_events=" << total_spawn_events
            << " scan_throughput_cells_per_s=" << (scanned_cells / steady_s)
            << " effective_read_bandwidth_gb_s="
            << ((scanned_cells * 3.0 * sizeof(double)) / steady_s * 1.0e-9)
            << '\n';

  return 0;
}
