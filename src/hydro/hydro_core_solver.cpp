#include "cosmosim/hydro/hydro_core_solver.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
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

[[nodiscard]] double soundSpeed(double rho_comoving, double pressure_comoving, double adiabatic_index) {
  return std::sqrt(adiabatic_index * std::max(pressure_comoving, k_small) / std::max(rho_comoving, k_small));
}

[[nodiscard]] HydroConservedState eulerPhysicalFlux(
    const HydroPrimitiveState& primitive,
    const HydroFace& face,
    double adiabatic_index) {
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

// Enforce positivity explicitly and count fallback events for diagnostics.
[[nodiscard]] bool enforcePrimitiveFloors(HydroPrimitiveState& state, double rho_floor, double pressure_floor) {
  bool changed = false;
  if (state.rho_comoving < rho_floor) {
    state.rho_comoving = rho_floor;
    changed = true;
  }
  if (state.pressure_comoving < pressure_floor) {
    state.pressure_comoving = pressure_floor;
    changed = true;
  }
  return changed;
}

[[nodiscard]] bool isContiguousPair(std::size_t left_index, std::size_t right_index, std::size_t cell_count) {
  return right_index == left_index + 1U || (left_index + 1U == cell_count && right_index == 0U);
}

[[nodiscard]] std::size_t wrappedPlusOne(std::size_t cell_index, std::size_t cell_count) {
  return (cell_index + 1U) % cell_count;
}

[[nodiscard]] std::size_t wrappedMinusOne(std::size_t cell_index, std::size_t cell_count) {
  return (cell_index + cell_count - 1U) % cell_count;
}

[[nodiscard]] double minmodLimiter(double a, double b) {
  if (a * b <= 0.0) {
    return 0.0;
  }
  return (std::abs(a) < std::abs(b)) ? a : b;
}

[[nodiscard]] HydroConservedState hllFlux(
    const HydroPrimitiveState& left_state,
    const HydroPrimitiveState& right_state,
    const HydroFace& face,
    double adiabatic_index) {
  const HydroConservedState u_left = HydroCoreSolver::conservedFromPrimitive(left_state, adiabatic_index);
  const HydroConservedState u_right = HydroCoreSolver::conservedFromPrimitive(right_state, adiabatic_index);

  const HydroConservedState f_left = eulerPhysicalFlux(left_state, face, adiabatic_index);
  const HydroConservedState f_right = eulerPhysicalFlux(right_state, face, adiabatic_index);

  const double vel_n_left = dot3(left_state.vel_x_peculiar, left_state.vel_y_peculiar, left_state.vel_z_peculiar, face.normal_x, face.normal_y, face.normal_z);
  const double vel_n_right = dot3(right_state.vel_x_peculiar, right_state.vel_y_peculiar, right_state.vel_z_peculiar, face.normal_x, face.normal_y, face.normal_z);

  const double c_left = soundSpeed(left_state.rho_comoving, left_state.pressure_comoving, adiabatic_index);
  const double c_right = soundSpeed(right_state.rho_comoving, right_state.pressure_comoving, adiabatic_index);

  const double s_left = std::min(vel_n_left - c_left, vel_n_right - c_right);
  const double s_right = std::max(vel_n_left + c_left, vel_n_right + c_right);

  if (s_left >= 0.0) {
    return f_left;
  }
  if (s_right <= 0.0) {
    return f_right;
  }

  const double inv_speed_delta = 1.0 / std::max(s_right - s_left, k_small);
  HydroConservedState flux;
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

std::size_t HydroConservedStateSoa::size() const { return m_mass_density_comoving.size(); }

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

std::size_t HydroPrimitiveCacheSoa::size() const { return m_rho_comoving.size(); }

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

std::string_view hydroSlopeLimiterToString(HydroSlopeLimiter limiter) {
  switch (limiter) {
    case HydroSlopeLimiter::kMinmod:
      return "minmod";
    case HydroSlopeLimiter::kMonotonizedCentral:
      return "mc";
    case HydroSlopeLimiter::kVanLeer:
      return "van_leer";
  }
  return "unknown";
}

HydroSlopeLimiter hydroSlopeLimiterFromString(std::string_view name) {
  if (name == "minmod") {
    return HydroSlopeLimiter::kMinmod;
  }
  if (name == "mc" || name == "monotonized_central") {
    return HydroSlopeLimiter::kMonotonizedCentral;
  }
  if (name == "van_leer") {
    return HydroSlopeLimiter::kVanLeer;
  }
  throw std::invalid_argument("Unsupported hydro slope limiter: " + std::string(name));
}

double applyHydroSlopeLimiter(HydroSlopeLimiter limiter, double delta_minus, double delta_plus) {
  switch (limiter) {
    case HydroSlopeLimiter::kMinmod:
      return minmodLimiter(delta_minus, delta_plus);
    case HydroSlopeLimiter::kMonotonizedCentral: {
      const double mm = minmodLimiter(delta_minus, delta_plus);
      return minmodLimiter(0.5 * (delta_minus + delta_plus), 2.0 * mm);
    }
    case HydroSlopeLimiter::kVanLeer: {
      const double product = delta_minus * delta_plus;
      if (product <= 0.0) {
        return 0.0;
      }
      return 2.0 * product / (delta_minus + delta_plus);
    }
  }
  return 0.0;
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

MusclHancockReconstruction::MusclHancockReconstruction(HydroReconstructionPolicy policy) : m_policy(policy) {
  if (m_policy.rho_floor <= 0.0 || m_policy.pressure_floor <= 0.0) {
    throw std::invalid_argument("MUSCL-Hancock reconstruction requires positive floors");
  }
}

bool MusclHancockReconstruction::reconstructFaceFromCache(
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

  const std::size_t cell_count = primitive_cache.size();
  if (!isContiguousPair(face.owner_cell, face.neighbor_cell, cell_count)) {
    return true;
  }

  const std::size_t left_minus_index = wrappedMinusOne(face.owner_cell, cell_count);
  const std::size_t right_plus_index = wrappedPlusOne(face.neighbor_cell, cell_count);

  const HydroPrimitiveState left_minus = primitive_cache.loadCell(left_minus_index);
  const HydroPrimitiveState right_plus = primitive_cache.loadCell(right_plus_index);

  auto limited = [&](double qm, double q, double qp) {
    const double dm = q - qm;
    const double dp = qp - q;
    const double slope = applyHydroSlopeLimiter(m_policy.limiter, dm, dp);
    if (std::abs(slope) < std::abs(0.5 * (dm + dp))) {
      ++m_limiter_clip_count;
    }
    return slope;
  };

  const double slope_rho_left = limited(left_minus.rho_comoving, left_state.rho_comoving, right_state.rho_comoving);
  const double slope_u_left = limited(left_minus.vel_x_peculiar, left_state.vel_x_peculiar, right_state.vel_x_peculiar);
  const double slope_p_left = limited(left_minus.pressure_comoving, left_state.pressure_comoving, right_state.pressure_comoving);

  const double slope_rho_right = limited(left_state.rho_comoving, right_state.rho_comoving, right_plus.rho_comoving);
  const double slope_u_right = limited(left_state.vel_x_peculiar, right_state.vel_x_peculiar, right_plus.vel_x_peculiar);
  const double slope_p_right = limited(left_state.pressure_comoving, right_state.pressure_comoving, right_plus.pressure_comoving);

  left_state.rho_comoving += 0.5 * slope_rho_left;
  left_state.vel_x_peculiar += 0.5 * slope_u_left;
  left_state.pressure_comoving += 0.5 * slope_p_left;

  right_state.rho_comoving -= 0.5 * slope_rho_right;
  right_state.vel_x_peculiar -= 0.5 * slope_u_right;
  right_state.pressure_comoving -= 0.5 * slope_p_right;

  if (m_policy.enable_muscl_hancock_predictor && m_policy.dt_over_dx_code > 0.0) {
    const double cfl = m_policy.dt_over_dx_code;

    const double drho_dt_left = -left_state.vel_x_peculiar * slope_rho_left - left_state.rho_comoving * slope_u_left;
    const double du_dt_left = -left_state.vel_x_peculiar * slope_u_left - slope_p_left / std::max(left_state.rho_comoving, k_small);
    const double dp_dt_left = -left_state.vel_x_peculiar * slope_p_left - m_policy.adiabatic_index * left_state.pressure_comoving * slope_u_left;

    const double drho_dt_right = -right_state.vel_x_peculiar * slope_rho_right - right_state.rho_comoving * slope_u_right;
    const double du_dt_right = -right_state.vel_x_peculiar * slope_u_right - slope_p_right / std::max(right_state.rho_comoving, k_small);
    const double dp_dt_right = -right_state.vel_x_peculiar * slope_p_right - m_policy.adiabatic_index * right_state.pressure_comoving * slope_u_right;

    left_state.rho_comoving += -0.5 * cfl * drho_dt_left;
    left_state.vel_x_peculiar += -0.5 * cfl * du_dt_left;
    left_state.pressure_comoving += -0.5 * cfl * dp_dt_left;

    right_state.rho_comoving += -0.5 * cfl * drho_dt_right;
    right_state.vel_x_peculiar += -0.5 * cfl * du_dt_right;
    right_state.pressure_comoving += -0.5 * cfl * dp_dt_right;
  }

  if (enforcePrimitiveFloors(left_state, m_policy.rho_floor, m_policy.pressure_floor)) {
    ++m_positivity_fallback_count;
  }
  if (enforcePrimitiveFloors(right_state, m_policy.rho_floor, m_policy.pressure_floor)) {
    ++m_positivity_fallback_count;
  }
  return true;
}

void MusclHancockReconstruction::reconstructFace(
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

HydroReconstructionPolicy MusclHancockReconstruction::policy() const { return m_policy; }
std::uint64_t MusclHancockReconstruction::limiterClipCount() const { return m_limiter_clip_count; }
std::uint64_t MusclHancockReconstruction::positivityFallbackCount() const { return m_positivity_fallback_count; }

HydroConservedState HlleRiemannSolver::computeFlux(
    const HydroPrimitiveState& left_state,
    const HydroPrimitiveState& right_state,
    const HydroFace& face,
    double adiabatic_index) const {
  return hllFlux(left_state, right_state, face, adiabatic_index);
}

HydroConservedState HllcRiemannSolver::computeFlux(
    const HydroPrimitiveState& left_state,
    const HydroPrimitiveState& right_state,
    const HydroFace& face,
    double adiabatic_index) const {
  const double un_left = dot3(left_state.vel_x_peculiar, left_state.vel_y_peculiar, left_state.vel_z_peculiar,
      face.normal_x, face.normal_y, face.normal_z);
  const double un_right = dot3(right_state.vel_x_peculiar, right_state.vel_y_peculiar, right_state.vel_z_peculiar,
      face.normal_x, face.normal_y, face.normal_z);

  const double c_left = soundSpeed(left_state.rho_comoving, left_state.pressure_comoving, adiabatic_index);
  const double c_right = soundSpeed(right_state.rho_comoving, right_state.pressure_comoving, adiabatic_index);

  const double s_left = std::min(un_left - c_left, un_right - c_right);
  const double s_right = std::max(un_left + c_left, un_right + c_right);

  if (s_left >= 0.0) {
    return eulerPhysicalFlux(left_state, face, adiabatic_index);
  }
  if (s_right <= 0.0) {
    return eulerPhysicalFlux(right_state, face, adiabatic_index);
  }

  const HydroConservedState u_left = HydroCoreSolver::conservedFromPrimitive(left_state, adiabatic_index);
  const HydroConservedState u_right = HydroCoreSolver::conservedFromPrimitive(right_state, adiabatic_index);
  const HydroConservedState f_left = eulerPhysicalFlux(left_state, face, adiabatic_index);
  const HydroConservedState f_right = eulerPhysicalFlux(right_state, face, adiabatic_index);

  const double numerator = right_state.pressure_comoving - left_state.pressure_comoving +
      left_state.rho_comoving * un_left * (s_left - un_left) -
      right_state.rho_comoving * un_right * (s_right - un_right);
  const double denominator = left_state.rho_comoving * (s_left - un_left) -
      right_state.rho_comoving * (s_right - un_right);
  if (std::abs(denominator) < k_small) {
    ++m_fallback_count;
    return hllFlux(left_state, right_state, face, adiabatic_index);
  }
  const double s_star = numerator / denominator;

  const double p_star_left = left_state.pressure_comoving + left_state.rho_comoving * (s_left - un_left) * (s_star - un_left);
  const double p_star_right = right_state.pressure_comoving + right_state.rho_comoving * (s_right - un_right) * (s_star - un_right);
  const double p_star = 0.5 * (p_star_left + p_star_right);
  if (p_star <= 0.0) {
    ++m_fallback_count;
    return hllFlux(left_state, right_state, face, adiabatic_index);
  }

  auto starState = [&](const HydroPrimitiveState& prim, const HydroConservedState& cons, double s_k, double u_k_n) {
    const double denom = s_k - s_star;
    if (std::abs(denom) < k_small) {
      return HydroConservedState{};
    }
    const double rho_star = prim.rho_comoving * (s_k - u_k_n) / denom;
    if (!std::isfinite(rho_star) || rho_star <= 0.0) {
      return HydroConservedState{};
    }
    HydroConservedState star;
    star.mass_density_comoving = rho_star;

    const double velocity_projection = dot3(prim.vel_x_peculiar, prim.vel_y_peculiar, prim.vel_z_peculiar,
        face.normal_x, face.normal_y, face.normal_z);
    const double delta_normal = s_star - velocity_projection;
    star.momentum_density_x_comoving = rho_star * (prim.vel_x_peculiar + delta_normal * face.normal_x);
    star.momentum_density_y_comoving = rho_star * (prim.vel_y_peculiar + delta_normal * face.normal_y);
    star.momentum_density_z_comoving = rho_star * (prim.vel_z_peculiar + delta_normal * face.normal_z);

    const double denom_energy = prim.rho_comoving * (s_k - u_k_n);
    if (std::abs(denom_energy) < k_small) {
      return HydroConservedState{};
    }
    const double energy_num =
        (s_k - u_k_n) * cons.total_energy_density_comoving - prim.pressure_comoving * u_k_n + p_star * s_star;
    star.total_energy_density_comoving = rho_star * energy_num / denom_energy;
    if (!std::isfinite(star.total_energy_density_comoving) || star.total_energy_density_comoving <= 0.0) {
      return HydroConservedState{};
    }
    return star;
  };

  const HydroConservedState u_star_left = starState(left_state, u_left, s_left, un_left);
  const HydroConservedState u_star_right = starState(right_state, u_right, s_right, un_right);

  if (u_star_left.mass_density_comoving <= 0.0 || u_star_right.mass_density_comoving <= 0.0 ||
      !std::isfinite(u_star_left.total_energy_density_comoving) || !std::isfinite(u_star_right.total_energy_density_comoving)) {
    ++m_fallback_count;
    return hllFlux(left_state, right_state, face, adiabatic_index);
  }

  if (s_star >= 0.0) {
    return f_left + s_left * (u_star_left - u_left);
  }
  return f_right + s_right * (u_star_right - u_right);
}

std::uint64_t HllcRiemannSolver::fallbackCount() const {
  return m_fallback_count;
}

HydroCoreSolver::HydroCoreSolver(double adiabatic_index) : m_adiabatic_index(adiabatic_index) {
  if (m_adiabatic_index <= 1.0) {
    throw std::invalid_argument("HydroCoreSolver requires adiabatic_index > 1");
  }
}

double HydroCoreSolver::adiabaticIndex() const { return m_adiabatic_index; }

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
  const std::uint64_t limiter_before = reconstruction.limiterClipCount();
  const std::uint64_t positivity_before = reconstruction.positivityFallbackCount();
  const std::uint64_t riemann_fallback_before = riemann_solver.fallbackCount();

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
    const std::uint64_t limiter_after = reconstruction.limiterClipCount();
    const std::uint64_t positivity_after = reconstruction.positivityFallbackCount();
    const std::uint64_t riemann_fallback_after = riemann_solver.fallbackCount();
    profile->limiter_clip_count += (limiter_after >= limiter_before) ? (limiter_after - limiter_before) : 0U;
    profile->positivity_fallback_count +=
        (positivity_after >= positivity_before) ? (positivity_after - positivity_before) : 0U;
    profile->riemann_fallback_count +=
        (riemann_fallback_after >= riemann_fallback_before) ? (riemann_fallback_after - riemann_fallback_before) : 0U;
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
