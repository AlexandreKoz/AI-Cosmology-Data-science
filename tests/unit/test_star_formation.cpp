#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>

#include "cosmosim/core/constants.hpp"
#include "cosmosim/physics/star_formation.hpp"

namespace {

cosmosim::core::SimulationState buildSingleCellState(double gas_mass_code) {
  cosmosim::core::SimulationState state;
  state.resizeCells(1);
  state.cells.center_x_comoving[0] = 1.0;
  state.cells.center_y_comoving[0] = 2.0;
  state.cells.center_z_comoving[0] = 3.0;
  state.cells.mass_code[0] = gas_mass_code;
  state.cells.time_bin[0] = 0;
  state.cells.patch_index[0] = 0;
  return state;
}

void testThresholdEligibility() {
  cosmosim::physics::StarFormationConfig config;
  config.enabled = true;
  config.density_threshold_code = 5.0;
  config.temperature_threshold_k = 1.0e4;
  config.max_velocity_divergence_code = 0.0;
  cosmosim::physics::StarFormationModel model(config);

  assert(model.isCellEligible(5.0, 10000.0, 0.0));
  assert(!model.isCellEligible(4.9, 10000.0, -1.0));
  assert(!model.isCellEligible(6.0, 12000.0, -1.0));
  assert(!model.isCellEligible(6.0, 9000.0, 0.1));
}

void testRateAndFreeFallTime() {
  cosmosim::physics::StarFormationConfig config;
  config.enabled = true;
  config.epsilon_ff = 0.02;
  config.gravitational_constant_code = 1.0;
  cosmosim::physics::StarFormationModel model(config);

  const double density = 32.0;
  const double t_ff = model.freeFallTimeCode(density);
  const double expected_t_ff = std::sqrt(3.0 * cosmosim::core::constants::k_pi / (32.0 * density));
  assert(std::abs(t_ff - expected_t_ff) < 1.0e-12);

  const double expected_mass = model.expectedFormedMassCode(2.0, density, 0.1);
  const double analytic_mass = 0.02 * 2.0 * 0.1 / expected_t_ff;
  assert(std::abs(expected_mass - analytic_mass) < 1.0e-12);
}

void testDeterministicConservationAndMetadata() {
  cosmosim::core::SimulationState state = buildSingleCellState(1.0);

  cosmosim::physics::StarFormationConfig config;
  config.enabled = true;
  config.spawn_mode = cosmosim::physics::StarFormationSpawnMode::kDeterministic;
  config.epsilon_ff = 0.4;
  config.density_threshold_code = 1.0;
  config.temperature_threshold_k = 2.0e4;
  config.max_velocity_divergence_code = 0.0;
  config.gravitational_constant_code = 1.0;

  cosmosim::physics::StarFormationModel model(config);

  const std::array<std::uint32_t, 1> active_cell{0};
  const std::array<double, 1> density{10.0};
  const std::array<double, 1> temperature{5.0e3};
  const std::array<double, 1> velocity_div{-0.5};

  cosmosim::physics::StarFormationStepContext context;
  context.dt_code = 0.1;
  context.scale_factor = 0.4;
  context.step_index = 42;

  const double gas_before = state.cells.mass_code[0];
  const auto result = model.runStep(
      state,
      cosmosim::physics::StarFormationGasView{active_cell, density, temperature, velocity_div},
      context);

  assert(result.counters.spawn_events == 1);
  assert(result.counters.eligible_cells == 1);
  assert(result.counters.consumed_gas_mass_code > 0.0);
  const double gas_after = state.cells.mass_code[0];
  const double star_mass = state.particles.mass_code.back();
  assert(std::abs((gas_before - gas_after) - star_mass) < 1.0e-12);

  cosmosim::physics::writeStarFormationStepMetadata(state, result, context, config);
  const auto* block = state.sidecars.find("star_formation");
  assert(block != nullptr);
  const std::string payload(reinterpret_cast<const char*>(block->payload.data()), block->payload.size());
  assert(payload.find("spawn_events=1") != std::string::npos);
  assert(payload.find("step_index=42") != std::string::npos);
}

}  // namespace

int main() {
  testThresholdEligibility();
  testRateAndFreeFallTime();
  testDeterministicConservationAndMetadata();
  return 0;
}
