#include <cassert>
#include <cmath>
#include <string>

#include "cosmosim/physics/star_formation.hpp"

namespace {

void initializeSingleCellState(cosmosim::core::SimulationState& state, double gas_mass, double density, double temp_k) {
  state.resizeCells(1);
  state.cells.mass_code[0] = gas_mass;
  state.cells.center_x_comoving[0] = 0.5;
  state.cells.center_y_comoving[0] = 0.5;
  state.cells.center_z_comoving[0] = 0.5;
  state.cells.time_bin[0] = 0;
  state.gas_cells.density_code[0] = density;
  state.gas_cells.temperature_code[0] = temp_k;
  state.species.count_by_species[static_cast<std::size_t>(cosmosim::core::ParticleSpecies::kGas)] = 1;
}

void testEligibilityThresholds() {
  cosmosim::physics::StarFormationModel model(cosmosim::physics::StarFormationConfig{});
  const auto eligible = model.evaluateEligibility(100.0, 1000.0, 0.5);
  assert(eligible.eligible);

  const auto too_hot = model.evaluateEligibility(100.0, 1.0e6, 0.5);
  assert(!too_hot.eligible);
  assert(too_hot.density_pass);
  assert(!too_hot.temperature_pass);
}

void testRateCalculation() {
  cosmosim::physics::StarFormationConfig config;
  config.epsilon_ff = 0.02;
  cosmosim::physics::StarFormationModel model(config);
  const auto rate = model.evaluateRate(64.0, 8.0, 0.1, true);
  const double expected_t_ff = std::sqrt((3.0 * 3.14159265358979323846) / (32.0 * 64.0));
  const double expected_mass = 0.02 * 8.0 * 0.1 / expected_t_ff;
  assert(std::abs(rate.free_fall_time_code - expected_t_ff) < 1.0e-12);
  assert(std::abs(rate.expected_stellar_mass_code - expected_mass) < 1.0e-12);
}

void testConservationAndMetadataSidecar() {
  cosmosim::physics::StarFormationConfig config;
  config.spawning_model = "stochastic";
  config.min_star_particle_mass_code = 1.0;
  config.random_seed_base = 0;
  config.epsilon_ff = 1.0;

  cosmosim::physics::StarFormationModel model(config);
  cosmosim::core::SimulationState state;
  initializeSingleCellState(state, 5.0, 100.0, 100.0);

  const std::array<std::uint32_t, 1> active{0};
  const auto counters = model.applyToActiveGasCells(state, active, {}, 1.0, 0.5, 3);

  assert(counters.spawn_events >= 1);
  assert(std::abs(counters.gas_mass_consumed_code - counters.stellar_mass_spawned_code) < 1.0e-12);
  assert(state.particles.size() == counters.spawn_events);
  assert(state.cells.mass_code[0] == 5.0 - counters.gas_mass_consumed_code);

  const auto* block = state.sidecars.find("star_formation");
  assert(block != nullptr);
  const std::string payload(reinterpret_cast<const char*>(block->payload.data()), block->payload.size());
  assert(payload.find("spawn_events=") != std::string::npos);
}

}  // namespace

int main() {
  testEligibilityThresholds();
  testRateCalculation();
  testConservationAndMetadataSidecar();
  return 0;
}
