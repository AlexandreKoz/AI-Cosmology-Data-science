#include "cosmosim/core/soa_layout.hpp"

namespace cosmosim::core {

bool validate_hot_layout(const ParticleHotSoA& particles) {
  const std::size_t n = particles.x_comoving_mpc.size();
  return particles.y_comoving_mpc.size() == n &&
         particles.z_comoving_mpc.size() == n &&
         particles.vx_peculiar_kms.size() == n &&
         particles.vy_peculiar_kms.size() == n &&
         particles.vz_peculiar_kms.size() == n &&
         particles.mass_code.size() == n;
}

std::size_t active_particle_count(const ParticleHotSoA& particles) {
  return particles.x_comoving_mpc.size();
}

} // namespace cosmosim::core
