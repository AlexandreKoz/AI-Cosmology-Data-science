#include "cosmosim/hydro/hydro_core_solver.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace cosmosim::hydro {
namespace {

constexpr double k_small = 1.0e-14;

[[nodiscard]] double dot3(double ax, double ay, double az, double bx, double by, double bz) {
  return ax * bx + ay * by + az * bz;
}

[[nodiscard]] double norm3(double x, double y, double z) {
  return std::sqrt(dot3(x, y, z, x, y, z));
}

[[nodiscard]] HydroConservedState eulerPhysicalFlux(
    const HydroPrimitiveState& primitive,
    const HydroFace& face,
    double adiabatic_index) {
  (void)adiabatic_index;
  const double mass_density = primitive.rho_comoving;
  const double vel_n = dot3(
      primitive.vel_x_peculiar,
      primitive.vel_y_peculiar,
      primitive.vel_z_peculiar,
      face.normal_x,
      face.normal_y,
      face.normal_z);

  HydroConservedState flux;
  flux.mass_density_comoving = mass_density * vel_n;
  flux.momentum_density_x_comoving = mass_density * primitive.vel_x_peculiar * vel_n + primitive.pressure_comoving * face.normal_x;
  flux.momentum_density_y_comoving = mass_density * primitive.vel_y_peculiar * vel_n + primitive.pressure_comoving * face.normal_y;
  flux.momentum_density_z_comoving = mass_density * primitive.vel_z_peculiar * vel_n + primitive.pressure_comoving * face.normal_z;

  const HydroConservedState conserved = HydroCoreSolver::conservedFromPrimitive(primitive, adiabatic_index);
  flux.total_energy_density_comoving = (conserved.total_energy_density_comoving + primitive.pressure_comoving) * vel_n;
  return flux;
}

[[nodiscard]] std::array<double, 3> gravityAtCell(
    std::size_t cell_index,
    std::span<const double> ax,
    std::span<const double> ay,
    std::span<const double> az) {
  const double gx = (cell_index < ax.size()) ? ax[cell_index] : 0.0;
  const double gy = (cell_index < ay.size()) ? ay[cell_index] : 0.0;
  const double gz = (cell_index < az.size()) ? az[cell_index] : 0.0;
  return {gx, gy, gz};
}

void validateAdvanceInputs(
    const HydroConservedStateSoa& conserved,
    const HydroPatchGeometry& geometry,
    const HydroUpdateContext& update,
    const HydroSourceContext& source_context,
    double adiabatic_index) {
  if (conserved.size() == 0) {
    throw std::invalid_argument("Hydro advance requires at least one cell");
  }
  if (geometry.cell_volume_comoving <= 0.0) {
    throw std::invalid_argument("Hydro advance requires positive cell_volume_comoving");
  }
  if (update.dt_code <= 0.0) {
    throw std::invalid_argument("Hydro advance requires dt_code > 0");
  }
  if (update.scale_factor <= 0.0) {
    throw std::invalid_argument("Hydro advance requires scale_factor > 0");
  }
  if (adiabatic_index <= 1.0) {
    throw std::invalid_argument("Hydro solver requires adiabatic_index > 1");
  }
  if (std::abs(source_context.update.dt_code - update.dt_code) > 1.0e-16 ||
      std::abs(source_context.update.scale_factor - update.scale_factor) > 1.0e-16 ||
      std::abs(source_context.update.hubble_rate_code - update.hubble_rate_code) > 1.0e-16) {
    throw std::invalid_argument("Hydro source_context.update must match update passed to advancePatch");
  }

  for (const HydroFace& face : geometry.faces) {
    if (face.owner_cell >= conserved.size()) {
      throw std::invalid_argument("Hydro face owner index is out of range");
    }
    if (face.neighbor_cell != k_invalid_cell_index && face.neighbor_cell >= conserved.size()) {
      throw std::invalid_argument("Hydro face neighbor index is out of range");
    }
    if (face.area_comoving <= 0.0) {
      throw std::invalid_argument("Hydro face area_comoving must be positive");
    }
    const double normal_norm = norm3(face.normal_x, face.normal_y, face.normal_z);
    if (std::abs(normal_norm - 1.0) > 1.0e-10) {
      throw std::invalid_argument("Hydro face normal must be unit length");
    }
  }
}

void validateActiveSet(const HydroActiveSetView& active_set, const HydroConservedStateSoa& conserved, const HydroPatchGeometry& geometry) {
  if (active_set.active_cells.empty()) {
    throw std::invalid_argument("Hydro active set requires at least one active cell");
  }
  if (active_set.active_faces.empty()) {
    throw std::invalid_argument("Hydro active set requires at least one active face");
  }

  for (std::size_t cell_index : active_set.active_cells) {
    if (cell_index >= conserved.size()) {
      throw std::invalid_argument("Hydro active cell index is out of range");
    }
  }
  for (std::size_t face_index : active_set.active_faces) {
    if (face_index >= geometry.faces.size()) {
      throw std::invalid_argument("Hydro active face index is out of range");
    }
  }
}

void fillPrimitiveCache(
    const HydroConservedStateSoa& conserved,
    const HydroActiveSetView& active_set,
    double adiabatic_index,
    HydroPrimitiveCacheSoa& primitive_cache) {
  if (primitive_cache.size() != conserved.size()) {
    primitive_cache.resize(conserved.size());
  }

  for (std::size_t cell_index : active_set.active_cells) {
    primitive_cache.storeCell(
        cell_index,
        HydroCoreSolver::primitiveFromConserved(conserved.loadCell(cell_index), adiabatic_index));
  }
}

}  // namespace

