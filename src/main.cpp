#include "cosmosim/core/sim_state.hpp"
#include "cosmosim/runtime/scheduler_cpu.hpp"
#include "cosmosim/utils/provenance.hpp"

#include <iostream>

int main() {
  cosmosim::core::SimState state{};
  std::cout << "CosmoSim bootstrap: " << cosmosim::utils::build_provenance_tag() << '\n';
  cosmosim::runtime::drift_kick_drift_step(state, 0.0);
  return 0;
}
