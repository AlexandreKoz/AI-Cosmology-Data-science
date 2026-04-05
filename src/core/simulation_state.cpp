#include "cosmosim/core/simulation_state.hpp"

#include <cassert>
#include <sstream>
#include <stdexcept>

namespace cosmosim::core {

std::string_view particleSpeciesName(ParticleSpecies species) {
  switch (species) {
    case ParticleSpecies::dark_matter:
      return "dark_matter";
    case ParticleSpecies::gas:
      return "gas";
    case ParticleSpecies::star:
      return "star";
    case ParticleSpecies::black_hole:
      return "black_hole";
  }
  return "unknown";
}

std::string SimulationMetadata::normalizedDump() const {
  std::ostringstream output;
  output << "schema_version=" << schema_version << '\n';
  output << "run_name=" << run_name << '\n';
  output << "config_digest=" << config_digest << '\n';
  output << "initial_conditions_source=" << initial_conditions_source << '\n';
  output << "provenance_stamp=" << provenance_stamp << '\n';
  output << "normalized_config_text=" << normalized_config_text << '\n';
  return output.str();
}

void ParticleSoa::resize(std::size_t count) {
  position_x_comoving.resize(count);
  position_y_comoving.resize(count);
  position_z_comoving.resize(count);
  velocity_x_peculiar.resize(count);
  velocity_y_peculiar.resize(count);
  velocity_z_peculiar.resize(count);
  mass_code.resize(count);
}

std::size_t ParticleSoa::size() const { return position_x_comoving.size(); }

void ParticleSidecar::resize(std::size_t count) {
  particle_id.resize(count);
  flags.resize(count);
}

std::size_t ParticleSidecar::size() const { return particle_id.size(); }

ParticleSpeciesState::ParticleSpeciesState(ParticleSpecies species) : m_species(species) {}

void ParticleSpeciesState::resize(std::size_t count) {
  hot.resize(count);
  cold.resize(count);
}

std::size_t ParticleSpeciesState::size() const { return hot.size(); }

bool ParticleSpeciesState::validateOwnershipInvariant() const {
  const auto count = hot.size();
  return hot.position_y_comoving.size() == count && hot.position_z_comoving.size() == count &&
         hot.velocity_x_peculiar.size() == count && hot.velocity_y_peculiar.size() == count &&
         hot.velocity_z_peculiar.size() == count && hot.mass_code.size() == count &&
         cold.particle_id.size() == count && cold.flags.size() == count;
}

ParticleSpecies ParticleSpeciesState::species() const { return m_species; }

void CellSoa::resize(std::size_t count) {
  density_comoving.resize(count);
  pressure_code.resize(count);
  internal_energy_code.resize(count);
  level.resize(count);
}

std::size_t CellSoa::size() const { return density_comoving.size(); }

void PatchSoa::resize(std::size_t count) {
  patch_id.resize(count);
  refinement_level.resize(count);
  parent_patch_local_index.resize(count);
}

std::size_t PatchSoa::size() const { return patch_id.size(); }

void ActiveIndexSet::clear() { m_compact_indices.clear(); }

void ActiveIndexSet::reserve(std::size_t count) { m_compact_indices.reserve(count); }

void ActiveIndexSet::assign(std::span<const std::uint32_t> compact_indices) {
  m_compact_indices.assign(compact_indices.begin(), compact_indices.end());
}

std::span<const std::uint32_t> ActiveIndexSet::view() const {
  return std::span<const std::uint32_t>(m_compact_indices.data(), m_compact_indices.size());
}

MonotonicScratchAllocator::MonotonicScratchAllocator(std::size_t initial_capacity_bytes)
    : m_storage(initial_capacity_bytes) {}

std::span<std::byte> MonotonicScratchAllocator::acquire(std::size_t bytes, std::size_t alignment) {
  const auto mask = alignment - 1;
  const auto aligned_cursor = (m_cursor_bytes + mask) & ~mask;
  const auto required = aligned_cursor + bytes;
  if (required > m_storage.size()) {
    m_storage.resize(required);
  }
  m_cursor_bytes = required;
  return std::span<std::byte>(m_storage.data() + aligned_cursor, bytes);
}

void MonotonicScratchAllocator::reset() { m_cursor_bytes = 0; }

std::size_t MonotonicScratchAllocator::committedBytes() const { return m_cursor_bytes; }

void ModuleSidecar::resizeElements(std::size_t element_count) {
  payload.resize(element_count * element_bytes);
}

std::size_t ModuleSidecar::elementCount() const {
  if (element_bytes == 0) {
    return 0;
  }
  return payload.size() / element_bytes;
}

SimulationState::SimulationState()
    : m_species_state({ParticleSpeciesState(ParticleSpecies::dark_matter),
                       ParticleSpeciesState(ParticleSpecies::gas),
                       ParticleSpeciesState(ParticleSpecies::star),
                       ParticleSpeciesState(ParticleSpecies::black_hole)}) {}

ParticleSpeciesState& SimulationState::speciesState(ParticleSpecies species) {
  return m_species_state.at(speciesArrayIndex(species));
}

const ParticleSpeciesState& SimulationState::speciesState(ParticleSpecies species) const {
  return m_species_state.at(speciesArrayIndex(species));
}

bool SimulationState::validateOwnershipInvariants() const {
  const bool cells_ok = cells.pressure_code.size() == cells.size() &&
                        cells.internal_energy_code.size() == cells.size() &&
                        cells.level.size() == cells.size();
  const bool patches_ok = patches.refinement_level.size() == patches.size() &&
                          patches.parent_patch_local_index.size() == patches.size();
  const bool species_ok = std::all_of(m_species_state.begin(), m_species_state.end(),
                                      [](const ParticleSpeciesState& state) {
                                        return state.validateOwnershipInvariant();
                                      });
  return cells_ok && patches_ok && species_ok;
}

ModuleSidecar* SimulationState::findModuleSidecar(std::string_view module_name) {
  const auto it = m_module_sidecars.find(std::string(module_name));
  if (it == m_module_sidecars.end()) {
    return nullptr;
  }
  return &it->second;
}

const ModuleSidecar* SimulationState::findModuleSidecar(std::string_view module_name) const {
  const auto it = m_module_sidecars.find(std::string(module_name));
  if (it == m_module_sidecars.end()) {
    return nullptr;
  }
  return &it->second;
}

ModuleSidecar& SimulationState::ensureModuleSidecar(std::string module_name,
                                                    std::size_t element_bytes) {
  auto [it, inserted] = m_module_sidecars.emplace(std::move(module_name), ModuleSidecar{});
  if (inserted) {
    it->second.module_name = it->first;
    it->second.element_bytes = element_bytes;
  } else if (it->second.element_bytes != element_bytes) {
    throw std::runtime_error("module sidecar element_bytes mismatch");
  }
  return it->second;
}

void SimulationState::clearTransientViews() {
  active_particle_local_indices.clear();
  active_cell_local_indices.clear();
}

std::size_t SimulationState::speciesArrayIndex(ParticleSpecies species) {
  const auto value = static_cast<std::size_t>(species);
  assert(value < k_particle_species_order.size());
  return value;
}

StepWorkspace::StepWorkspace(std::unique_ptr<ScratchAllocator> allocator_in)
    : allocator(std::move(allocator_in)) {
  if (!allocator) {
    allocator = std::make_unique<MonotonicScratchAllocator>();
  }
}

void StepWorkspace::clearForNextStep() {
  gather_map.clear();
  staging_scalar.clear();
  allocator->reset();
}

}  // namespace cosmosim::core
