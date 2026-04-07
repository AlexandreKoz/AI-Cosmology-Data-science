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
  double density_threshold_code = 10.0;
  double temperature_threshold_k = 1.0e4;
  double min_converging_flow_rate_code = 0.0;
  double epsilon_ff = 0.01;
  double min_star_particle_mass_code = 0.1;
  bool stochastic_spawning = true;
  std::uint64_t random_seed = 123456789ull;
  std::uint32_t metadata_schema_version = 1;
};

struct StarFormationCellInput {
  std::uint32_t cell_index = 0;
  double gas_mass_code = 0.0;
  double gas_density_code = 0.0;
  double gas_temperature_k = 0.0;
  double velocity_divergence_code = 0.0;
  double metallicity_mass_fraction = 0.0;
};

struct StarFormationCounters {
  std::uint64_t scanned_cells = 0;
  std::uint64_t eligible_cells = 0;
  std::uint64_t spawn_events = 0;
  std::uint64_t spawned_particles = 0;
  double expected_spawn_mass_code = 0.0;
  double spawned_mass_code = 0.0;
};

struct StarFormationCellOutcome {
  bool eligible = false;
  double free_fall_time_code = 0.0;
  double sfr_density_rate_code = 0.0;
  double expected_spawn_mass_code = 0.0;
  std::uint32_t spawned_particle_count = 0;
  double spawned_mass_code = 0.0;
  double random_u01 = 0.0;
};

struct StarFormationStepReport {
  StarFormationCounters counters;
  std::vector<std::uint32_t> spawned_from_cells;
};

class StarFormationModel {
 public:
  explicit StarFormationModel(StarFormationConfig config);

  [[nodiscard]] const StarFormationConfig& config() const noexcept;
  [[nodiscard]] bool isEligible(const StarFormationCellInput& cell) const;
  [[nodiscard]] double freeFallTimeCode(double gas_density_code) const;
  [[nodiscard]] double sfrDensityRateCode(double gas_density_code) const;
  [[nodiscard]] double expectedSpawnMassCode(const StarFormationCellInput& cell, double dt_code) const;
  [[nodiscard]] StarFormationCellOutcome sampleCellOutcome(
      const StarFormationCellInput& cell,
      double dt_code,
      std::uint64_t step_index,
      std::uint32_t rank_local_seed_offset = 0) const;

  [[nodiscard]] StarFormationStepReport apply(
      core::SimulationState& state,
      std::span<const std::uint32_t> active_cell_indices,
      std::span<const double> velocity_divergence_code,
      std::span<const double> metallicity_mass_fraction,
      double dt_code,
      double scale_factor,
      std::uint64_t step_index,
      std::uint32_t rank_local_seed_offset = 0) const;

  [[nodiscard]] core::ModuleSidecarBlock buildMetadataSidecar(const StarFormationCounters& counters) const;

 private:
  StarFormationConfig m_config;
};

}  // namespace cosmosim::physics
