#include <algorithm>
#include <cassert>
#include <cmath>
#include <span>
#include <cstdint>
#include <vector>

#include "cosmosim/gravity/tree_gravity.hpp"

namespace {

void directSumAcceleration(
    std::span<const double> pos_x,
    std::span<const double> pos_y,
    std::span<const double> pos_z,
    std::span<const double> mass,
    std::span<const std::uint32_t> active,
    const cosmosim::gravity::TreeGravityOptions& options,
    std::span<double> ax,
    std::span<double> ay,
    std::span<double> az) {
  for (std::size_t i = 0; i < active.size(); ++i) {
    const std::uint32_t target = active[i];
    const double tx = pos_x[target];
    const double ty = pos_y[target];
    const double tz = pos_z[target];
    double acc_x = 0.0;
    double acc_y = 0.0;
    double acc_z = 0.0;
    for (std::size_t j = 0; j < mass.size(); ++j) {
      if (j == target) {
        continue;
      }
      const double dx = pos_x[j] - tx;
      const double dy = pos_y[j] - ty;
      const double dz = pos_z[j] - tz;
      const double r2 = dx * dx + dy * dy + dz * dz;
      const double factor = options.gravitational_constant_code * mass[j] *
          cosmosim::gravity::softenedInvR3(r2, options.softening);
      acc_x += factor * dx;
      acc_y += factor * dy;
      acc_z += factor * dz;
    }
    ax[i] = acc_x;
    ay[i] = acc_y;
    az[i] = acc_z;
  }
}

}  // namespace

int main() {
  constexpr std::size_t particle_count = 96;
  std::vector<double> pos_x(particle_count, 0.0);
  std::vector<double> pos_y(particle_count, 0.0);
  std::vector<double> pos_z(particle_count, 0.0);
  std::vector<double> mass(particle_count, 0.0);

  for (std::size_t i = 0; i < particle_count; ++i) {
    pos_x[i] = std::fmod(static_cast<double>((29U * i + 7U) % 997U) * 0.013, 1.0);
    pos_y[i] = std::fmod(static_cast<double>((47U * i + 11U) % 991U) * 0.017, 1.0);
    pos_z[i] = std::fmod(static_cast<double>((71U * i + 3U) % 983U) * 0.019, 1.0);
    mass[i] = 0.5 + static_cast<double>((13U * i) % 9U) * 0.05;
  }

  std::vector<std::uint32_t> active(16);
  for (std::size_t i = 0; i < active.size(); ++i) {
    active[i] = static_cast<std::uint32_t>(i * 3U);
  }

  cosmosim::gravity::TreeGravityOptions options;
  options.opening_theta = 0.4;
  options.gravitational_constant_code = 1.0;
  options.max_leaf_size = 4;
  options.softening.epsilon_comoving = 1.0e-3;

  cosmosim::gravity::TreeGravitySolver solver;
  cosmosim::gravity::TreeGravityProfile profile;
  solver.build(pos_x, pos_y, pos_z, mass, options, &profile);

  std::vector<double> tree_ax(active.size(), 0.0);
  std::vector<double> tree_ay(active.size(), 0.0);
  std::vector<double> tree_az(active.size(), 0.0);
  solver.evaluateActiveSet(pos_x, pos_y, pos_z, mass, active, tree_ax, tree_ay, tree_az, options, &profile);

  std::vector<double> direct_ax(active.size(), 0.0);
  std::vector<double> direct_ay(active.size(), 0.0);
  std::vector<double> direct_az(active.size(), 0.0);
  directSumAcceleration(pos_x, pos_y, pos_z, mass, active, options, direct_ax, direct_ay, direct_az);

  double max_relative_error = 0.0;
  for (std::size_t i = 0; i < active.size(); ++i) {
    const double direct_norm = std::sqrt(
        direct_ax[i] * direct_ax[i] + direct_ay[i] * direct_ay[i] + direct_az[i] * direct_az[i]);
    const double diff_norm = std::sqrt(
        (tree_ax[i] - direct_ax[i]) * (tree_ax[i] - direct_ax[i]) +
        (tree_ay[i] - direct_ay[i]) * (tree_ay[i] - direct_ay[i]) +
        (tree_az[i] - direct_az[i]) * (tree_az[i] - direct_az[i]));
    const double denom = std::max(direct_norm, 1.0e-12);
    max_relative_error = std::max(max_relative_error, diff_norm / denom);
  }

  assert(max_relative_error < 0.03);
  assert(profile.build_ms >= 0.0);
  assert(profile.multipole_ms >= 0.0);
  assert(profile.traversal_ms >= 0.0);
  assert(profile.accepted_nodes > 0);

  return 0;
}
