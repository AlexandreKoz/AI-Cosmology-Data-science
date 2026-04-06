#include <cassert>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "cosmosim/hydro/hydro_core_solver.hpp"

namespace {

constexpr double k_tol = 1.0e-10;

void testPrimitiveConservedRoundTrip() {
  cosmosim::hydro::HydroPrimitiveState primitive;
  primitive.rho_comoving = 2.5;
  primitive.vel_x_peculiar = 1.2;
  primitive.vel_y_peculiar = -0.5;
  primitive.vel_z_peculiar = 0.75;
  primitive.pressure_comoving = 3.0;

  const double gamma = 5.0 / 3.0;
  const cosmosim::hydro::HydroConservedState conserved =
      cosmosim::hydro::HydroCoreSolver::conservedFromPrimitive(primitive, gamma);
  const cosmosim::hydro::HydroPrimitiveState round_trip =
      cosmosim::hydro::HydroCoreSolver::primitiveFromConserved(conserved, gamma);

  assert(std::abs(round_trip.rho_comoving - primitive.rho_comoving) < k_tol);
  assert(std::abs(round_trip.vel_x_peculiar - primitive.vel_x_peculiar) < k_tol);
  assert(std::abs(round_trip.vel_y_peculiar - primitive.vel_y_peculiar) < k_tol);
  assert(std::abs(round_trip.vel_z_peculiar - primitive.vel_z_peculiar) < k_tol);
  assert(std::abs(round_trip.pressure_comoving - primitive.pressure_comoving) < k_tol);
}

void testComovingSourceTermSanity() {
  cosmosim::hydro::HydroPrimitiveState primitive;
  primitive.rho_comoving = 1.5;
  primitive.vel_x_peculiar = 2.0;
  primitive.vel_y_peculiar = -1.0;
  primitive.vel_z_peculiar = 0.5;
  primitive.pressure_comoving = 1.2;

  const double gamma = 5.0 / 3.0;
  const cosmosim::hydro::HydroConservedState conserved =
      cosmosim::hydro::HydroCoreSolver::conservedFromPrimitive(primitive, gamma);

  const std::vector<double> gravity_x{0.2};
  const std::vector<double> gravity_y{-0.1};
  const std::vector<double> gravity_z{0.05};

  cosmosim::hydro::HydroSourceContext context;
  context.update.dt_code = 0.01;
  context.update.scale_factor = 0.8;
  context.update.hubble_rate_code = 0.4;
  context.gravity_accel_x_peculiar = gravity_x;
  context.gravity_accel_y_peculiar = gravity_y;
  context.gravity_accel_z_peculiar = gravity_z;

  cosmosim::hydro::ComovingGravityExpansionSource source;
  const cosmosim::hydro::HydroConservedState source_state =
      source.sourceForCell(0, conserved, primitive, context);

  assert(std::abs(source_state.mass_density_comoving) < k_tol);
  assert(source_state.momentum_density_x_comoving < primitive.rho_comoving * gravity_x[0]);
  assert(source_state.momentum_density_y_comoving > primitive.rho_comoving * gravity_y[0]);

  const double expected_work = primitive.rho_comoving *
      (primitive.vel_x_peculiar * gravity_x[0] +
       primitive.vel_y_peculiar * gravity_y[0] +
       primitive.vel_z_peculiar * gravity_z[0]);
  assert(source_state.total_energy_density_comoving < expected_work);
}

void testActiveSetUpdateTouchesOnlySelectedCells() {
  constexpr double gamma = 1.4;
  cosmosim::hydro::HydroConservedStateSoa conserved(4);

  for (std::size_t i = 0; i < conserved.size(); ++i) {
    cosmosim::hydro::HydroPrimitiveState primitive;
    primitive.rho_comoving = (i < 2) ? 1.0 : 0.125;
    primitive.pressure_comoving = (i < 2) ? 1.0 : 0.1;
    conserved.storeCell(i, cosmosim::hydro::HydroCoreSolver::conservedFromPrimitive(primitive, gamma));
  }
  const auto baseline_cell_2 = conserved.loadCell(2);

  cosmosim::hydro::HydroPatchGeometry geometry;
  geometry.cell_volume_comoving = 1.0;
  geometry.faces = {
      cosmosim::hydro::HydroFace{.owner_cell = 0, .neighbor_cell = 1, .area_comoving = 1.0, .normal_x = 1.0},
      cosmosim::hydro::HydroFace{.owner_cell = 1, .neighbor_cell = 2, .area_comoving = 1.0, .normal_x = 1.0},
      cosmosim::hydro::HydroFace{.owner_cell = 2, .neighbor_cell = 3, .area_comoving = 1.0, .normal_x = 1.0},
  };

  const std::vector<std::size_t> active_cells{0, 1};
  const std::vector<std::size_t> active_faces{0};
  const cosmosim::hydro::HydroActiveSetView active_set{
      .active_cells = active_cells,
      .active_faces = active_faces};

  cosmosim::hydro::HydroUpdateContext update;
  update.dt_code = 1.0e-3;
  update.scale_factor = 1.0;
  update.hubble_rate_code = 0.0;

  cosmosim::hydro::HydroSourceContext source_context;
  source_context.update = update;

  cosmosim::hydro::HydroCoreSolver solver(gamma);
  cosmosim::hydro::PiecewiseConstantReconstruction reconstruction;
  cosmosim::hydro::HlleRiemannSolver riemann;
  solver.advancePatchActiveSet(conserved, geometry, active_set, update, reconstruction, riemann, {}, source_context, nullptr);

  const auto after_cell_2 = conserved.loadCell(2);
  assert(std::abs(after_cell_2.mass_density_comoving - baseline_cell_2.mass_density_comoving) < k_tol);
  assert(std::abs(after_cell_2.total_energy_density_comoving - baseline_cell_2.total_energy_density_comoving) < k_tol);
}

void testSourceContextMustMatchUpdate() {
  constexpr double gamma = 1.4;
  cosmosim::hydro::HydroConservedStateSoa conserved(2);
  for (std::size_t i = 0; i < 2; ++i) {
    cosmosim::hydro::HydroPrimitiveState primitive;
    primitive.rho_comoving = 1.0;
    primitive.pressure_comoving = 1.0;
    conserved.storeCell(i, cosmosim::hydro::HydroCoreSolver::conservedFromPrimitive(primitive, gamma));
  }

  cosmosim::hydro::HydroPatchGeometry geometry;
  geometry.cell_volume_comoving = 1.0;
  geometry.faces = {
      cosmosim::hydro::HydroFace{.owner_cell = 0, .neighbor_cell = 1, .area_comoving = 1.0, .normal_x = 1.0},
  };
  const std::vector<std::size_t> active_cells{0, 1};
  const std::vector<std::size_t> active_faces{0};
  const cosmosim::hydro::HydroActiveSetView active_set{
      .active_cells = active_cells,
      .active_faces = active_faces};

  cosmosim::hydro::HydroUpdateContext update;
  update.dt_code = 1.0e-3;
  update.scale_factor = 1.0;
  update.hubble_rate_code = 0.0;

  cosmosim::hydro::HydroSourceContext mismatched_source_context;
  mismatched_source_context.update = update;
  mismatched_source_context.update.dt_code = 2.0e-3;

  cosmosim::hydro::HydroCoreSolver solver(gamma);
  cosmosim::hydro::PiecewiseConstantReconstruction reconstruction;
  cosmosim::hydro::HlleRiemannSolver riemann;

  bool threw = false;
  try {
    solver.advancePatchActiveSet(
        conserved,
        geometry,
        active_set,
        update,
        reconstruction,
        riemann,
        {},
        mismatched_source_context,
        nullptr);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

void testScratchAndPrimitiveCachePathMatchesDefaultPath() {
  constexpr double gamma = 1.4;
  cosmosim::hydro::HydroConservedStateSoa baseline(8);
  cosmosim::hydro::HydroConservedStateSoa cached_path(8);

  for (std::size_t i = 0; i < baseline.size(); ++i) {
    cosmosim::hydro::HydroPrimitiveState primitive;
    primitive.rho_comoving = (i < 4) ? 1.0 : 0.125;
    primitive.pressure_comoving = (i < 4) ? 1.0 : 0.1;
    primitive.vel_x_peculiar = 0.01 * static_cast<double>(i);
    const auto conserved = cosmosim::hydro::HydroCoreSolver::conservedFromPrimitive(primitive, gamma);
    baseline.storeCell(i, conserved);
    cached_path.storeCell(i, conserved);
  }

  cosmosim::hydro::HydroPatchGeometry geometry;
  geometry.cell_volume_comoving = 1.0;
  for (std::size_t i = 0; i < 8; ++i) {
    geometry.faces.push_back(cosmosim::hydro::HydroFace{
        .owner_cell = i,
        .neighbor_cell = (i + 1U) % 8U,
        .area_comoving = 1.0,
        .normal_x = 1.0});
  }

  cosmosim::hydro::HydroUpdateContext update;
  update.dt_code = 1.0e-3;
  update.scale_factor = 1.0;
  update.hubble_rate_code = 0.0;
  cosmosim::hydro::HydroSourceContext source_context;
  source_context.update = update;

  cosmosim::hydro::HydroCoreSolver solver(gamma);
  cosmosim::hydro::PiecewiseConstantReconstruction reconstruction;
  cosmosim::hydro::HlleRiemannSolver riemann;

  solver.advancePatch(baseline, geometry, update, reconstruction, riemann, {}, source_context, nullptr);

  cosmosim::hydro::HydroScratchBuffers scratch;
  cosmosim::hydro::HydroPrimitiveCacheSoa primitive_cache(cached_path.size());
  solver.advancePatchWithScratch(
      cached_path,
      geometry,
      update,
      reconstruction,
      riemann,
      {},
      source_context,
      scratch,
      &primitive_cache,
      nullptr);

  for (std::size_t i = 0; i < baseline.size(); ++i) {
    const auto a = baseline.loadCell(i);
    const auto b = cached_path.loadCell(i);
    assert(std::abs(a.mass_density_comoving - b.mass_density_comoving) < 1.0e-12);
    assert(std::abs(a.momentum_density_x_comoving - b.momentum_density_x_comoving) < 1.0e-12);
    assert(std::abs(a.total_energy_density_comoving - b.total_energy_density_comoving) < 1.0e-12);
  }
}

}  // namespace

int main() {
  testPrimitiveConservedRoundTrip();
  testComovingSourceTermSanity();
  testActiveSetUpdateTouchesOnlySelectedCells();
  testSourceContextMustMatchUpdate();
  testScratchAndPrimitiveCachePathMatchesDefaultPath();
  return 0;
}
