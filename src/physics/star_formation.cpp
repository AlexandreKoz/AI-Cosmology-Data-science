#include "cosmosim/physics/star_formation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#include "cosmosim/core/constants.hpp"

namespace cosmosim::physics {
namespace {

constexpr double k_density_floor = 1.0e-30;
constexpr double k_mass_floor = 1.0e-30;

[[nodiscard]] std::uint64_t splitMix64(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30U)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27U)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31U);
}

[[nodiscard]] std::uint64_t hashEventSeed(
    std::uint64_t base_seed,
    std::uint64_t step_index,
    std::uint32_t rank_id,
    std::uint32_t cell_index,
    std::uint32_t event_ordinal) {
  std::uint64_t value = splitMix64(base_seed);
  value ^= splitMix64(step_index + 0xA5A5A5A5ULL);
  value ^= splitMix64(static_cast<std::uint64_t>(rank_id) << 32U);
  value ^= splitMix64(static_cast<std::uint64_t>(cell_index) << 1U);
  value ^= splitMix64(static_cast<std::uint64_t>(event_ordinal) << 17U);
  return splitMix64(value);
}

[[nodiscard]] double toUnitUniform(std::uint64_t draw_u64) {
  constexpr double k_norm = 1.0 / static_cast<double>(std::numeric_limits<std::uint64_t>::max());
  return static_cast<double>(draw_u64) * k_norm;
}

[[nodiscard]] std::uint64_t nextParticleId(const core::SimulationState& state) {
  if (state.particle_sidecar.particle_id.empty()) {
    return 1;
  }
  const auto max_it = std::max_element(
      state.particle_sidecar.particle_id.begin(),
      state.particle_sidecar.particle_id.end());
  return *max_it + 1;
}

void appendStarParticle(
    core::SimulationState& state,
    std::uint32_t source_cell_index,
    double birth_mass_code,
    double formation_scale_factor,
    std::uint64_t particle_id) {
  const std::size_t particle_index = state.particles.size();

  state.particles.position_x_comoving.push_back(state.cells.center_x_comoving[source_cell_index]);
  state.particles.position_y_comoving.push_back(state.cells.center_y_comoving[source_cell_index]);
  state.particles.position_z_comoving.push_back(state.cells.center_z_comoving[source_cell_index]);
  state.particles.velocity_x_peculiar.push_back(0.0);
  state.particles.velocity_y_peculiar.push_back(0.0);
  state.particles.velocity_z_peculiar.push_back(0.0);
  state.particles.mass_code.push_back(birth_mass_code);
  state.particles.time_bin.push_back(state.cells.time_bin[source_cell_index]);

  state.particle_sidecar.particle_id.push_back(particle_id);
  state.particle_sidecar.sfc_key.push_back(0);
  state.particle_sidecar.species_tag.push_back(static_cast<std::uint32_t>(core::ParticleSpecies::kStar));
  state.particle_sidecar.particle_flags.push_back(0);
  state.particle_sidecar.owning_rank.push_back(0);

  state.star_particles.particle_index.push_back(static_cast<std::uint32_t>(particle_index));
  state.star_particles.formation_scale_factor.push_back(formation_scale_factor);
  state.star_particles.birth_mass_code.push_back(birth_mass_code);
  state.star_particles.metallicity_mass_fraction.push_back(0.0);
}

[[nodiscard]] std::vector<std::byte> encodeUtf8(const std::string& text) {
  std::vector<std::byte> payload(text.size());
  if (!text.empty()) {
    std::memcpy(payload.data(), text.data(), text.size());
  }
  return payload;
}

}  // namespace

StarFormationModel::StarFormationModel(StarFormationConfig config) : m_config(std::move(config)) {
  if (m_config.epsilon_ff < 0.0 || m_config.epsilon_ff > 1.0) {
    throw std::invalid_argument("StarFormationModel: epsilon_ff must be in [0, 1]");
  }
  if (m_config.density_threshold_code <= 0.0) {
    throw std::invalid_argument("StarFormationModel: density_threshold_code must be > 0");
  }
  if (m_config.temperature_threshold_k <= 0.0) {
    throw std::invalid_argument("StarFormationModel: temperature_threshold_k must be > 0");
  }
  if (m_config.gravitational_constant_code <= 0.0) {
    throw std::invalid_argument("StarFormationModel: gravitational_constant_code must be > 0");
  }
  if (m_config.minimum_star_particle_mass_code <= 0.0) {
    throw std::invalid_argument("StarFormationModel: minimum_star_particle_mass_code must be > 0");
  }
}

