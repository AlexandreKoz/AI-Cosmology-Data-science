#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cosmosim/core/soa_storage.hpp"

namespace cosmosim::core {

// Canonical species tags used in sidecar accounting and invariant checks.
enum class ParticleSpecies : std::uint8_t {
  kDarkMatter = 0,
  kGas = 1,
  kStar = 2,
  kBlackHole = 3,
  kTracer = 4,
};

struct ParticleSoa {
  // Hot particle fields (solver-facing): comoving positions + peculiar velocities.
  AlignedVector<double> position_x_comoving;
  AlignedVector<double> position_y_comoving;
  AlignedVector<double> position_z_comoving;
  AlignedVector<double> velocity_x_peculiar;
  AlignedVector<double> velocity_y_peculiar;
  AlignedVector<double> velocity_z_peculiar;
  AlignedVector<double> mass_code;
  AlignedVector<double> internal_energy_code;

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool isConsistent() const noexcept;
};

struct ParticleSidecar {
  // Cold particle metadata sidecar: IDs, species ownership, and flags.
  AlignedVector<std::uint64_t> particle_id;
  AlignedVector<std::uint32_t> species_tag;
  AlignedVector<std::uint32_t> particle_flags;
  AlignedVector<std::uint32_t> owning_rank;

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool isConsistent() const noexcept;
};

struct CellSoa {
  // Hot Eulerian cell state for finite-volume hydrodynamics.
  AlignedVector<double> density_code;
  AlignedVector<double> pressure_code;
  AlignedVector<double> velocity_x_peculiar;
  AlignedVector<double> velocity_y_peculiar;
  AlignedVector<double> velocity_z_peculiar;
  AlignedVector<std::uint32_t> patch_index;

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool isConsistent() const noexcept;
};

struct PatchSoa {
  // AMR patch descriptors and contiguous cell ranges [first_cell, first_cell + cell_count).
  AlignedVector<std::uint64_t> patch_id;
  AlignedVector<std::int32_t> level;
  AlignedVector<std::uint32_t> first_cell;
  AlignedVector<std::uint32_t> cell_count;

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool isConsistent() const noexcept;
};

struct SpeciesContainer {
  // Explicit species counts; used as an auditable ownership ledger.
  std::array<std::uint64_t, 5> count_by_species{};

  [[nodiscard]] std::uint64_t totalCount() const noexcept;
  [[nodiscard]] bool isConsistentWith(const ParticleSidecar& sidecar) const noexcept;
};

struct StateMetadata {
  // Schema/provenance fields that must remain stable across restart/snapshot workflows.
  std::uint32_t schema_version = 1;
  std::string run_name = "cosmosim_run";
  std::uint64_t normalized_config_hash = 0;
  std::string normalized_config_hash_hex;
  std::uint64_t step_index = 0;
  double scale_factor = 1.0;
  std::string snapshot_stem = "snapshot";
  std::string restart_stem = "restart";

  [[nodiscard]] std::string serialize() const;
  [[nodiscard]] static StateMetadata deserialize(std::string_view text);
};

struct ModuleSidecarBlock {
  // Opaque module payload with an independent schema version.
  std::string module_name;
  std::uint32_t schema_version = 1;
  std::vector<std::byte> payload;
};

class ModuleSidecarRegistry {
 public:
  // Insert or replace sidecar payload for a module.
  void upsert(ModuleSidecarBlock block);
  [[nodiscard]] const ModuleSidecarBlock* find(std::string_view module_name) const;
  [[nodiscard]] std::size_t size() const noexcept;

 private:
  std::unordered_map<std::string, ModuleSidecarBlock> m_sidecars;
};

class SimulationState {
 public:
  // Single ownership root for persistent run state.
  ParticleSoa particles;
  ParticleSidecar particle_sidecar;
  CellSoa cells;
  PatchSoa patches;
  SpeciesContainer species;
  StateMetadata metadata;
  ModuleSidecarRegistry sidecars;

