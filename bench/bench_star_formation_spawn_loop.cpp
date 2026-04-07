#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "cosmosim/core/build_config.hpp"
#include "cosmosim/physics/star_formation.hpp"

int main() {
  constexpr std::size_t k_cells = 1 << 18;
  constexpr std::size_t k_iterations = 12;

  cosmosim::core::SimulationState state;
  state.resizeCells(k_cells);
  for (std::size_t i = 0; i < k_cells; ++i) {
    state.cells.center_x_comoving[i] = static_cast<double>(i);
    state.cells.center_y_comoving[i] = 0.0;
    state.cells.center_z_comoving[i] = 0.0;
    state.cells.mass_code[i] = 2.0;
    state.cells.time_bin[i] = static_cast<std::uint8_t>(i % 8);
    state.cells.patch_index[i] = 0;
  }

  std::vector<std::uint32_t> active_cell_index(k_cells);
  std::vector<double> density_code(k_cells, 32.0);
  std::vector<double> temperature_k(k_cells, 8000.0);
  std::vector<double> velocity_divergence_code(k_cells, -0.25);
  for (std::size_t i = 0; i < k_cells; ++i) {
    active_cell_index[i] = static_cast<std::uint32_t>(i);
  }

  cosmosim::physics::StarFormationConfig config;
  config.enabled = true;
  config.spawn_mode = cosmosim::physics::StarFormationSpawnMode::kStochastic;
  config.epsilon_ff = 0.03;
  config.density_threshold_code = 2.0;
  config.temperature_threshold_k = 1.2e4;
  config.max_velocity_divergence_code = 0.0;
  config.gravitational_constant_code = 1.0;
  config.minimum_star_particle_mass_code = 0.02;
  config.rng_seed = 12345;

  cosmosim::physics::StarFormationModel model(config);
  const auto gas_view = cosmosim::physics::StarFormationGasView{
      active_cell_index,
      density_code,
      temperature_k,
      velocity_divergence_code};

  volatile double sink = 0.0;
  const auto setup_begin = std::chrono::steady_clock::now();
  auto setup_result = model.runStep(
      state,
      gas_view,
      cosmosim::physics::StarFormationStepContext{.dt_code = 0.01, .scale_factor = 0.5, .step_index = 0, .rank_id = 0});
  sink += setup_result.counters.formed_stellar_mass_code;
  const auto setup_end = std::chrono::steady_clock::now();

  const auto steady_begin = std::chrono::steady_clock::now();
  for (std::size_t iter = 0; iter < k_iterations; ++iter) {
    auto result = model.runStep(
        state,
        gas_view,
        cosmosim::physics::StarFormationStepContext{
            .dt_code = 0.01,
            .scale_factor = 0.5,
            .step_index = static_cast<std::uint64_t>(iter + 1),
            .rank_id = 0});
    sink += result.counters.formed_stellar_mass_code;
  }
  const auto steady_end = std::chrono::steady_clock::now();

  const double setup_ms = std::chrono::duration<double, std::milli>(setup_end - setup_begin).count();
  const double steady_ms = std::chrono::duration<double, std::milli>(steady_end - steady_begin).count();
  const double steady_s = steady_ms * 1.0e-3;
  const double loop_updates = static_cast<double>(k_cells * k_iterations);

  std::cout << "bench_star_formation_spawn_loop"
            << " build_type=" << COSMOSIM_BUILD_TYPE
            << " hardware=cpu"
            << " threads=1"
            << " features=star_formation+stochastic_spawn"
            << " setup_ms=" << setup_ms
            << " steady_ms=" << steady_ms
            << " cells=" << k_cells
            << " iterations=" << k_iterations
            << " active_cell_updates_per_s=" << (loop_updates / steady_s)
            << " effective_input_bandwidth_gb_s="
            << ((loop_updates * 3.0 * sizeof(double)) / steady_s * 1.0e-9)
            << " sink=" << sink
            << '\n';

  return 0;
}
