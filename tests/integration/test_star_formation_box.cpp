#include <cassert>
#include <cmath>
#include <numeric>
#include <vector>

#include "cosmosim/core/simulation_state.hpp"
#include "cosmosim/physics/star_formation.hpp"

namespace {

double totalStarMass(const cosmosim::core::SimulationState& state) {
  double total = 0.0;
  for (double mass : state.particles.mass_code) {
    total += mass;
  }
  return total;
}

void setupBox(cosmosim::core::SimulationState& state, std::size_t cell_count) {
  state.resizeCells(cell_count);
  for (std::size_t i = 0; i < cell_count; ++i) {
    state.cells.mass_code[i] = 2.0;
    state.cells.center_x_comoving[i] = static_cast<double>(i);
    state.cells.center_y_comoving[i] = 0.0;
    state.cells.center_z_comoving[i] = 0.0;
    state.cells.time_bin[i] = 0;
    state.gas_cells.density_code[i] = 64.0;
    state.gas_cells.temperature_code[i] = 100.0;
  }
  state.species.count_by_species[static_cast<std::size_t>(cosmosim::core::ParticleSpecies::kGas)] = cell_count;
}

void testSmallBoxTrendAndRegressionPayload() {
  cosmosim::physics::StarFormationConfig config;
  config.epsilon_ff = 0.5;
  config.min_star_particle_mass_code = 0.25;
  config.random_seed_base = 1234;
  config.spawning_model = "stochastic";
  cosmosim::physics::StarFormationModel model(config);

  cosmosim::core::SimulationState state;
  setupBox(state, 4);

  std::vector<std::uint32_t> active{0, 1, 2, 3};
  const double initial_gas_mass = std::accumulate(state.cells.mass_code.begin(), state.cells.mass_code.end(), 0.0);

  cosmosim::physics::StarFormationCounters aggregate;
  for (std::uint64_t step = 0; step < 5; ++step) {
    const auto counters = model.applyToActiveGasCells(state, active, {}, 0.2, 0.2 + 0.1 * step, step);
    aggregate.spawn_events += counters.spawn_events;
    aggregate.gas_mass_consumed_code += counters.gas_mass_consumed_code;
    aggregate.stellar_mass_spawned_code += counters.stellar_mass_spawned_code;
  }

  const double final_gas_mass = std::accumulate(state.cells.mass_code.begin(), state.cells.mass_code.end(), 0.0);
  const double star_mass = totalStarMass(state);

  assert(aggregate.spawn_events > 0);
  assert(final_gas_mass < initial_gas_mass);
  assert(star_mass > 0.0);
  assert(std::abs((initial_gas_mass - final_gas_mass) - star_mass) < 1.0e-10);

  const auto* block = state.sidecars.find("star_formation");
  assert(block != nullptr);
  const std::string payload(reinterpret_cast<const char*>(block->payload.data()), block->payload.size());
  assert(payload.find("schema_version=1") != std::string::npos);
  assert(payload.find("spawning_model=stochastic") != std::string::npos);
}

}  // namespace

int main() {
  testSmallBoxTrendAndRegressionPayload();
  return 0;
}
