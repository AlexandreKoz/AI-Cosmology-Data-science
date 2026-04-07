#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "cosmosim/core/simulation_state.hpp"

namespace cosmosim::physics {

struct StarFormationConfig {
  bool enabled = true;
  std::string spawning_model = "stochastic";
  double epsilon_ff = 0.01;
  double density_threshold_code = 10.0;
  double temperature_threshold_k = 2.0e4;
  double virial_parameter_max = 1.0;
  double min_star_particle_mass_code = 0.1;
  std::uint64_t random_seed_base = 1;
  double star_metallicity_mass_fraction = 0.02;
};

struct StarFormationEligibility {
  bool eligible = false;
  bool density_pass = false;
  bool temperature_pass = false;
  bool virial_pass = false;
};

struct StarFormationRate {
  double free_fall_time_code = 0.0;
  double sfr_density_code = 0.0;
  double expected_stellar_mass_code = 0.0;
};

struct StarFormationCounters {
  std::uint64_t scanned_cells = 0;
  std::uint64_t eligible_cells = 0;
  std::uint64_t spawn_events = 0;
  double gas_mass_consumed_code = 0.0;
  double stellar_mass_spawned_code = 0.0;
};

class StarFormationModel {
 public:
  explicit StarFormationModel(StarFormationConfig config);

  [[nodiscard]] const StarFormationConfig& config() const noexcept;
  [[nodiscard]] StarFormationEligibility evaluateEligibility(
      double density_code,
      double temperature_k,
      double virial_parameter) const;
  [[nodiscard]] StarFormationRate evaluateRate(
      double gas_density_code,
      double gas_mass_code,
      double dt_code,
      bool eligible) const;

  [[nodiscard]] StarFormationCounters applyToActiveGasCells(
      core::SimulationState& state,
      std::span<const std::uint32_t> active_cell_indices,
      std::span<const double> virial_parameter_by_cell,
      double dt_code,
      double formation_scale_factor,
      std::uint64_t step_index) const;

 private:
  StarFormationConfig m_config;
};

}  // namespace cosmosim::physics