HydroConservedState& HydroConservedState::operator+=(const HydroConservedState& rhs) {
  mass_density_comoving += rhs.mass_density_comoving;
  momentum_density_x_comoving += rhs.momentum_density_x_comoving;
  momentum_density_y_comoving += rhs.momentum_density_y_comoving;
  momentum_density_z_comoving += rhs.momentum_density_z_comoving;
  total_energy_density_comoving += rhs.total_energy_density_comoving;
  return *this;
}

HydroConservedState& HydroConservedState::operator-=(const HydroConservedState& rhs) {
  mass_density_comoving -= rhs.mass_density_comoving;
  momentum_density_x_comoving -= rhs.momentum_density_x_comoving;
  momentum_density_y_comoving -= rhs.momentum_density_y_comoving;
  momentum_density_z_comoving -= rhs.momentum_density_z_comoving;
  total_energy_density_comoving -= rhs.total_energy_density_comoving;
  return *this;
}

HydroConservedState operator+(HydroConservedState lhs, const HydroConservedState& rhs) {
  lhs += rhs;
  return lhs;
}

HydroConservedState operator-(HydroConservedState lhs, const HydroConservedState& rhs) {
  lhs -= rhs;
  return lhs;
}

HydroConservedState operator*(double scalar, HydroConservedState state) {
  state.mass_density_comoving *= scalar;
  state.momentum_density_x_comoving *= scalar;
  state.momentum_density_y_comoving *= scalar;
  state.momentum_density_z_comoving *= scalar;
  state.total_energy_density_comoving *= scalar;
  return state;
}

HydroConservedStateSoa::HydroConservedStateSoa(std::size_t cell_count)
    : m_mass_density_comoving(cell_count, 0.0),
      m_momentum_density_x_comoving(cell_count, 0.0),
      m_momentum_density_y_comoving(cell_count, 0.0),
      m_momentum_density_z_comoving(cell_count, 0.0),
      m_total_energy_density_comoving(cell_count, 0.0) {}

void HydroConservedStateSoa::resize(std::size_t cell_count) {
  m_mass_density_comoving.resize(cell_count, 0.0);
  m_momentum_density_x_comoving.resize(cell_count, 0.0);
  m_momentum_density_y_comoving.resize(cell_count, 0.0);
  m_momentum_density_z_comoving.resize(cell_count, 0.0);
  m_total_energy_density_comoving.resize(cell_count, 0.0);
}

std::size_t HydroConservedStateSoa::size() const {
  return m_mass_density_comoving.size();
}

HydroConservedState HydroConservedStateSoa::loadCell(std::size_t cell_index) const {
  return HydroConservedState{
      .mass_density_comoving = m_mass_density_comoving.at(cell_index),
      .momentum_density_x_comoving = m_momentum_density_x_comoving.at(cell_index),
      .momentum_density_y_comoving = m_momentum_density_y_comoving.at(cell_index),
      .momentum_density_z_comoving = m_momentum_density_z_comoving.at(cell_index),
      .total_energy_density_comoving = m_total_energy_density_comoving.at(cell_index)};
}

