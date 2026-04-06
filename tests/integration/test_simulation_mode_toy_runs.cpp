#include <array>
#include <cassert>
#include <string>
#include <vector>

#include "cosmosim/core/config.hpp"
#include "cosmosim/core/simulation_mode.hpp"

namespace {

void testPeriodicToyRunPolicyAndGhosts() {
  const std::string config_text = R"(
[mode]
mode = cosmo_cube
hydro_boundary = periodic
gravity_boundary = periodic
)";

  const auto frozen = cosmosim::core::loadFrozenConfigFromString(config_text, "periodic_toy");
  const auto policy = cosmosim::core::buildModePolicy(frozen.config.mode);

  assert(policy.hydro_boundary == cosmosim::core::BoundaryCondition::kPeriodic);
  assert(policy.gravity_boundary == cosmosim::core::GravityBoundaryModel::kPeriodicPoisson);

  const std::array<double, 3> potential = {11.0, 12.0, 13.0};
  std::vector<double> left(1, 0.0);
  std::vector<double> right(1, 0.0);
  cosmosim::core::fillGravityPotentialGhostCells1d(
      potential,
      left,
      right,
      policy.gravity_boundary,
      -1.0);

  assert(left[0] == 13.0);
  assert(right[0] == 11.0);
}

void testIsolatedToyRunPolicyAndGhosts() {
  const std::string config_text = R"(
[mode]
mode = isolated_galaxy
hydro_boundary = open
gravity_boundary = isolated_monopole
)";

  const auto frozen = cosmosim::core::loadFrozenConfigFromString(config_text, "isolated_toy");
  const auto policy = cosmosim::core::buildModePolicy(frozen.config.mode);
  cosmosim::core::validateModePolicy(frozen.config, policy);

  assert(policy.hydro_boundary == cosmosim::core::BoundaryCondition::kOpen);
  assert(policy.gravity_boundary == cosmosim::core::GravityBoundaryModel::kIsolatedMonopoleDirichlet);

  const std::array<double, 3> density = {1.0, 2.0, 3.0};
  const std::array<double, 3> velocity = {4.0, 5.0, 6.0};
  const std::array<double, 3> pressure = {7.0, 8.0, 9.0};

  std::vector<double> left_density(1, 0.0);
  std::vector<double> right_density(1, 0.0);
  std::vector<double> left_velocity(1, 0.0);
  std::vector<double> right_velocity(1, 0.0);
  std::vector<double> left_pressure(1, 0.0);
  std::vector<double> right_pressure(1, 0.0);

  cosmosim::core::fillHydroGhostCells1d(
      {.density_code = density, .velocity_normal_code = velocity, .pressure_code = pressure},
      {.left_density_code = left_density,
       .right_density_code = right_density,
       .left_velocity_normal_code = left_velocity,
       .right_velocity_normal_code = right_velocity,
       .left_pressure_code = left_pressure,
       .right_pressure_code = right_pressure},
      policy.hydro_boundary);

  assert(left_density[0] == 1.0);
  assert(right_density[0] == 3.0);
  assert(left_velocity[0] == 4.0);
  assert(right_pressure[0] == 9.0);
}

}  // namespace

int main() {
  testPeriodicToyRunPolicyAndGhosts();
  testIsolatedToyRunPolicyAndGhosts();
  return 0;
}
