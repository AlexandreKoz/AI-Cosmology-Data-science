#include "cosmosim/core/soa_layout.hpp"

namespace cosmosim::core {

namespace {

[[nodiscard]] bool has_common_size(const std::size_t expected_size,
                                   const std::initializer_list<std::size_t> field_sizes) {
  for (const std::size_t field_size : field_sizes) {
    if (field_size != expected_size) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] EntityByteBudget make_budget(const std::size_t hot_baseline,
                                           const std::size_t cold_baseline,
                                           const std::size_t sidecar_baseline,
                                           const std::size_t hot_flagship,
                                           const std::size_t cold_flagship,
                                           const std::size_t sidecar_flagship) {
  EntityByteBudget budget{};
  budget.hot_bytes_baseline = hot_baseline;
  budget.cold_bytes_baseline = cold_baseline;
  budget.sidecar_bytes_baseline = sidecar_baseline;
  budget.total_bytes_baseline = hot_baseline + cold_baseline + sidecar_baseline;
  budget.hot_bytes_flagship = hot_flagship;
  budget.cold_bytes_flagship = cold_flagship;
  budget.sidecar_bytes_flagship = sidecar_flagship;
  budget.total_bytes_flagship = hot_flagship + cold_flagship + sidecar_flagship;
  return budget;
}

} // namespace

bool validate_hot_layout(const HotParticleSoA& particles) {
  const std::size_t n = particles.x_comoving_mpc.size();
  return has_common_size(
      n,
      {
          particles.y_comoving_mpc.size(),
          particles.z_comoving_mpc.size(),
          particles.vx_peculiar_kms.size(),
          particles.vy_peculiar_kms.size(),
          particles.vz_peculiar_kms.size(),
          particles.mass_code.size(),
          particles.eps_grav_comoving_mpc.size(),
      });
}

bool validate_hot_layout(const HotGasCellSoA& cells) {
  const std::size_t n = cells.x_comoving_mpc.size();
  return has_common_size(
      n,
      {
          cells.y_comoving_mpc.size(),
          cells.z_comoving_mpc.size(),
          cells.vx_peculiar_kms.size(),
          cells.vy_peculiar_kms.size(),
          cells.vz_peculiar_kms.size(),
          cells.mass_code.size(),
          cells.rho_code.size(),
          cells.u_thermal_code.size(),
          cells.smoothing_length_comoving_mpc.size(),
      });
}

bool validate_hot_layout(const HotStarSoA& stars) {
  const std::size_t n = stars.x_comoving_mpc.size();
  return has_common_size(
      n,
      {
          stars.y_comoving_mpc.size(),
          stars.z_comoving_mpc.size(),
          stars.vx_peculiar_kms.size(),
          stars.vy_peculiar_kms.size(),
          stars.vz_peculiar_kms.size(),
          stars.mass_code.size(),
      });
}

bool validate_hot_layout(const HotBhSoA& bhs) {
  const std::size_t n = bhs.x_comoving_mpc.size();
  return has_common_size(
      n,
      {
          bhs.y_comoving_mpc.size(),
          bhs.z_comoving_mpc.size(),
          bhs.vx_peculiar_kms.size(),
          bhs.vy_peculiar_kms.size(),
          bhs.vz_peculiar_kms.size(),
          bhs.mass_code.size(),
          bhs.accretion_rate_code.size(),
      });
}

bool validate_cold_layout(const ColdEntitySidecar& cold_sidecar, const std::size_t expected_size) {
  return has_common_size(
      expected_size,
      {
          cold_sidecar.persistent_id.size(),
          cold_sidecar.restart_slot.size(),
          cold_sidecar.io_group_code.size(),
          cold_sidecar.provenance_flags.size(),
      });
}

std::size_t active_particle_count(const HotParticleSoA& particles) {
  return particles.x_comoving_mpc.size();
}

std::size_t active_cell_count(const HotGasCellSoA& cells) {
  return cells.x_comoving_mpc.size();
}

EntityByteBudget dm_byte_budget() {
  constexpr std::size_t hot_baseline = (8U * sizeof(float));
  constexpr std::size_t cold_baseline = sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                        sizeof(std::uint16_t) + sizeof(std::uint8_t);
  constexpr std::size_t sidecar_baseline = 3U * sizeof(float);

  constexpr std::size_t hot_flagship = hot_baseline;
  constexpr std::size_t cold_flagship = cold_baseline;
  constexpr std::size_t sidecar_flagship = (3U * sizeof(float)) + sizeof(float) + sizeof(std::uint8_t);

  return make_budget(hot_baseline,
                     cold_baseline,
                     sidecar_baseline,
                     hot_flagship,
                     cold_flagship,
                     sidecar_flagship);
}

EntityByteBudget gas_byte_budget() {
  constexpr std::size_t hot_baseline = (10U * sizeof(float));
  constexpr std::size_t cold_baseline = sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                        sizeof(std::uint16_t) + sizeof(std::uint8_t);
  constexpr std::size_t sidecar_baseline = sizeof(float);

  constexpr std::size_t hot_flagship = hot_baseline;
  constexpr std::size_t cold_flagship = cold_baseline;
  constexpr std::size_t sidecar_flagship = (4U * sizeof(float)) + sizeof(std::uint8_t);

  return make_budget(hot_baseline,
                     cold_baseline,
                     sidecar_baseline,
                     hot_flagship,
                     cold_flagship,
                     sidecar_flagship);
}

EntityByteBudget star_byte_budget() {
  constexpr std::size_t hot_baseline = (7U * sizeof(float));
  constexpr std::size_t cold_baseline = sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                        sizeof(std::uint16_t) + sizeof(std::uint8_t);
  constexpr std::size_t sidecar_baseline = sizeof(float);

  constexpr std::size_t hot_flagship = hot_baseline;
  constexpr std::size_t cold_flagship = cold_baseline;
  constexpr std::size_t sidecar_flagship = (sizeof(float) + sizeof(std::uint8_t));

  return make_budget(hot_baseline,
                     cold_baseline,
                     sidecar_baseline,
                     hot_flagship,
                     cold_flagship,
                     sidecar_flagship);
}

EntityByteBudget bh_byte_budget() {
  constexpr std::size_t hot_baseline = (8U * sizeof(float));
  constexpr std::size_t cold_baseline = sizeof(std::uint64_t) + sizeof(std::uint32_t) +
                                        sizeof(std::uint16_t) + sizeof(std::uint8_t);
  constexpr std::size_t sidecar_baseline = sizeof(std::uint8_t);

  constexpr std::size_t hot_flagship = hot_baseline;
  constexpr std::size_t cold_flagship = cold_baseline;
  constexpr std::size_t sidecar_flagship = sizeof(float) + sizeof(std::uint8_t);

  return make_budget(hot_baseline,
                     cold_baseline,
                     sidecar_baseline,
                     hot_flagship,
                     cold_flagship,
                     sidecar_flagship);
}

} // namespace cosmosim::core
