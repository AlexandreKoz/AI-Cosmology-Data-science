#include "cosmosim/physics/star_formation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cosmosim::physics {
namespace {

constexpr double k_newton_g_code = 1.0;
constexpr double k_pi = 3.14159265358979323846;
constexpr std::uint64_t k_species_star = static_cast<std::uint64_t>(core::ParticleSpecies::kStar);

[[nodiscard]] std::uint64_t mix64(std::uint64_t x) {
  x += 0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31U);
}

[[nodiscard]] double uniform01(std::uint64_t seed) {
  const std::uint64_t mantissa = mix64(seed) >> 11U;
  return static_cast<double>(mantissa) * (1.0 / 9007199254740992.0);
}

[[nodiscard]] std::string countersToText(
    const StarFormationConfig& config,
    const StarFormationCounters& counters,
    std::uint64_t step_index,
    double dt_code,
    double scale_factor) {
  std::ostringstream out;
  out << "module=star_formation\n";
  out << "schema_version=1\n";
  out << "step_index=" << step_index << '\n';
  out << "dt_code=" << dt_code << '\n';
  out << "formation_scale_factor=" << scale_factor << '\n';
  out << "spawning_model=" << config.spawning_model << '\n';
  out << "epsilon_ff=" << config.epsilon_ff << '\n';
  out << "density_threshold_code=" << config.density_threshold_code << '\n';
  out << "temperature_threshold_k=" << config.temperature_threshold_k << '\n';
  out << "virial_parameter_max=" << config.virial_parameter_max << '\n';
  out << "min_star_particle_mass_code=" << config.min_star_particle_mass_code << '\n';
  out << "random_seed_base=" << config.random_seed_base << '\n';
  out << "scanned_cells=" << counters.scanned_cells << '\n';
  out << "eligible_cells=" << counters.eligible_cells << '\n';
  out << "spawn_events=" << counters.spawn_events << '\n';
  out << "gas_mass_consumed_code=" << counters.gas_mass_consumed_code << '\n';
  out << "stellar_mass_spawned_code=" << counters.stellar_mass_spawned_code << '\n';
  return out.str();
}

}  // namespace

StarFormationModel::StarFormationModel(StarFormationConfig config) : m_config(std::move(config)) {
  if (m_config.epsilon_ff <= 0.0 || m_config.epsilon_ff > 1.0) {
    throw std::invalid_argument("StarFormationModel: epsilon_ff must be in (0, 1]");
  }
  if (m_config.density_threshold_code <= 0.0) {
    throw std::invalid_argument("StarFormationModel: density_threshold_code must be > 0");
  }
  if (m_config.temperature_threshold_k <= 0.0) {
    throw std::invalid_argument("StarFormationModel: temperature_threshold_k must be > 0");
  }
  if (m_config.virial_parameter_max <= 0.0) {
    throw std::invalid_argument("StarFormationModel: virial_parameter_max must be > 0");
  }
  if (m_config.min_star_particle_mass_code <= 0.0) {
    throw std::invalid_argument("StarFormationModel: min_star_particle_mass_code must be > 0");
  }
  if (m_config.spawning_model != "stochastic" && m_config.spawning_model != "deterministic") {
    throw std::invalid_argument("StarFormationModel: spawning_model must be 'stochastic' or 'deterministic'");
  }
  m_config.star_metallicity_mass_fraction = std::clamp(m_config.star_metallicity_mass_fraction, 0.0, 1.0);
}

const StarFormationConfig& StarFormationModel::config() const noexcept { return m_config; }

StarFormationEligibility StarFormationModel::evaluateEligibility(
    double density_code,
    double temperature_k,
    double virial_parameter) const {
  StarFormationEligibility eligibility;
  eligibility.density_pass = density_code >= m_config.density_threshold_code;
  eligibility.temperature_pass = temperature_k <= m_config.temperature_threshold_k;
  eligibility.virial_pass = virial_parameter <= m_config.virial_parameter_max;
  eligibility.eligible = eligibility.density_pass && eligibility.temperature_pass && eligibility.virial_pass;
  return eligibility;
}

StarFormationRate StarFormationModel::evaluateRate(
    double gas_density_code,
    double gas_mass_code,
    double dt_code,
    bool eligible) const {
  StarFormationRate rate;
  if (!eligible || dt_code <= 0.0 || gas_density_code <= 0.0 || gas_mass_code <= 0.0) {
    return rate;
  }

  rate.free_fall_time_code = std::sqrt((3.0 * k_pi) / (32.0 * k_newton_g_code * gas_density_code));
  rate.sfr_density_code = m_config.epsilon_ff * gas_density_code /
      std::max(rate.free_fall_time_code, std::numeric_limits<double>::min());
  rate.expected_stellar_mass_code = std::min(
      gas_mass_code,
      m_config.epsilon_ff * gas_mass_code * dt_code /
          std::max(rate.free_fall_time_code, std::numeric_limits<double>::min()));
  return rate;
}

