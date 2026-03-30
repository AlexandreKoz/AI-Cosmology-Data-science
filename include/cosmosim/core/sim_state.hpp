#pragma once

#include "cosmosim/core/soa_layout.hpp"

namespace cosmosim::core {

struct SimState {
  ParticleHotSoA hot_particles;
  ParticleColdSidecar cold_particles;
  double scale_factor_a = 1.0;
  double hubble_H_code = 0.0;
};

} // namespace cosmosim::core
