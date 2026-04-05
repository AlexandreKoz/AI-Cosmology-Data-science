#include <chrono>
#include <cstddef>
#include <iostream>
#include <vector>

#include "core/internal/state_layout.hpp"

int main() {
  constexpr std::size_t particle_count = 200000;
  cosmosim::core::internal::ParticleHotSoa hot;
  hot.position_x_comoving.resize(particle_count, 1.0);
  hot.position_y_comoving.resize(particle_count, 2.0);
  hot.position_z_comoving.resize(particle_count, 3.0);

  const auto start = std::chrono::steady_clock::now();
  double checksum = 0.0;
  for (std::size_t i = 0; i < particle_count; ++i) {
    checksum += hot.position_x_comoving[i] + hot.position_y_comoving[i] + hot.position_z_comoving[i];
  }
  const auto stop = std::chrono::steady_clock::now();
  const auto elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();

  std::cout << "bench_layout_smoke checksum=" << checksum << " elapsed_us=" << elapsed_us << '\n';
  return 0;
}
