#include <cassert>
#include <cmath>
#include <vector>

#include "cosmosim/hydro/hydro_core_solver.hpp"

namespace {

constexpr double k_tol = 1.0e-9;

cosmosim::hydro::HydroPatchGeometry makePeriodic1dGeometry(std::size_t cell_count) {
  cosmosim::hydro::HydroPatchGeometry geometry;
  geometry.cell_volume_comoving = 1.0;
  geometry.faces.reserve(cell_count);

  for (std::size_t i = 0; i < cell_count; ++i) {
    geometry.faces.push_back(cosmosim::hydro::HydroFace{
        .owner_cell = i,
        .neighbor_cell = (i + 1U) % cell_count,
        .area_comoving = 1.0,
        .normal_x = 1.0,
        .normal_y = 0.0,
        .normal_z = 0.0});
  }

  return geometry;
}

void fillSodLikeInitialState(cosmosim::hydro::HydroConservedStateSoa& conserved, double gamma) {
  for (std::size_t i = 0; i < conserved.size(); ++i) {
    cosmosim::hydro::HydroPrimitiveState primitive;
    if (i < conserved.size() / 2U) {
      primitive.rho_comoving = 1.0;
      primitive.pressure_comoving = 1.0;
    } else {
      primitive.rho_comoving = 0.125;
      primitive.pressure_comoving = 0.1;
    }
    primitive.vel_x_peculiar = 0.0;
    primitive.vel_y_peculiar = 0.0;
    primitive.vel_z_peculiar = 0.0;

    conserved.storeCell(i, cosmosim::hydro::HydroCoreSolver::conservedFromPrimitive(primitive, gamma));
  }
}

void testSodLikePeriodicConservationAndRegression() {
  constexpr std::size_t k_cell_count = 64;
  constexpr std::size_t k_step_count = 30;
  constexpr double k_gamma = 1.4;

  cosmosim::hydro::HydroConservedStateSoa conserved(k_cell_count);
  fillSodLikeInitialState(conserved, k_gamma);

  const cosmosim::hydro::HydroPatchGeometry geometry = makePeriodic1dGeometry(k_cell_count);

  cosmosim::hydro::HydroUpdateContext update;
  update.dt_code = 1.0e-3;
  update.scale_factor = 1.0;
  update.hubble_rate_code = 0.0;

  cosmosim::hydro::HydroSourceContext source_context;
  source_context.update = update;

  cosmosim::hydro::HydroCoreSolver solver(k_gamma);
  cosmosim::hydro::PiecewiseConstantReconstruction reconstruction;
  cosmosim::hydro::HlleRiemannSolver riemann_solver;

  const double initial_mass = [&]() {
    double sum = 0.0;
    for (double rho : conserved.massDensityComoving()) {
      sum += rho;
    }
    return sum;
  }();
  const double initial_energy = [&]() {
    double sum = 0.0;
    for (double e : conserved.totalEnergyDensityComoving()) {
      sum += e;
    }
    return sum;
  }();

  for (std::size_t step = 0; step < k_step_count; ++step) {
    solver.advancePatch(
        conserved,
        geometry,
        update,
        reconstruction,
        riemann_solver,
        {},
        source_context,
        nullptr);
  }

  const double final_mass = [&]() {
    double sum = 0.0;
    for (double rho : conserved.massDensityComoving()) {
      sum += rho;
    }
    return sum;
  }();
  const double final_energy = [&]() {
    double sum = 0.0;
    for (double e : conserved.totalEnergyDensityComoving()) {
      sum += e;
    }
    return sum;
  }();

  assert(std::abs(final_mass - initial_mass) < k_tol);
  assert(std::abs(final_energy - initial_energy) < 1.0e-6);

  // Regression probes: the interface should smooth while bulk states remain bounded.
  const double left_bulk = conserved.massDensityComoving()[20U];
  const double right_bulk = conserved.massDensityComoving()[40U];
  const double interface_left = conserved.massDensityComoving()[31U];
  const double interface_right = conserved.massDensityComoving()[32U];

  assert(left_bulk > 0.9 && left_bulk < 1.05);
  assert(right_bulk > 0.08 && right_bulk < 0.35);
  assert(interface_left < 1.0 && interface_left > 0.2);
  assert(interface_right > 0.125 && interface_right < 0.9);
}

}  // namespace

int main() {
  testSodLikePeriodicConservationAndRegression();
  return 0;
}