void HydroConservedStateSoa::storeCell(std::size_t cell_index, const HydroConservedState& cell_state) {
  m_mass_density_comoving.at(cell_index) = cell_state.mass_density_comoving;
  m_momentum_density_x_comoving.at(cell_index) = cell_state.momentum_density_x_comoving;
  m_momentum_density_y_comoving.at(cell_index) = cell_state.momentum_density_y_comoving;
  m_momentum_density_z_comoving.at(cell_index) = cell_state.momentum_density_z_comoving;
  m_total_energy_density_comoving.at(cell_index) = cell_state.total_energy_density_comoving;
}

std::span<double> HydroConservedStateSoa::massDensityComoving() { return m_mass_density_comoving; }
std::span<const double> HydroConservedStateSoa::massDensityComoving() const { return m_mass_density_comoving; }
std::span<double> HydroConservedStateSoa::momentumDensityXComoving() { return m_momentum_density_x_comoving; }
std::span<const double> HydroConservedStateSoa::momentumDensityXComoving() const { return m_momentum_density_x_comoving; }
std::span<double> HydroConservedStateSoa::momentumDensityYComoving() { return m_momentum_density_y_comoving; }
std::span<const double> HydroConservedStateSoa::momentumDensityYComoving() const { return m_momentum_density_y_comoving; }
std::span<double> HydroConservedStateSoa::momentumDensityZComoving() { return m_momentum_density_z_comoving; }
std::span<const double> HydroConservedStateSoa::momentumDensityZComoving() const { return m_momentum_density_z_comoving; }
std::span<double> HydroConservedStateSoa::totalEnergyDensityComoving() { return m_total_energy_density_comoving; }
std::span<const double> HydroConservedStateSoa::totalEnergyDensityComoving() const { return m_total_energy_density_comoving; }

HydroPrimitiveCacheSoa::HydroPrimitiveCacheSoa(std::size_t cell_count)
    : m_rho_comoving(cell_count, 0.0),
      m_vel_x_peculiar(cell_count, 0.0),
      m_vel_y_peculiar(cell_count, 0.0),
      m_vel_z_peculiar(cell_count, 0.0),
      m_pressure_comoving(cell_count, 0.0) {}

void HydroPrimitiveCacheSoa::resize(std::size_t cell_count) {
  m_rho_comoving.resize(cell_count, 0.0);
  m_vel_x_peculiar.resize(cell_count, 0.0);
  m_vel_y_peculiar.resize(cell_count, 0.0);
  m_vel_z_peculiar.resize(cell_count, 0.0);
  m_pressure_comoving.resize(cell_count, 0.0);
}

std::size_t HydroPrimitiveCacheSoa::size() const {
  return m_rho_comoving.size();
}

HydroPrimitiveState HydroPrimitiveCacheSoa::loadCell(std::size_t cell_index) const {
  return HydroPrimitiveState{
      .rho_comoving = m_rho_comoving.at(cell_index),
      .vel_x_peculiar = m_vel_x_peculiar.at(cell_index),
      .vel_y_peculiar = m_vel_y_peculiar.at(cell_index),
      .vel_z_peculiar = m_vel_z_peculiar.at(cell_index),
      .pressure_comoving = m_pressure_comoving.at(cell_index)};
}

void HydroPrimitiveCacheSoa::storeCell(std::size_t cell_index, const HydroPrimitiveState& primitive_state) {
  m_rho_comoving.at(cell_index) = primitive_state.rho_comoving;
  m_vel_x_peculiar.at(cell_index) = primitive_state.vel_x_peculiar;
  m_vel_y_peculiar.at(cell_index) = primitive_state.vel_y_peculiar;
  m_vel_z_peculiar.at(cell_index) = primitive_state.vel_z_peculiar;
  m_pressure_comoving.at(cell_index) = primitive_state.pressure_comoving;
}

bool HydroReconstruction::reconstructFaceFromCache(
    const HydroPrimitiveCacheSoa& primitive_cache,
    const HydroFace& face,
    HydroPrimitiveState& left_state,
    HydroPrimitiveState& right_state) const {
  (void)primitive_cache;
  (void)face;
  (void)left_state;
  (void)right_state;
  return false;
}

