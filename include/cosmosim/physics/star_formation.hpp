#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "cosmosim/core/simulation_state.hpp"

namespace cosmosim::physics {

enum class StarFormationSpawnMode : std::uint8_t {
  kDeterministic = 0,
  kStochastic = 1,
};

struct StarFormationConfig {
  bool enabled = false;
  double epsilon_ff = 0.01;
  double density_threshold_code = 10.0;
  double temperature_threshold_k = 1.0e4;
  double max_velocity_divergence_code = 0.0;
  double gravitational_constant_code = 1.0;
  double minimum_star_particle_mass_code = 0.01;
  StarFormationSpawnMode spawn_mode = StarFormationSpawnMode::kStochastic;
  std::uint64_t rng_seed = 1;
};

struct StarFormationStepContext {
  double dt_code = 0.0;
  double scale_factor = 1.0;
  std::uint64_t step_index = 0;
  std::uint32_t rank_id = 0;
};

struct StarFormationGasView {
  std::span<const std::uint32_t> active_cell_index;
  std::span<const double> density_code;
  std::span<const double> temperature_k;
  std::span<const double> velocity_divergence_code;
};

struct StarFormationCounters {
  std::uint64_t eligible_cells = 0;
  std::uint64_t spawn_events = 0;
  double formed_stellar_mass_code = 0.0;
  double consumed_gas_mass_code = 0.0;
};

struct StarFormationEventRecord {
  std::uint32_t source_cell_index = 0;
  std::uint32_t spawned_particle_index = 0;
  double formed_mass_code = 0.0;
  double formation_scale_factor = 1.0;
  double spawn_probability = 1.0;
  std::uint64_t rng_draw_u64 = 0;
};

struct StarFormationStepResult {
  StarFormationCounters counters;
  std::vector<StarFormationEventRecord> events;
};

class StarFormationModel {
 public:
  explicit StarFormationModel(StarFormationConfig config);

  [[nodiscard]] const StarFormationConfig& config() const noexcept;
  [[nodiscard]] bool isCellEligible(
      double density_code,
      double temperature_k,
      double velocity_divergence_code) const noexcept;
  [[nodiscard]] double freeFallTimeCode(double density_code) const noexcept;
  [[nodiscard]] double expectedFormedMassCode(
      double gas_mass_code,
      double density_code,
      double dt_code) const noexcept;

  [[nodiscard]] StarFormationStepResult runStep(
      core::SimulationState& state,
      const StarFormationGasView& gas_view,
      const StarFormationStepContext& context) const;

 private:
  StarFormationConfig m_config;
};

void writeStarFormationStepMetadata(
    core::SimulationState& state,
    const StarFormationStepResult& result,
    const StarFormationStepContext& context,
    const StarFormationConfig& config);

[[nodiscard]] std::string starFormationSpawnModeToString(StarFormationSpawnMode mode);
[[nodiscard]] StarFormationSpawnMode starFormationSpawnModeFromString(const std::string& mode);

}  // namespace cosmosim::physics
