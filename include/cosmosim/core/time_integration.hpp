#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "cosmosim/core/cosmology.hpp"
#include "cosmosim/core/simulation_state.hpp"

namespace cosmosim::core {

// Explicit step stage contract shared by gravity, hydro, physics, analysis, and I/O modules.
enum class IntegrationStage : std::uint8_t {
  kGravityKickPre = 0,
  kDrift = 1,
  kHydroUpdate = 2,
  kSourceTerms = 3,
  kGravityKickPost = 4,
  kAnalysisHooks = 5,
  kOutputCheck = 6,
};

[[nodiscard]] std::string_view integrationStageName(IntegrationStage stage);

// Baseline stepping family; hierarchical bins can reuse the same stage contract.
enum class TimeStepScheme : std::uint8_t {
  kKickDriftKick = 0,
};

// Hierarchical stepping metadata kept outside particle arrays for auditable ownership.
struct TimeBinContext {
  bool hierarchical_enabled = false;
  std::uint8_t active_bin = 0;
  std::uint8_t max_bin = 0;
};

// Persistent integrator state tracked by the orchestrator.
struct IntegratorState {
  double current_time_code = 0.0;
  double current_scale_factor = 1.0;
  double dt_time_code = 0.0;
  std::uint64_t step_index = 0;
  TimeStepScheme scheme = TimeStepScheme::kKickDriftKick;
  TimeBinContext time_bins;
};

// Explicit compact active-set descriptor with optional subset spans.
struct ActiveSetDescriptor {
  std::span<const std::uint32_t> particle_indices;
  std::span<const std::uint32_t> cell_indices;
  bool particles_are_subset = false;
  bool cells_are_subset = false;

  [[nodiscard]] bool hasParticleSubset(std::size_t total_particle_count) const noexcept;
  [[nodiscard]] bool hasCellSubset(std::size_t total_cell_count) const noexcept;
};

// Per-stage execution context passed to callbacks without mutating interface contracts.
struct StepContext {
  SimulationState& state;
  IntegratorState& integrator_state;
  ActiveSetDescriptor active_set;
  TransientStepWorkspace* workspace = nullptr;
  const LambdaCdmBackground* cosmology_background = nullptr;
  IntegrationStage stage = IntegrationStage::kGravityKickPre;
};

// Callback interface implemented by gravity, hydro, source, analysis, and output modules.
class IntegrationCallback {
 public:
  virtual ~IntegrationCallback() = default;

  [[nodiscard]] virtual std::string_view callbackName() const = 0;
  virtual void onStage(StepContext& context) = 0;
};

// Stage scheduler isolates ordering from solver implementation details.
class StageScheduler {
 public:
  [[nodiscard]] std::vector<IntegrationStage> schedule(
      const IntegratorState& integrator_state,
      const ActiveSetDescriptor& active_set) const;

  [[nodiscard]] static std::span<const IntegrationStage> kickDriftKickOrder();
};

// Single authoritative step orchestrator for current baseline stepping.
class StepOrchestrator {
 public:
  explicit StepOrchestrator(StageScheduler scheduler = {});

  void registerCallback(IntegrationCallback& callback);
  [[nodiscard]] std::size_t callbackCount() const noexcept;

  void executeSingleStep(
      SimulationState& state,
      IntegratorState& integrator_state,
      ActiveSetDescriptor active_set,
      const LambdaCdmBackground* cosmology_background,
      TransientStepWorkspace* workspace = nullptr) const;

 private:
  StageScheduler m_scheduler;
  std::vector<IntegrationCallback*> m_callbacks;
};

// da/dt = a H(a) for standard FLRW backgrounds.
[[nodiscard]] double computeScaleFactorRate(const LambdaCdmBackground& background, double scale_factor);

// Forward-Euler helper used by baseline tests and scheduler scaffolding.
[[nodiscard]] double advanceScaleFactorEuler(
    const LambdaCdmBackground& background,
    double scale_factor,
    double dt_time_code);

// dt estimate for an intended delta-a increment around the current scale factor.
[[nodiscard]] double estimateDeltaTimeFromScaleFactorStep(
    const LambdaCdmBackground& background,
    double scale_factor,
    double delta_scale_factor);

// Drift prefactor integral: integral_{a0}^{a1} da / (a^2 H(a)).
[[nodiscard]] double computeComovingDriftFactor(
    const LambdaCdmBackground& background,
    double scale_factor_begin,
    double scale_factor_end,
    std::uint32_t midpoint_samples = 16);

// Kick prefactor for comoving acceleration terms proportional to 1/a.
[[nodiscard]] double computeComovingKickFactor(
    const LambdaCdmBackground& background,
    double scale_factor_begin,
    double scale_factor_end,
    std::uint32_t midpoint_samples = 16);

// Hubble drag factor for dv/dt = -H(a) v over [a0, a1].
[[nodiscard]] double computeHubbleDragFactor(double scale_factor_begin, double scale_factor_end);

}  // namespace cosmosim::core