HydroConservedState ComovingGravityExpansionSource::sourceForCell(
    std::size_t cell_index,
    const HydroConservedState& conserved,
    const HydroPrimitiveState& primitive,
    const HydroSourceContext& context) const {
  const std::array<double, 3> g = gravityAtCell(
      cell_index,
      context.gravity_accel_x_peculiar,
      context.gravity_accel_y_peculiar,
      context.gravity_accel_z_peculiar);

  const double hubble_rate = context.update.hubble_rate_code;
  const double internal_energy_density = std::max(
      conserved.total_energy_density_comoving -
          0.5 * (conserved.momentum_density_x_comoving * conserved.momentum_density_x_comoving +
                 conserved.momentum_density_y_comoving * conserved.momentum_density_y_comoving +
                 conserved.momentum_density_z_comoving * conserved.momentum_density_z_comoving) /
              std::max(conserved.mass_density_comoving, k_small),
      0.0);

  HydroConservedState source;
  source.mass_density_comoving = 0.0;
  source.momentum_density_x_comoving = primitive.rho_comoving * g[0] - hubble_rate * conserved.momentum_density_x_comoving;
  source.momentum_density_y_comoving = primitive.rho_comoving * g[1] - hubble_rate * conserved.momentum_density_y_comoving;
  source.momentum_density_z_comoving = primitive.rho_comoving * g[2] - hubble_rate * conserved.momentum_density_z_comoving;

  const double work_gravity = primitive.rho_comoving *
      (primitive.vel_x_peculiar * g[0] + primitive.vel_y_peculiar * g[1] + primitive.vel_z_peculiar * g[2]);
  source.total_energy_density_comoving = work_gravity - 2.0 * hubble_rate * internal_energy_density;
  return source;
}

void PiecewiseConstantReconstruction::reconstructFace(
    const HydroConservedStateSoa& conserved,
    const HydroFace& face,
    HydroPrimitiveState& left_state,
    HydroPrimitiveState& right_state,
    double adiabatic_index) const {
  left_state = HydroCoreSolver::primitiveFromConserved(conserved.loadCell(face.owner_cell), adiabatic_index);
  if (face.neighbor_cell == k_invalid_cell_index) {
    right_state = left_state;
    return;
  }
  right_state = HydroCoreSolver::primitiveFromConserved(conserved.loadCell(face.neighbor_cell), adiabatic_index);
}

bool PiecewiseConstantReconstruction::reconstructFaceFromCache(
    const HydroPrimitiveCacheSoa& primitive_cache,
    const HydroFace& face,
    HydroPrimitiveState& left_state,
    HydroPrimitiveState& right_state) const {
  left_state = primitive_cache.loadCell(face.owner_cell);
  if (face.neighbor_cell == k_invalid_cell_index) {
    right_state = left_state;
    return true;
  }
  right_state = primitive_cache.loadCell(face.neighbor_cell);
  return true;
}

