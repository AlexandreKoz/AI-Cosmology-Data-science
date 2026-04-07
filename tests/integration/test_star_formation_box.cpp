#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "cosmosim/physics/star_formation.hpp"

int main() {
  cosmosim::core::SimulationState state;
  state.resizeCells(8);
  for (std::size_t i = 0; i < state.cells.size(); ++i) {
    state.cells.center_x_comoving[i] = static_cast<double>(i);
    state.cells.center_y_comoving[i] = 0.0;
    state.cells.center_z_comoving[i] = 0.0;
    state.cells.mass_code[i] = 1.0;
    state.cells.time_bin[i] = 0;
    state.cells.patch_index[i] = 0;
  }

  cosmosim::physics::StarFormationConfig config;
  config.enabled = true;
  config.spawn_mode = cosmosim::physics::StarFormationSpawnMode::kStochastic;
  config.epsilon_ff = 0.5;
  config.density_threshold_code = 1.0;
  config.temperature_threshold_k = 2.0e4;
  config.max_velocity_divergence_code = 0.0;
  config.gravitational_constant_code = 1.0;
  config.minimum_star_particle_mass_code = 0.05;
  config.rng_seed = 19;

  cosmosim::physics::StarFormationModel model(config);

  std::vector<std::uint32_t> active_index(state.cells.size());
  std::vector<double> density(state.cells.size(), 20.0);
  std::vector<double> temperature(state.cells.size(), 8.0e3);
  std::vector<double> velocity_div(state.cells.size(), -0.5);
  for (std::size_t i = 0; i < active_index.size(); ++i) {
    active_index[i] = static_cast<std::uint32_t>(i);
  }

  double initial_gas_mass = 0.0;
  for (double mass : state.cells.mass_code) {
    initial_gas_mass += mass;
  }

  std::uint64_t total_events = 0;
  for (std::uint64_t step = 0; step < 4; ++step) {
    const auto result = model.runStep(
        state,
        cosmosim::physics::StarFormationGasView{active_index, density, temperature, velocity_div},
        cosmosim::physics::StarFormationStepContext{.dt_code = 0.05, .scale_factor = 0.5, .step_index = step, .rank_id = 0});
    total_events += result.counters.spawn_events;
    cosmosim::physics::writeStarFormationStepMetadata(
        state,
        result,
        cosmosim::physics::StarFormationStepContext{.dt_code = 0.05, .scale_factor = 0.5, .step_index = step, .rank_id = 0},
        config);
  }

  double final_gas_mass = 0.0;
  for (double mass : state.cells.mass_code) {
    final_gas_mass += mass;
  }
  double final_star_mass = 0.0;
  for (double mass : state.particles.mass_code) {
    final_star_mass += mass;
  }

  assert(total_events > 0);
  assert(final_gas_mass < initial_gas_mass);
  assert(final_star_mass > 0.0);
  assert(std::abs((initial_gas_mass - final_gas_mass) - final_star_mass) < 1.0e-10);
  assert(state.star_particles.size() == state.particles.size());
  assert(state.sidecars.find("star_formation") != nullptr);
  return 0;
}
