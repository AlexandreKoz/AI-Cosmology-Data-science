#include <cassert>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include "cosmosim/core/build_config.hpp"
#include "cosmosim/gravity/pm_solver.hpp"

namespace {

constexpr double k_tolerance = 1.0e-8;
constexpr double k_pi = 3.141592653589793238462643383279502884;

void testCicMassConservation() {
  const cosmosim::gravity::PmGridShape shape{16, 8, 4};
  cosmosim::gravity::PmGridStorage grid(shape);
  cosmosim::gravity::PmSolver solver(shape);

  const std::vector<double> pos_x{0.1, 2.1, 3.9, 7.2};
  const std::vector<double> pos_y{0.2, 1.2, 2.2, 3.2};
  const std::vector<double> pos_z{0.3, 0.4, 0.5, 0.6};
  const std::vector<double> mass{2.0, 3.0, 4.0, 5.0};

  cosmosim::gravity::PmSolveOptions options;
  options.box_size_mpc_comoving = 8.0;
  options.scale_factor = 1.0;

  solver.assignDensity(grid, pos_x, pos_y, pos_z, mass, options, nullptr);

  const double total_density = std::accumulate(grid.density().begin(), grid.density().end(), 0.0);
  const double total_mass = std::accumulate(mass.begin(), mass.end(), 0.0);
  const double cell_volume = std::pow(options.box_size_mpc_comoving, 3.0) / static_cast<double>(shape.cellCount());
  assert(std::abs(total_density * cell_volume - total_mass) < k_tolerance);
}

void testPoissonAnalyticMode() {
  const cosmosim::gravity::PmGridShape shape{16, 8, 8};
  cosmosim::gravity::PmGridStorage grid(shape);
  cosmosim::gravity::PmSolver solver(shape);

  cosmosim::gravity::PmSolveOptions options;
  options.box_size_mpc_comoving = 1.0;
  options.scale_factor = 0.5;
  options.gravitational_constant_code = 1.2;

  const double amplitude = 0.25;
  const double kx = 2.0 * k_pi / options.box_size_mpc_comoving;
  for (std::size_t ix = 0; ix < shape.nx; ++ix) {
    const double x = (static_cast<double>(ix) + 0.5) / static_cast<double>(shape.nx) * options.box_size_mpc_comoving;
    for (std::size_t iy = 0; iy < shape.ny; ++iy) {
      for (std::size_t iz = 0; iz < shape.nz; ++iz) {
        grid.density()[grid.linearIndex(ix, iy, iz)] = amplitude * std::sin(kx * x);
      }
    }
  }

  solver.solvePoissonPeriodic(grid, options, nullptr);

  const double expected_amp = 4.0 * k_pi * options.gravitational_constant_code *
      options.scale_factor * options.scale_factor * amplitude / kx;

  double corr = 0.0;
  double norm_expected = 0.0;
  double norm_got = 0.0;
  for (std::size_t ix = 0; ix < shape.nx; ++ix) {
    const double x = (static_cast<double>(ix) + 0.5) / static_cast<double>(shape.nx) * options.box_size_mpc_comoving;
    const double expected = expected_amp * std::cos(kx * x);
    for (std::size_t iy = 0; iy < shape.ny; ++iy) {
      for (std::size_t iz = 0; iz < shape.nz; ++iz) {
        const double got = grid.force_x()[grid.linearIndex(ix, iy, iz)];
        corr += expected * got;
        norm_expected += expected * expected;
        norm_got += got * got;
      }
    }
  }

  const double cosine_similarity = corr / std::sqrt(std::max(norm_expected * norm_got, 1.0e-20));
#if COSMOSIM_ENABLE_FFTW
  assert(cosine_similarity > 0.98);
#else
  assert(std::isfinite(cosine_similarity));
  assert(norm_got > 0.0);
#endif
}

void testTreePmBuildGate() {
  if (cosmosim::gravity::treePmSupportedByBuild()) {
    cosmosim::gravity::requireTreePmSupportOrThrow("treepm");
    return;
  }

  bool threw = false;
  bool saw_actionable_message = false;
  try {
    cosmosim::gravity::requireTreePmSupportOrThrow("treepm");
  } catch (const std::runtime_error& ex) {
    threw = true;
    saw_actionable_message = std::string(ex.what()).find("COSMOSIM_ENABLE_FFTW=ON") != std::string::npos;
  }
  assert(threw);
  assert(saw_actionable_message);
}

}  // namespace

int main() {
  testCicMassConservation();
  testPoissonAnalyticMode();
  testTreePmBuildGate();
  return 0;
}
