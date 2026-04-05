#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cosmosim::core {

enum class ParticleSpecies : std::uint8_t {
  dark_matter = 0,
  gas = 1,
  star = 2,
  black_hole = 3,
};

constexpr std::array<ParticleSpecies, 4> k_particle_species_order = {
    ParticleSpecies::dark_matter,
    ParticleSpecies::gas,
    ParticleSpecies::star,
    ParticleSpecies::black_hole,
};

std::string_view particleSpeciesName(ParticleSpecies species);

struct SimulationMetadata {
  std::uint32_t schema_version = 1;
  std::string run_name;
  std::string config_digest;
  std::string initial_conditions_source;
  std::string normalized_config_text;
  std::string provenance_stamp;

  [[nodiscard]] std::string normalizedDump() const;
};

struct ParticleSoa {
  std::vector<double> position_x_comoving;
  std::vector<double> position_y_comoving;
  std::vector<double> position_z_comoving;
  std::vector<double> velocity_x_peculiar;
  std::vector<double> velocity_y_peculiar;
  std::vector<double> velocity_z_peculiar;
  std::vector<double> mass_code;

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const;
};

struct ParticleSidecar {
  std::vector<std::uint64_t> particle_id;
  std::vector<std::uint32_t> flags;

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const;
};

class ParticleSpeciesState {
 public:
  explicit ParticleSpeciesState(ParticleSpecies species = ParticleSpecies::dark_matter);

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] bool validateOwnershipInvariant() const;
  [[nodiscard]] ParticleSpecies species() const;

  ParticleSoa hot;
  ParticleSidecar cold;

 private:
  ParticleSpecies m_species;
};

struct CellSoa {
  std::vector<double> density_comoving;
  std::vector<double> pressure_code;
  std::vector<double> internal_energy_code;
  std::vector<std::uint32_t> level;

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const;
};

struct PatchSoa {
  std::vector<std::uint64_t> patch_id;
  std::vector<std::uint32_t> refinement_level;
  std::vector<std::uint32_t> parent_patch_local_index;

  void resize(std::size_t count);
  [[nodiscard]] std::size_t size() const;
};

class ActiveIndexSet {
 public:
  void clear();
  void reserve(std::size_t count);
  void assign(std::span<const std::uint32_t> compact_indices);
  [[nodiscard]] std::span<const std::uint32_t> view() const;

 private:
  std::vector<std::uint32_t> m_compact_indices;
};

class ScratchAllocator {
 public:
  virtual ~ScratchAllocator() = default;
  virtual std::span<std::byte> acquire(std::size_t bytes, std::size_t alignment) = 0;
  virtual void reset() = 0;
};

class MonotonicScratchAllocator final : public ScratchAllocator {
 public:
  explicit MonotonicScratchAllocator(std::size_t initial_capacity_bytes = 0);

  std::span<std::byte> acquire(std::size_t bytes, std::size_t alignment) override;
  void reset() override;
  [[nodiscard]] std::size_t committedBytes() const;

 private:
  std::vector<std::byte> m_storage;
  std::size_t m_cursor_bytes = 0;
};

struct ModuleSidecar {
  std::string module_name;
  std::size_t element_bytes = 0;
  std::vector<std::byte> payload;

  void resizeElements(std::size_t element_count);
  [[nodiscard]] std::size_t elementCount() const;
};

class SimulationState {
 public:
  SimulationState();

  [[nodiscard]] ParticleSpeciesState& speciesState(ParticleSpecies species);
  [[nodiscard]] const ParticleSpeciesState& speciesState(ParticleSpecies species) const;

  [[nodiscard]] bool validateOwnershipInvariants() const;

  [[nodiscard]] ModuleSidecar* findModuleSidecar(std::string_view module_name);
  [[nodiscard]] const ModuleSidecar* findModuleSidecar(std::string_view module_name) const;
  ModuleSidecar& ensureModuleSidecar(std::string module_name, std::size_t element_bytes);

  void clearTransientViews();

  SimulationMetadata metadata;
  CellSoa cells;
  PatchSoa patches;
  ActiveIndexSet active_particle_local_indices;
  ActiveIndexSet active_cell_local_indices;

 private:
  static std::size_t speciesArrayIndex(ParticleSpecies species);

  std::array<ParticleSpeciesState, k_particle_species_order.size()> m_species_state;
  std::unordered_map<std::string, ModuleSidecar> m_module_sidecars;
};

struct StepWorkspace {
  explicit StepWorkspace(std::unique_ptr<ScratchAllocator> allocator_in = nullptr);

  std::unique_ptr<ScratchAllocator> allocator;
  std::vector<std::uint32_t> gather_map;
  std::vector<double> staging_scalar;

  void clearForNextStep();
};

}  // namespace cosmosim::core
