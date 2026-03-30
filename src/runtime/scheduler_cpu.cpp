#include "cosmosim/runtime/scheduler_cpu.hpp"

#include <stdexcept>

namespace cosmosim::runtime {

void drift_kick_drift_step(cosmosim::core::SimState& state, const double dt_code) {
  if (!cosmosim::core::validate_hot_layout(state.hot_particles)) {
    throw std::runtime_error("Invalid SoA layout: field lengths mismatch");
  }

  const auto n = cosmosim::core::active_particle_count(state.hot_particles);

  #pragma omp parallel for if(n > 4096)
  for (std::size_t i = 0; i < n; ++i) {
    state.hot_particles.x_comoving_mpc[i] += state.hot_particles.vx_peculiar_kms[i] * dt_code;
    state.hot_particles.y_comoving_mpc[i] += state.hot_particles.vy_peculiar_kms[i] * dt_code;
    state.hot_particles.z_comoving_mpc[i] += state.hot_particles.vz_peculiar_kms[i] * dt_code;
  }
}

} // namespace cosmosim::runtime