  void resizeParticles(std::size_t count);
  void resizeCells(std::size_t count);
  void resizePatches(std::size_t count);

  [[nodiscard]] bool validateOwnershipInvariants() const;
};

struct ActiveIndexSet {
  // Per-step compact active index lists assembled by scheduler/rung logic.
  std::vector<std::uint32_t> particle_indices;
  std::vector<std::uint32_t> cell_indices;

  void clear();
};

struct ParticleActiveView {
  // Compact contiguous particle spans materialized in the transient workspace.
  std::span<const std::uint64_t> particle_id;
  std::span<const std::uint32_t> species_tag;
  std::span<const double> position_x_comoving;
  std::span<const double> position_y_comoving;
  std::span<const double> position_z_comoving;
  std::span<const double> velocity_x_peculiar;
  std::span<const double> velocity_y_peculiar;
  std::span<const double> velocity_z_peculiar;
  std::span<const double> mass_code;

  [[nodiscard]] std::size_t size() const noexcept;
};

struct CellActiveView {
  // Compact contiguous cell spans materialized in the transient workspace.
  std::span<const double> density_code;
  std::span<const double> pressure_code;
  std::span<const double> velocity_x_peculiar;
  std::span<const double> velocity_y_peculiar;
  std::span<const double> velocity_z_peculiar;
  std::span<const std::uint32_t> patch_index;

  [[nodiscard]] std::size_t size() const noexcept;
};

class ScratchAllocator {
 public:
  virtual ~ScratchAllocator() = default;
  [[nodiscard]] virtual std::byte* allocateBytes(std::size_t bytes, std::size_t alignment) = 0;
  virtual void reset() = 0;

  template <typename T>
  [[nodiscard]] T* allocateArray(std::size_t count) {
    auto* raw = allocateBytes(sizeof(T) * count, alignof(T));
    return reinterpret_cast<T*>(raw);
  }
};

class MonotonicScratchAllocator final : public ScratchAllocator {
 public:
  // Growable monotonic byte arena for transient per-step scratch data.
  explicit MonotonicScratchAllocator(std::size_t initial_capacity_bytes = 0);

  [[nodiscard]] std::byte* allocateBytes(std::size_t bytes, std::size_t alignment) override;
  void reset() override;

  [[nodiscard]] std::size_t capacityBytes() const noexcept;

 private:
  std::vector<std::byte> m_storage;
  std::size_t m_offset_bytes = 0;
};

struct TransientStepWorkspace {
  // Compact particle active-set buffers.
  AlignedVector<std::uint64_t> particle_id;
  AlignedVector<std::uint32_t> particle_species_tag;
  AlignedVector<double> particle_position_x_comoving;
  AlignedVector<double> particle_position_y_comoving;
  AlignedVector<double> particle_position_z_comoving;
  AlignedVector<double> particle_velocity_x_peculiar;
  AlignedVector<double> particle_velocity_y_peculiar;
  AlignedVector<double> particle_velocity_z_peculiar;
  AlignedVector<double> particle_mass_code;

  // Compact cell active-set buffers.
  AlignedVector<double> cell_density_code;
  AlignedVector<double> cell_pressure_code;
  AlignedVector<double> cell_velocity_x_peculiar;
  AlignedVector<double> cell_velocity_y_peculiar;
  AlignedVector<double> cell_velocity_z_peculiar;
  AlignedVector<std::uint32_t> cell_patch_index;

  // Monotonic scratch arena reused between steps via reset().
  MonotonicScratchAllocator scratch;

  void clear();
};

[[nodiscard]] ParticleActiveView buildParticleActiveView(
    const SimulationState& state,
    std::span<const std::uint32_t> active_particle_indices,
    TransientStepWorkspace& workspace);

[[nodiscard]] CellActiveView buildCellActiveView(
    const SimulationState& state,
    std::span<const std::uint32_t> active_cell_indices,
    TransientStepWorkspace& workspace);


}  // namespace cosmosim::core
