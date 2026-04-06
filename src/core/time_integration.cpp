#include "cosmosim/core/time_integration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace cosmosim::core {
namespace {

constexpr std::array<IntegrationStage, 7> k_kick_drift_kick_order = {
    IntegrationStage::kGravityKickPre,
    IntegrationStage::kDrift,
    IntegrationStage::kHydroUpdate,
    IntegrationStage::kSourceTerms,
    IntegrationStage::kGravityKickPost,
    IntegrationStage::kAnalysisHooks,
    IntegrationStage::kOutputCheck,
};

[[nodiscard]] double midpointIntegrateDriftLike(
    const LambdaCdmBackground& background,
    double scale_factor_begin,
    double scale_factor_end,
    std::uint32_t midpoint_samples) {
  if (scale_factor_begin <= 0.0 || scale_factor_end <= 0.0) {
    throw std::invalid_argument("scale factors must be positive for drift-like integrals");
  }
  if (scale_factor_end < scale_factor_begin) {
    throw std::invalid_argument("scale_factor_end must be >= scale_factor_begin");
  }

  const std::uint32_t samples = std::max<std::uint32_t>(midpoint_samples, 1U);
  const double delta_a = (scale_factor_end - scale_factor_begin) / static_cast<double>(samples);
  if (delta_a == 0.0) {
    return 0.0;
  }

  double accum = 0.0;
  for (std::uint32_t i = 0; i < samples; ++i) {
    const double a_mid = scale_factor_begin + (static_cast<double>(i) + 0.5) * delta_a;
    accum += 1.0 / (a_mid * a_mid * background.hubbleSi(a_mid));
  }
  return accum * delta_a;
}

}  // namespace

std::string_view integrationStageName(IntegrationStage stage) {
  switch (stage) {
    case IntegrationStage::kGravityKickPre:
      return "gravity_kick_pre";
    case IntegrationStage::kDrift:
      return "drift";
    case IntegrationStage::kHydroUpdate:
      return "hydro_update";
    case IntegrationStage::kSourceTerms:
      return "source_terms";
    case IntegrationStage::kGravityKickPost:
      return "gravity_kick_post";
    case IntegrationStage::kAnalysisHooks:
      return "analysis_hooks";
    case IntegrationStage::kOutputCheck:
      return "output_check";
  }
  return "unknown";
}

bool ActiveSetDescriptor::hasParticleSubset(std::size_t total_particle_count) const noexcept {
  return particles_are_subset && particle_indices.size() < total_particle_count;
}

bool ActiveSetDescriptor::hasCellSubset(std::size_t total_cell_count) const noexcept {
  return cells_are_subset && cell_indices.size() < total_cell_count;
}

std::vector<IntegrationStage> StageScheduler::schedule(
    const IntegratorState& integrator_state,
    const ActiveSetDescriptor& /*active_set*/) const {
  if (integrator_state.scheme != TimeStepScheme::kKickDriftKick) {
    throw std::invalid_argument("unsupported timestep scheme");
  }

  return std::vector<IntegrationStage>(k_kick_drift_kick_order.begin(), k_kick_drift_kick_order.end());
}

std::span<const IntegrationStage> StageScheduler::kickDriftKickOrder() { return k_kick_drift_kick_order; }

StepOrchestrator::StepOrchestrator(StageScheduler scheduler) : m_scheduler(std::move(scheduler)) {}

void StepOrchestrator::registerCallback(IntegrationCallback& callback) { m_callbacks.push_back(&callback); }

std::size_t StepOrchestrator::callbackCount() const noexcept { return m_callbacks.size(); }

void StepOrchestrator::executeSingleStep(
    SimulationState& state,
    IntegratorState& integrator_state,
    ActiveSetDescriptor active_set,
    const LambdaCdmBackground* cosmology_background,
    TransientStepWorkspace* workspace) const {
  if (integrator_state.dt_time_code <= 0.0) {
    throw std::invalid_argument("dt_time_code must be positive");
  }

  StepContext context{
      .state = state,
      .integrator_state = integrator_state,
      .active_set = active_set,
      .workspace = workspace,
      .cosmology_background = cosmology_background,
      .stage = IntegrationStage::kGravityKickPre,
  };

  const auto ordered_stages = m_scheduler.schedule(integrator_state, active_set);
  for (const auto stage : ordered_stages) {
    context.stage = stage;
    for (auto* callback : m_callbacks) {
      callback->onStage(context);
    }
  }

  integrator_state.current_time_code += integrator_state.dt_time_code;
  if (cosmology_background != nullptr) {
    integrator_state.current_scale_factor = advanceScaleFactorEuler(
        *cosmology_background,
        integrator_state.current_scale_factor,
        integrator_state.dt_time_code);
  }
  ++integrator_state.step_index;
}

double computeScaleFactorRate(const LambdaCdmBackground& background, double scale_factor) {
  if (scale_factor <= 0.0) {
    throw std::invalid_argument("scale_factor must be positive");
  }
  return scale_factor * background.hubbleSi(scale_factor);
}

double advanceScaleFactorEuler(
    const LambdaCdmBackground& background,
    double scale_factor,
    double dt_time_code) {
  if (dt_time_code < 0.0) {
    throw std::invalid_argument("dt_time_code must be non-negative");
  }
  return scale_factor + dt_time_code * computeScaleFactorRate(background, scale_factor);
}

double estimateDeltaTimeFromScaleFactorStep(
    const LambdaCdmBackground& background,
    double scale_factor,
    double delta_scale_factor) {
  if (delta_scale_factor < 0.0) {
    throw std::invalid_argument("delta_scale_factor must be non-negative");
  }

  const double rate = computeScaleFactorRate(background, scale_factor);
  if (rate <= 0.0) {
    throw std::invalid_argument("scale-factor rate must be positive");
  }
  return delta_scale_factor / rate;
}

double computeComovingDriftFactor(
    const LambdaCdmBackground& background,
    double scale_factor_begin,
    double scale_factor_end,
    std::uint32_t midpoint_samples) {
  return midpointIntegrateDriftLike(background, scale_factor_begin, scale_factor_end, midpoint_samples);
}

double computeComovingKickFactor(
    const LambdaCdmBackground& background,
    double scale_factor_begin,
    double scale_factor_end,
    std::uint32_t midpoint_samples) {
  return midpointIntegrateDriftLike(background, scale_factor_begin, scale_factor_end, midpoint_samples);
}

double computeHubbleDragFactor(double scale_factor_begin, double scale_factor_end) {
  if (scale_factor_begin <= 0.0 || scale_factor_end <= 0.0) {
    throw std::invalid_argument("scale factors must be positive");
  }
  if (scale_factor_end < scale_factor_begin) {
    throw std::invalid_argument("scale_factor_end must be >= scale_factor_begin");
  }

  return scale_factor_begin / scale_factor_end;
}

}  // namespace cosmosim::core
