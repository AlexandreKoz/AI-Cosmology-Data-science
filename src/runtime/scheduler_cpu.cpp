#include "cosmosim/runtime/scheduler_cpu.hpp"

#include <cstddef>
#include <stdexcept>

namespace cosmosim::runtime {

void drift_kick_drift_step(cosmosim::core::SimState& state, const double dt_code) {
  if (!cosmosim::core::validate_hot_layout(state.particle_species.hot_dm)) {
    throw std::runtime_error("Invalid SoA layout: field lengths mismatch");
  }

  const std::size_t n = cosmosim::core::active_particle_count(state.particle_species.hot_dm);

#pragma omp parallel for if(n > 4096)
  for (std::size_t i = 0; i < n; ++i) {
    state.particle_species.hot_dm.x_comoving_mpc[i] += state.particle_species.hot_dm.vx_peculiar_kms[i] *
                                                       static_cast<float>(dt_code);
    state.particle_species.hot_dm.y_comoving_mpc[i] += state.particle_species.hot_dm.vy_peculiar_kms[i] *
                                                       static_cast<float>(dt_code);
    state.particle_species.hot_dm.z_comoving_mpc[i] += state.particle_species.hot_dm.vz_peculiar_kms[i] *
                                                       static_cast<float>(dt_code);
  }
}

} // namespace cosmosim::runtime