HydroConservedState HlleRiemannSolver::computeFlux(
    const HydroPrimitiveState& left_state,
    const HydroPrimitiveState& right_state,
    const HydroFace& face,
    double adiabatic_index) const {
  const HydroConservedState u_left = HydroCoreSolver::conservedFromPrimitive(left_state, adiabatic_index);
  const HydroConservedState u_right = HydroCoreSolver::conservedFromPrimitive(right_state, adiabatic_index);

  const HydroConservedState f_left = eulerPhysicalFlux(left_state, face, adiabatic_index);
  const HydroConservedState f_right = eulerPhysicalFlux(right_state, face, adiabatic_index);

  const double vel_n_left = dot3(
      left_state.vel_x_peculiar,
      left_state.vel_y_peculiar,
      left_state.vel_z_peculiar,
      face.normal_x,
      face.normal_y,
      face.normal_z);
  const double vel_n_right = dot3(
      right_state.vel_x_peculiar,
      right_state.vel_y_peculiar,
      right_state.vel_z_peculiar,
      face.normal_x,
      face.normal_y,
      face.normal_z);

  const double c_left = std::sqrt(adiabatic_index * left_state.pressure_comoving / std::max(left_state.rho_comoving, k_small));
  const double c_right = std::sqrt(adiabatic_index * right_state.pressure_comoving / std::max(right_state.rho_comoving, k_small));

  const double s_left = std::min(vel_n_left - c_left, vel_n_right - c_right);
  const double s_right = std::max(vel_n_left + c_left, vel_n_right + c_right);

  if (s_left >= 0.0) {
    return f_left;
  }
  if (s_right <= 0.0) {
    return f_right;
  }

  HydroConservedState flux;
  const double inv_speed_delta = 1.0 / std::max(s_right - s_left, k_small);
  flux.mass_density_comoving =
      (s_right * f_left.mass_density_comoving - s_left * f_right.mass_density_comoving +
       s_left * s_right * (u_right.mass_density_comoving - u_left.mass_density_comoving)) *
      inv_speed_delta;
  flux.momentum_density_x_comoving =
      (s_right * f_left.momentum_density_x_comoving - s_left * f_right.momentum_density_x_comoving +
       s_left * s_right * (u_right.momentum_density_x_comoving - u_left.momentum_density_x_comoving)) *
      inv_speed_delta;
  flux.momentum_density_y_comoving =
      (s_right * f_left.momentum_density_y_comoving - s_left * f_right.momentum_density_y_comoving +
       s_left * s_right * (u_right.momentum_density_y_comoving - u_left.momentum_density_y_comoving)) *
      inv_speed_delta;
  flux.momentum_density_z_comoving =
      (s_right * f_left.momentum_density_z_comoving - s_left * f_right.momentum_density_z_comoving +
       s_left * s_right * (u_right.momentum_density_z_comoving - u_left.momentum_density_z_comoving)) *
      inv_speed_delta;
  flux.total_energy_density_comoving =
      (s_right * f_left.total_energy_density_comoving - s_left * f_right.total_energy_density_comoving +
       s_left * s_right * (u_right.total_energy_density_comoving - u_left.total_energy_density_comoving)) *
      inv_speed_delta;
  return flux;
}

HydroCoreSolver::HydroCoreSolver(double adiabatic_index) : m_adiabatic_index(adiabatic_index) {
  if (m_adiabatic_index <= 1.0) {
    throw std::invalid_argument("HydroCoreSolver requires adiabatic_index > 1");
  }
}

double HydroCoreSolver::adiabaticIndex() const {
  return m_adiabatic_index;
}

void HydroScratchBuffers::resize(std::size_t cell_count, std::size_t active_face_count) {
  cell_delta.assign(cell_count, HydroConservedState{});
  left_states.resize(active_face_count);
  right_states.resize(active_face_count);
  fluxes.resize(active_face_count);
}

HydroConservedState HydroCoreSolver::conservedFromPrimitive(
    const HydroPrimitiveState& primitive,
    double adiabatic_index) {
  if (primitive.rho_comoving <= 0.0) {
    throw std::invalid_argument("Primitive state requires rho_comoving > 0");
  }
  if (primitive.pressure_comoving <= 0.0) {
    throw std::invalid_argument("Primitive state requires pressure_comoving > 0");
  }

  const double velocity_squared =
      primitive.vel_x_peculiar * primitive.vel_x_peculiar +
      primitive.vel_y_peculiar * primitive.vel_y_peculiar +
      primitive.vel_z_peculiar * primitive.vel_z_peculiar;

  HydroConservedState conserved;
  conserved.mass_density_comoving = primitive.rho_comoving;
  conserved.momentum_density_x_comoving = primitive.rho_comoving * primitive.vel_x_peculiar;
  conserved.momentum_density_y_comoving = primitive.rho_comoving * primitive.vel_y_peculiar;
  conserved.momentum_density_z_comoving = primitive.rho_comoving * primitive.vel_z_peculiar;
  conserved.total_energy_density_comoving =
      primitive.pressure_comoving / (adiabatic_index - 1.0) + 0.5 * primitive.rho_comoving * velocity_squared;
  return conserved;
}

HydroPrimitiveState HydroCoreSolver::primitiveFromConserved(
    const HydroConservedState& conserved,
    double adiabatic_index) {
  if (conserved.mass_density_comoving <= 0.0) {
    throw std::invalid_argument("Conserved state requires mass_density_comoving > 0");
  }

  const double inv_rho = 1.0 / conserved.mass_density_comoving;
  HydroPrimitiveState primitive;
  primitive.rho_comoving = conserved.mass_density_comoving;
  primitive.vel_x_peculiar = conserved.momentum_density_x_comoving * inv_rho;
  primitive.vel_y_peculiar = conserved.momentum_density_y_comoving * inv_rho;
  primitive.vel_z_peculiar = conserved.momentum_density_z_comoving * inv_rho;

  const double kinetic_density = 0.5 *
      (conserved.momentum_density_x_comoving * conserved.momentum_density_x_comoving +
       conserved.momentum_density_y_comoving * conserved.momentum_density_y_comoving +
       conserved.momentum_density_z_comoving * conserved.momentum_density_z_comoving) *
      inv_rho;
  const double internal_density = conserved.total_energy_density_comoving - kinetic_density;
  primitive.pressure_comoving = std::max((adiabatic_index - 1.0) * internal_density, k_small);
  return primitive;
}