StarFormationCounters StarFormationModel::applyToActiveGasCells(
    core::SimulationState& state,
    std::span<const std::uint32_t> active_cell_indices,
    std::span<const double> virial_parameter_by_cell,
    double dt_code,
    double formation_scale_factor,
    std::uint64_t step_index) const {
  if (!m_config.enabled || dt_code <= 0.0 || active_cell_indices.empty()) {
    return {};
  }

  if (state.cells.size() != state.gas_cells.size()) {
    throw std::invalid_argument("StarFormationModel.applyToActiveGasCells: cell and gas sidecar sizes differ");
  }

  if (!virial_parameter_by_cell.empty() && virial_parameter_by_cell.size() != state.cells.size()) {
    throw std::invalid_argument(
        "StarFormationModel.applyToActiveGasCells: virial_parameter_by_cell must match cell count");
  }

  StarFormationCounters counters;
  const std::size_t initial_particle_count = state.particles.size();

  for (std::uint32_t cell_index : active_cell_indices) {
    ++counters.scanned_cells;
    if (cell_index >= state.cells.size()) {
      throw std::out_of_range("StarFormationModel.applyToActiveGasCells: active cell index out of range");
    }

    const double virial_parameter = virial_parameter_by_cell.empty() ? 0.0 : virial_parameter_by_cell[cell_index];
    const StarFormationEligibility eligibility = evaluateEligibility(
        state.gas_cells.density_code[cell_index],
        state.gas_cells.temperature_code[cell_index],
        virial_parameter);
    if (!eligibility.eligible) {
      continue;
    }

    ++counters.eligible_cells;
    const StarFormationRate rate = evaluateRate(
        state.gas_cells.density_code[cell_index],
        state.cells.mass_code[cell_index],
        dt_code,
        true);

    if (rate.expected_stellar_mass_code <= 0.0) {
      continue;
    }

    const double gas_mass_available = state.cells.mass_code[cell_index];
    std::uint64_t spawn_count = 0;

    if (m_config.spawning_model == "deterministic") {
      if (rate.expected_stellar_mass_code >= m_config.min_star_particle_mass_code) {
        spawn_count = 1;
      }
    } else {
      const double expected_count = rate.expected_stellar_mass_code / m_config.min_star_particle_mass_code;
      spawn_count = static_cast<std::uint64_t>(std::floor(expected_count));
      const double fractional = expected_count - static_cast<double>(spawn_count);
      const std::uint64_t random_key =
          m_config.random_seed_base ^ (step_index * 0x9e3779b97f4a7c15ULL) ^
          (static_cast<std::uint64_t>(cell_index) * 0xbf58476d1ce4e5b9ULL);
      if (uniform01(random_key) < fractional) {
        ++spawn_count;
      }
    }

    if (spawn_count == 0) {
      continue;
    }

    const double target_star_mass = std::min(
        gas_mass_available,
        static_cast<double>(spawn_count) * m_config.min_star_particle_mass_code);
    if (target_star_mass <= 0.0) {
      continue;
    }

    state.cells.mass_code[cell_index] -= target_star_mass;
    counters.gas_mass_consumed_code += target_star_mass;

    const std::size_t new_particle_index = state.particles.size();
    state.resizeParticles(new_particle_index + 1);

    state.particles.position_x_comoving[new_particle_index] = state.cells.center_x_comoving[cell_index];
    state.particles.position_y_comoving[new_particle_index] = state.cells.center_y_comoving[cell_index];
    state.particles.position_z_comoving[new_particle_index] = state.cells.center_z_comoving[cell_index];
    state.particles.velocity_x_peculiar[new_particle_index] = 0.0;
    state.particles.velocity_y_peculiar[new_particle_index] = 0.0;
    state.particles.velocity_z_peculiar[new_particle_index] = 0.0;
    state.particles.mass_code[new_particle_index] = target_star_mass;
    state.particles.time_bin[new_particle_index] = state.cells.time_bin[cell_index];

    state.particle_sidecar.species_tag[new_particle_index] = static_cast<std::uint32_t>(k_species_star);
    state.particle_sidecar.particle_flags[new_particle_index] = 0U;
    state.particle_sidecar.owning_rank[new_particle_index] = 0U;
    state.particle_sidecar.sfc_key[new_particle_index] = 0ULL;
    state.particle_sidecar.particle_id[new_particle_index] =
        1'000'000ULL + static_cast<std::uint64_t>(new_particle_index);

    const std::size_t star_local_index = state.star_particles.size();
    state.star_particles.resize(star_local_index + 1);
    state.star_particles.particle_index[star_local_index] = static_cast<std::uint32_t>(new_particle_index);
    state.star_particles.formation_scale_factor[star_local_index] = formation_scale_factor;
    state.star_particles.birth_mass_code[star_local_index] = target_star_mass;
    state.star_particles.metallicity_mass_fraction[star_local_index] = m_config.star_metallicity_mass_fraction;

    ++counters.spawn_events;
    counters.stellar_mass_spawned_code += target_star_mass;
  }

  if (state.species.count_by_species.size() > k_species_star) {
    state.species.count_by_species[k_species_star] +=
        static_cast<std::uint64_t>(state.particles.size() - initial_particle_count);
  }
  state.rebuildSpeciesIndex();

  core::ModuleSidecarBlock block;
  block.module_name = "star_formation";
  block.schema_version = 1;
  const std::string payload_text = countersToText(m_config, counters, step_index, dt_code, formation_scale_factor);
  block.payload.resize(payload_text.size());
  std::memcpy(block.payload.data(), payload_text.data(), payload_text.size());
  state.sidecars.upsert(std::move(block));

  return counters;
}

}  // namespace cosmosim::physics
