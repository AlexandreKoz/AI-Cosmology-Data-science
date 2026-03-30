#pragma once

#include "cosmosim/core/sim_state.hpp"

namespace cosmosim::runtime {

void drift_kick_drift_step(cosmosim::core::SimState& state, double dt_code);

} // namespace cosmosim::runtime
