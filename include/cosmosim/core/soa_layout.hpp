#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace cosmosim::core {

enum class FidelityMode : std::uint8_t {
  baseline = 0,
  flagship = 1,
};

struct HotParticleSoA {
  std::vector<float> x_comoving_mpc;
  std::vector<float> y_comoving_mpc;
  std::vector<float> z_comoving_mpc;
  std::vector<float> vx_peculiar_kms;
  std::vector<float> vy_peculiar_kms;
  std::vector<float> vz_peculiar_kms;
  std::vector<float> mass_code;
  std::vector<float> eps_grav_comoving_mpc;
};

struct HotGasCellSoA {
  std::vector<float> x_comoving_mpc;
  std::vector<float> y_comoving_mpc;
  std::vector<float> z_comoving_mpc;
  std::vector<float> vx_peculiar_kms;
  std::vector<float> vy_peculiar_kms;
  std::vector<float> vz_peculiar_kms;
  std::vector<float> mass_code;
  std::vector<float> rho_code;
  std::vector<float> u_thermal_code;
  std::vector<float> smoothing_length_comoving_mpc;
};

struct HotStarSoA {
  std::vector<float> x_comoving_mpc;
  std::vector<float> y_comoving_mpc;
  std::vector<float> z_comoving_mpc;
  std::vector<float> vx_peculiar_kms;
  std::vector<float> vy_peculiar_kms;
  std::vector<float> vz_peculiar_kms;
  std::vector<float> mass_code;
};

struct HotBhSoA {
  std::vector<float> x_comoving_mpc;
  std::vector<float> y_comoving_mpc;
  std::vector<float> z_comoving_mpc;
  std::vector<float> vx_peculiar_kms;
  std::vector<float> vy_peculiar_kms;
  std::vector<float> vz_peculiar_kms;
  std::vector<float> mass_code;
  std::vector<float> accretion_rate_code;
};

struct ColdEntitySidecar {
  std::vector<std::uint64_t> persistent_id;
  std::vector<std::uint32_t> restart_slot;
  std::vector<std::uint16_t> io_group_code;
  std::vector<std::uint8_t> provenance_flags;
};

struct OptionalModuleSidecars {
  std::vector<float> grav_ax_code;
  std::vector<float> grav_ay_code;
  std::vector<float> grav_az_code;

  std::vector<float> chemistry_metallicity_code;
  std::vector<float> mhd_bx_code;
  std::vector<float> mhd_by_code;
  std::vector<float> mhd_bz_code;

  std::vector<std::uint8_t> feedback_flags;
};

struct ActiveSetIndex {
  std::vector<std::uint32_t> particle_dm_index;
  std::vector<std::uint32_t> cell_gas_index;
  std::vector<std::uint32_t> particle_star_index;
  std::vector<std::uint32_t> particle_bh_index;
};

struct GravityKernelView {
  std::span<const std::uint32_t> particle_dm_index;
  std::span<const float> x_comoving_mpc;
  std::span<const float> y_comoving_mpc;
  std::span<const float> z_comoving_mpc;
  std::span<const float> mass_code;
};

struct HydroKernelView {
  std::span<const std::uint32_t> cell_gas_index;
  std::span<const float> rho_code;
  std::span<const float> u_thermal_code;
  std::span<const float> vx_peculiar_kms;
  std::span<const float> vy_peculiar_kms;
  std::span<const float> vz_peculiar_kms;
};

struct AmrKernelView {
  std::span<const std::uint32_t> cell_gas_index;
  std::span<const float> rho_code;
  std::span<const float> smoothing_length_comoving_mpc;
};

struct IoSnapshotView {
  std::span<const std::uint64_t> persistent_id;
  std::span<const float> x_comoving_mpc;
  std::span<const float> y_comoving_mpc;
  std::span<const float> z_comoving_mpc;
  std::span<const float> vx_peculiar_kms;
  std::span<const float> vy_peculiar_kms;
  std::span<const float> vz_peculiar_kms;
  std::span<const float> mass_code;
};

struct AnalysisView {
  std::span<const std::uint64_t> persistent_id;
  std::span<const std::uint32_t> restart_slot;
  std::span<const float> mass_code;
};

struct EntityByteBudget {
  std::size_t hot_bytes_baseline = 0;
  std::size_t cold_bytes_baseline = 0;
  std::size_t sidecar_bytes_baseline = 0;
  std::size_t total_bytes_baseline = 0;

  std::size_t hot_bytes_flagship = 0;
  std::size_t cold_bytes_flagship = 0;
  std::size_t sidecar_bytes_flagship = 0;
  std::size_t total_bytes_flagship = 0;
};

[[nodiscard]] bool validate_hot_layout(const HotParticleSoA& particles);
[[nodiscard]] bool validate_hot_layout(const HotGasCellSoA& cells);
[[nodiscard]] bool validate_hot_layout(const HotStarSoA& stars);
[[nodiscard]] bool validate_hot_layout(const HotBhSoA& bhs);
[[nodiscard]] bool validate_cold_layout(const ColdEntitySidecar& cold_sidecar, std::size_t expected_size);

[[nodiscard]] std::size_t active_particle_count(const HotParticleSoA& particles);
[[nodiscard]] std::size_t active_cell_count(const HotGasCellSoA& cells);

[[nodiscard]] EntityByteBudget dm_byte_budget();
[[nodiscard]] EntityByteBudget gas_byte_budget();
[[nodiscard]] EntityByteBudget star_byte_budget();
[[nodiscard]] EntityByteBudget bh_byte_budget();

} // namespace cosmosim::core
