#include "cosmosim/core/soa_layout.hpp"

#include <cassert>

int main() {
  cosmosim::core::ParticleHotSoA particles{};
  particles.x_comoving_mpc = {0.0, 1.0};
  particles.y_comoving_mpc = {0.0, 1.0};
  particles.z_comoving_mpc = {0.0, 1.0};
  particles.vx_peculiar_kms = {0.0, 0.0};
  particles.vy_peculiar_kms = {0.0, 0.0};
  particles.vz_peculiar_kms = {0.0, 0.0};
  particles.mass_code = {1.0, 1.0};

  assert(cosmosim::core::validate_hot_layout(particles));
  assert(cosmosim::core::active_particle_count(particles) == 2);

  particles.mass_code.pop_back();
  assert(!cosmosim::core::validate_hot_layout(particles));
  return 0;
}
