#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "cosmosim/core/time_integration.hpp"

namespace {

constexpr double k_tolerance = 1.0e-12;

class StageRecorder final : public cosmosim::core::IntegrationCallback {
 public:
  std::string_view callbackName() const override { return "stage_recorder"; }

  void onStage(cosmosim::core::StepContext& context) override {
    observed_stages.push_back(context.stage);
  }

  std::vector<cosmosim::core::IntegrationStage> observed_stages;
};

void testKickDriftKickOrdering() {
  cosmosim::core::SimulationState state;
  cosmosim::core::IntegratorState integrator_state;
  integrator_state.dt_time_code = 1.0;

  StageRecorder recorder;
  cosmosim::core::StepOrchestrator orchestrator;
  orchestrator.registerCallback(recorder);

  const cosmosim::core::ActiveSetDescriptor active_set{};
  orchestrator.executeSingleStep(state, integrator_state, active_set, nullptr, nullptr);

  const auto expected = cosmosim::core::StageScheduler::kickDriftKickOrder();
  assert(recorder.observed_stages.size() == expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    assert(recorder.observed_stages[i] == expected[i]);
  }
}

void testCosmologyHelpers() {
  cosmosim::core::CosmologyBackgroundConfig cfg;
  cfg.hubble_param = 0.7;
  cfg.omega_matter = 1.0;
  cfg.omega_lambda = 0.0;
  cfg.omega_radiation = 0.0;
  cfg.omega_curvature = 0.0;

  cosmosim::core::LambdaCdmBackground background(cfg);

  const double a0 = 0.5;
  const double a1 = 1.0;
  const double drift = cosmosim::core::computeComovingDriftFactor(background, a0, a1, 2000);

  // For Einstein-de Sitter: H(a)=H0*a^{-3/2} => integral da/(a^2 H(a)) = 2*(sqrt(a1)-sqrt(a0))/H0.
  const double h0 = background.hubble0Si();
  const double expected = 2.0 * (std::sqrt(a1) - std::sqrt(a0)) / h0;
  assert(std::abs(drift - expected) / expected < 5.0e-4);

  const double drag = cosmosim::core::computeHubbleDragFactor(a0, a1);
  assert(std::abs(drag - 0.5) < k_tolerance);

  const double delta_a = 1.0e-4;
  const double dt = cosmosim::core::estimateDeltaTimeFromScaleFactorStep(background, a0, delta_a);
  const double a_next = cosmosim::core::advanceScaleFactorEuler(background, a0, dt);
  assert(std::abs(a_next - (a0 + delta_a)) / delta_a < 1.0e-12);
}

void testActiveSubsetDetection() {
  std::vector<std::uint32_t> active_particles = {0, 2, 4};
  std::vector<std::uint32_t> active_cells = {1, 3};

  cosmosim::core::ActiveSetDescriptor active_set{
      .particle_indices = active_particles,
      .cell_indices = active_cells,
      .particles_are_subset = true,
      .cells_are_subset = true,
  };

  assert(active_set.hasParticleSubset(10));
  assert(active_set.hasCellSubset(8));
  assert(!active_set.hasParticleSubset(3));
  assert(!active_set.hasCellSubset(2));
}

}  // namespace

int main() {
  testKickDriftKickOrdering();
  testCosmologyHelpers();
  testActiveSubsetDetection();
  return 0;
}
