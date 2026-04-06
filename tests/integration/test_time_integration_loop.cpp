#include <cassert>
#include <cmath>

#include "cosmosim/core/time_integration.hpp"

namespace {

class GravityKickMock final : public cosmosim::core::IntegrationCallback {
 public:
  std::string_view callbackName() const override { return "gravity_kick_mock"; }

  void onStage(cosmosim::core::StepContext& context) override {
    if (context.stage != cosmosim::core::IntegrationStage::kGravityKickPre &&
        context.stage != cosmosim::core::IntegrationStage::kGravityKickPost) {
      return;
    }

    auto& state = context.state;
    const double kick = 0.25 * context.integrator_state.dt_time_code;
    for (std::size_t i = 0; i < state.particles.size(); ++i) {
      state.particles.velocity_x_peculiar[i] += kick;
    }
  }
};

void runNoPhysicsLoop() {
  cosmosim::core::SimulationState state;
  cosmosim::core::IntegratorState integrator_state;
  integrator_state.dt_time_code = 0.1;
  integrator_state.current_scale_factor = 1.0;

  cosmosim::core::StepOrchestrator orchestrator;
  for (int step = 0; step < 5; ++step) {
    orchestrator.executeSingleStep(state, integrator_state, {}, nullptr, nullptr);
  }

  assert(std::abs(integrator_state.current_time_code - 0.5) < 1.0e-12);
  assert(integrator_state.step_index == 5U);
  assert(std::abs(integrator_state.current_scale_factor - 1.0) < 1.0e-12);
}

void runGravityOnlyLoop() {
  cosmosim::core::SimulationState state;
  state.resizeParticles(4);

  for (std::size_t i = 0; i < state.particles.size(); ++i) {
    state.particles.velocity_x_peculiar[i] = 0.0;
  }

  cosmosim::core::IntegratorState integrator_state;
  integrator_state.dt_time_code = 5.0e16;
  integrator_state.current_scale_factor = 1.0;

  cosmosim::core::CosmologyBackgroundConfig cfg;
  cfg.hubble_param = 0.7;
  cfg.omega_matter = 0.3;
  cfg.omega_lambda = 0.7;
  cosmosim::core::LambdaCdmBackground background(cfg);

  GravityKickMock gravity_kick;
  cosmosim::core::StepOrchestrator orchestrator;
  orchestrator.registerCallback(gravity_kick);

  for (int step = 0; step < 4; ++step) {
    orchestrator.executeSingleStep(state, integrator_state, {}, &background, nullptr);
  }

  // Two kick stages per step with 0.25*dt each => net velocity increment per step is 0.5*dt.
  const double expected_velocity = 4.0 * 0.5 * integrator_state.dt_time_code;
  for (std::size_t i = 0; i < state.particles.size(); ++i) {
    assert(std::abs(state.particles.velocity_x_peculiar[i] - expected_velocity) < 1.0e-12);
  }

  assert(integrator_state.current_time_code > 0.0);
  assert(integrator_state.current_scale_factor > 1.0);
}

}  // namespace

int main() {
  runNoPhysicsLoop();
  runGravityOnlyLoop();
  return 0;
}