const StarFormationConfig& StarFormationModel::config() const noexcept {
  return m_config;
}

bool StarFormationModel::isCellEligible(
    double density_code,
    double temperature_k,
    double velocity_divergence_code) const noexcept {
  if (!m_config.enabled) {
    return false;
  }
  return density_code >= m_config.density_threshold_code &&
      temperature_k <= m_config.temperature_threshold_k &&
      velocity_divergence_code <= m_config.max_velocity_divergence_code;
}

double StarFormationModel::freeFallTimeCode(double density_code) const noexcept {
  const double safe_density = std::max(density_code, k_density_floor);
  return std::sqrt(3.0 * core::constants::k_pi / (32.0 * m_config.gravitational_constant_code * safe_density));
}

double StarFormationModel::expectedFormedMassCode(
    double gas_mass_code,
    double density_code,
    double dt_code) const noexcept {
  const double t_ff = freeFallTimeCode(density_code);
  const double expected_mass = m_config.epsilon_ff * std::max(gas_mass_code, 0.0) * dt_code / std::max(t_ff, 1.0e-30);
  return std::max(0.0, expected_mass);
}

StarFormationStepResult StarFormationModel::runStep(
    core::SimulationState& state,
    const StarFormationGasView& gas_view,
    const StarFormationStepContext& context) const {
  if (context.dt_code <= 0.0) {
    throw std::invalid_argument("StarFormationModel::runStep requires dt_code > 0");
  }

  const std::size_t n = gas_view.active_cell_index.size();
  if (gas_view.density_code.size() != n || gas_view.temperature_k.size() != n ||
      gas_view.velocity_divergence_code.size() != n) {
    throw std::invalid_argument("StarFormationModel::runStep span size mismatch");
  }

  StarFormationStepResult result;
  std::uint64_t particle_id = nextParticleId(state);

  for (std::size_t i = 0; i < n; ++i) {
    const std::uint32_t cell_index = gas_view.active_cell_index[i];
    if (cell_index >= state.cells.size()) {
      throw std::out_of_range("StarFormationModel::runStep active cell index out of range");
    }

    const double density_code = gas_view.density_code[i];
    const double temperature_k = gas_view.temperature_k[i];
    const double velocity_divergence_code = gas_view.velocity_divergence_code[i];
    const bool eligible = isCellEligible(density_code, temperature_k, velocity_divergence_code);
    if (!eligible) {
      continue;
    }

    ++result.counters.eligible_cells;
    const double gas_mass = state.cells.mass_code[cell_index];
    const double expected_mass = std::min(
        expectedFormedMassCode(gas_mass, density_code, context.dt_code),
        std::max(gas_mass, 0.0));
    if (expected_mass <= k_mass_floor) {
      continue;
    }

    if (m_config.spawn_mode == StarFormationSpawnMode::kDeterministic) {
      const double formed_mass = std::min(expected_mass, gas_mass);
      if (formed_mass < k_mass_floor) {
        continue;
      }
      state.cells.mass_code[cell_index] -= formed_mass;
      appendStarParticle(state, cell_index, formed_mass, context.scale_factor, particle_id++);

      ++result.counters.spawn_events;
      result.counters.consumed_gas_mass_code += formed_mass;
      result.counters.formed_stellar_mass_code += formed_mass;
      result.events.push_back(StarFormationEventRecord{
          .source_cell_index = cell_index,
          .spawned_particle_index = static_cast<std::uint32_t>(state.particles.size() - 1U),
          .formed_mass_code = formed_mass,
          .formation_scale_factor = context.scale_factor,
          .spawn_probability = 1.0,
          .rng_draw_u64 = 0});
      continue;
    }

    const double spawn_mass = std::min(m_config.minimum_star_particle_mass_code, gas_mass);
    if (spawn_mass < k_mass_floor) {
      continue;
    }

    const std::uint32_t guaranteed_spawns = static_cast<std::uint32_t>(std::floor(expected_mass / spawn_mass));
    const double fractional_spawn = (expected_mass / spawn_mass) - static_cast<double>(guaranteed_spawns);
    std::uint32_t spawns_this_cell = guaranteed_spawns;

    const std::uint64_t draw_u64 = hashEventSeed(
        m_config.rng_seed,
        context.step_index,
        context.rank_id,
        cell_index,
        guaranteed_spawns);
    const double draw = toUnitUniform(draw_u64);
    if (draw < fractional_spawn) {
      ++spawns_this_cell;
    }

    const double max_spawns_by_mass = std::floor(gas_mass / spawn_mass);
    spawns_this_cell = std::min(
        spawns_this_cell,
        static_cast<std::uint32_t>(std::max(0.0, max_spawns_by_mass)));

    for (std::uint32_t event_i = 0; event_i < spawns_this_cell; ++event_i) {
      state.cells.mass_code[cell_index] -= spawn_mass;
      appendStarParticle(state, cell_index, spawn_mass, context.scale_factor, particle_id++);

      ++result.counters.spawn_events;
      result.counters.consumed_gas_mass_code += spawn_mass;
      result.counters.formed_stellar_mass_code += spawn_mass;
      result.events.push_back(StarFormationEventRecord{
          .source_cell_index = cell_index,
          .spawned_particle_index = static_cast<std::uint32_t>(state.particles.size() - 1U),
          .formed_mass_code = spawn_mass,
          .formation_scale_factor = context.scale_factor,
          .spawn_probability = fractional_spawn,
          .rng_draw_u64 = draw_u64});
    }
  }

  std::array<std::uint64_t, 5> species_count{};
  for (std::uint32_t tag : state.particle_sidecar.species_tag) {
    if (tag < species_count.size()) {
      ++species_count[tag];
    }
  }
  state.species.count_by_species = species_count;
  state.rebuildSpeciesIndex();
  return result;
}