void HydroCoreSolver::advancePatch(
    HydroConservedStateSoa& conserved,
    const HydroPatchGeometry& geometry,
    const HydroUpdateContext& update,
    const HydroReconstruction& reconstruction,
    const HydroRiemannSolver& riemann_solver,
    std::span<const HydroSourceTerm* const> source_terms,
    const HydroSourceContext& source_context,
    HydroProfileEvent* profile) const {
  HydroScratchBuffers scratch;
  HydroPrimitiveCacheSoa primitive_cache(conserved.size());
  advancePatchWithScratch(
      conserved,
      geometry,
      update,
      reconstruction,
      riemann_solver,
      source_terms,
      source_context,
      scratch,
      &primitive_cache,
      profile);
}

void HydroCoreSolver::advancePatchWithScratch(
    HydroConservedStateSoa& conserved,
    const HydroPatchGeometry& geometry,
    const HydroUpdateContext& update,
    const HydroReconstruction& reconstruction,
    const HydroRiemannSolver& riemann_solver,
    std::span<const HydroSourceTerm* const> source_terms,
    const HydroSourceContext& source_context,
    HydroScratchBuffers& scratch,
    HydroPrimitiveCacheSoa* primitive_cache,
    HydroProfileEvent* profile) const {
  std::vector<std::size_t> full_cells(conserved.size());
  for (std::size_t i = 0; i < full_cells.size(); ++i) {
    full_cells[i] = i;
  }
  std::vector<std::size_t> full_faces(geometry.faces.size());
  for (std::size_t i = 0; i < full_faces.size(); ++i) {
    full_faces[i] = i;
  }
  advancePatchActiveSetWithScratch(
      conserved,
      geometry,
      HydroActiveSetView{.active_cells = full_cells, .active_faces = full_faces},
      update,
      reconstruction,
      riemann_solver,
      source_terms,
      source_context,
      scratch,
      primitive_cache,
      profile);
}

void HydroCoreSolver::advancePatchActiveSet(
    HydroConservedStateSoa& conserved,
    const HydroPatchGeometry& geometry,
    const HydroActiveSetView& active_set,
    const HydroUpdateContext& update,
    const HydroReconstruction& reconstruction,
    const HydroRiemannSolver& riemann_solver,
    std::span<const HydroSourceTerm* const> source_terms,
    const HydroSourceContext& source_context,
    HydroProfileEvent* profile) const {
  HydroScratchBuffers scratch;
  HydroPrimitiveCacheSoa primitive_cache(conserved.size());
  advancePatchActiveSetWithScratch(
      conserved,
      geometry,
      active_set,
      update,
      reconstruction,
      riemann_solver,
      source_terms,
      source_context,
      scratch,
      &primitive_cache,
      profile);
}

