#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cosmosim::core {

struct ParticleHotSoA {
  std::vector<double> x_comoving_mpc;
  std::vector<double> y_comoving_mpc;
  std::vector<double> z_comoving_mpc;
  std::vector<double> vx_peculiar_kms;
  std::vector<double> vy_peculiar_kms;
  std::vector<double> vz_peculiar_kms;
  std::vector<double> mass_code;
};

struct ParticleColdSidecar {
  std::vector<std::uint64_t> persistent_id;
  std::vector<std::uint8_t> provenance_flags;
};

[[nodiscard]] bool validate_hot_layout(const ParticleHotSoA& particles);
[[nodiscard]] std::size_t active_particle_count(const ParticleHotSoA& particles);

} // namespace cosmosim::core