void writeStarFormationStepMetadata(
    core::SimulationState& state,
    const StarFormationStepResult& result,
    const StarFormationStepContext& context,
    const StarFormationConfig& config) {
  std::string text;
  text += "schema_version=1\n";
  text += "step_index=" + std::to_string(context.step_index) + "\n";
  text += "scale_factor=" + std::to_string(context.scale_factor) + "\n";
  text += "dt_code=" + std::to_string(context.dt_code) + "\n";
  text += "spawn_mode=" + starFormationSpawnModeToString(config.spawn_mode) + "\n";
  text += "rng_seed=" + std::to_string(config.rng_seed) + "\n";
  text += "eligible_cells=" + std::to_string(result.counters.eligible_cells) + "\n";
  text += "spawn_events=" + std::to_string(result.counters.spawn_events) + "\n";
  text += "formed_stellar_mass_code=" + std::to_string(result.counters.formed_stellar_mass_code) + "\n";
  text += "consumed_gas_mass_code=" + std::to_string(result.counters.consumed_gas_mass_code) + "\n";

  core::ModuleSidecarBlock block;
  block.module_name = "star_formation";
  block.schema_version = 1;
  block.payload = encodeUtf8(text);
  state.sidecars.upsert(std::move(block));
}

std::string starFormationSpawnModeToString(StarFormationSpawnMode mode) {
  switch (mode) {
    case StarFormationSpawnMode::kDeterministic:
      return "deterministic";
    case StarFormationSpawnMode::kStochastic:
      return "stochastic";
  }
  return "stochastic";
}

StarFormationSpawnMode starFormationSpawnModeFromString(const std::string& mode) {
  if (mode == "deterministic") {
    return StarFormationSpawnMode::kDeterministic;
  }
  if (mode == "stochastic") {
    return StarFormationSpawnMode::kStochastic;
  }
  throw std::invalid_argument("starFormationSpawnModeFromString: unsupported mode='" + mode + "'");
}

}  // namespace cosmosim::physics