void HydroCoreSolver::advancePatchActiveSetWithScratch(
    HydroConservedStateSoa& conserved,
    const HydroPatchGeometry& geometry,
    const HydroActiveSetView& active_set,
    const HydroUpdateContext& update,
    const HydroReconstruction& reconstruction,
    const HydroRiemannSolver& riemann_solver,
    std::span<const HydroSourceTerm* const> source_terms,
    const HydroSourceContext& source_context,
    HydroScratchBuffers& scratch,
    HydroPrimitiveCacheSoa* primitive_cache,
    HydroProfileEvent* profile) const {
  validateAdvanceInputs(conserved, geometry, update, source_context, m_adiabatic_index);
  validateActiveSet(active_set, conserved, geometry);
  scratch.resize(conserved.size(), active_set.active_faces.size());
  if (primitive_cache != nullptr) {
    fillPrimitiveCache(conserved, active_set, m_adiabatic_index, *primitive_cache);
  }

  const auto total_start = std::chrono::steady_clock::now();
  const auto reconstruct_start = std::chrono::steady_clock::now();
  for (std::size_t active_face_slot = 0; active_face_slot < active_set.active_faces.size(); ++active_face_slot) {
    const std::size_t face_index = active_set.active_faces[active_face_slot];
    const bool consumed_cache =
        primitive_cache != nullptr &&
        reconstruction.reconstructFaceFromCache(
            *primitive_cache,
            geometry.faces[face_index],
            scratch.left_states[active_face_slot],
            scratch.right_states[active_face_slot]);
    if (!consumed_cache) {
      reconstruction.reconstructFace(
          conserved,
          geometry.faces[face_index],
          scratch.left_states[active_face_slot],
          scratch.right_states[active_face_slot],
          m_adiabatic_index);
    }
  }
  const auto reconstruct_stop = std::chrono::steady_clock::now();

  const auto riemann_start = std::chrono::steady_clock::now();
  for (std::size_t active_face_slot = 0; active_face_slot < active_set.active_faces.size(); ++active_face_slot) {
    const std::size_t face_index = active_set.active_faces[active_face_slot];
    scratch.fluxes[active_face_slot] = riemann_solver.computeFlux(
        scratch.left_states[active_face_slot],
        scratch.right_states[active_face_slot],
        geometry.faces[face_index],
        m_adiabatic_index);
  }
  const auto riemann_stop = std::chrono::steady_clock::now();

  const auto accumulate_start = std::chrono::steady_clock::now();
  const double flux_scale = update.dt_code / (update.scale_factor * geometry.cell_volume_comoving);
  for (std::size_t active_face_slot = 0; active_face_slot < active_set.active_faces.size(); ++active_face_slot) {
    const std::size_t face_index = active_set.active_faces[active_face_slot];
    const HydroFace& face = geometry.faces[face_index];
    const HydroConservedState face_delta = (flux_scale * face.area_comoving) * scratch.fluxes[active_face_slot];
    scratch.cell_delta[face.owner_cell] -= face_delta;
    if (face.neighbor_cell != k_invalid_cell_index) {
      scratch.cell_delta[face.neighbor_cell] += face_delta;
    }
  }
  const auto accumulate_stop = std::chrono::steady_clock::now();

  const auto source_start = std::chrono::steady_clock::now();
  for (std::size_t cell_index : active_set.active_cells) {
    const HydroConservedState old_cell = conserved.loadCell(cell_index);
    const HydroPrimitiveState primitive = primitive_cache != nullptr
        ? primitive_cache->loadCell(cell_index)
        : primitiveFromConserved(old_cell, m_adiabatic_index);

    HydroConservedState source_total;
    for (const HydroSourceTerm* source : source_terms) {
      if (source == nullptr) {
        continue;
      }
      source_total += source->sourceForCell(cell_index, old_cell, primitive, source_context);
    }

    HydroConservedState updated = old_cell + scratch.cell_delta[cell_index] + update.dt_code * source_total;
    updated.mass_density_comoving = std::max(updated.mass_density_comoving, k_small);
    updated.total_energy_density_comoving = std::max(updated.total_energy_density_comoving, k_small);
    conserved.storeCell(cell_index, updated);
    if (primitive_cache != nullptr) {
      primitive_cache->storeCell(cell_index, primitiveFromConserved(updated, m_adiabatic_index));
    }
  }
  const auto source_stop = std::chrono::steady_clock::now();
  const auto total_stop = std::chrono::steady_clock::now();

  if (profile != nullptr) {
    profile->face_count += static_cast<std::uint64_t>(active_set.active_faces.size());
    profile->reconstruct_ms += std::chrono::duration<double, std::milli>(reconstruct_stop - reconstruct_start).count();
    profile->riemann_ms += std::chrono::duration<double, std::milli>(riemann_stop - riemann_start).count();
    profile->accumulate_ms += std::chrono::duration<double, std::milli>(accumulate_stop - accumulate_start).count();
    profile->source_ms += std::chrono::duration<double, std::milli>(source_stop - source_start).count();
    profile->total_ms += std::chrono::duration<double, std::milli>(total_stop - total_start).count();
    profile->bytes_moved += static_cast<std::uint64_t>(
        (6U * active_set.active_cells.size() + 10U * active_set.active_faces.size()) * sizeof(double));
  }
}

}  // namespace cosmosim::hydro
