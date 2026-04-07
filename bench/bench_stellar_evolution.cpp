#include "cosmosim/stellar_evolution_bookkeeping.h"

#include <chrono>
#include <iostream>
#include <string>

using cosmosim::ChannelWeights;
using cosmosim::EvolutionTableProvenance;
using cosmosim::StarPopulationState;
using cosmosim::StellarEvolutionBookkeeper;

#ifndef COSMOSIM_SOURCE_DIR
#define COSMOSIM_SOURCE_DIR "."
#endif


int main() {
    const std::string table_path = std::string(COSMOSIM_SOURCE_DIR) + "/resources/stellar_evolution/default_table.csv";
    EvolutionTableProvenance provenance{"v1_default", table_path,
                                         "stellar_evo_schema_v1"};
    auto table = StellarEvolutionBookkeeper::loadEvolutionTableCsv(provenance.source_path, provenance);

    ChannelWeights channel_weights{0.30, 0.50, 0.20};
    StellarEvolutionBookkeeper bookkeeper(table, channel_weights);

    constexpr std::size_t n_star = 200000;
    StarPopulationState stars;
    stars.current_mass_msun.resize(n_star, 1000.0);
    stars.birth_mass_msun.resize(n_star, 1000.0);
    stars.age_yr.resize(n_star, 1.0e7);
    stars.metallicity_mass_fraction.resize(n_star, 0.02);

    const double timestep_yr = 5.0e7;
    const int warmup_steps = 3;
    const int measured_steps = 10;

    for (int i = 0; i < warmup_steps; ++i) {
        (void)bookkeeper.advancePopulation(stars, timestep_yr, 1.0e-8);
    }

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < measured_steps; ++i) {
        (void)bookkeeper.advancePopulation(stars, timestep_yr, 1.0e-8);
    }
    const auto t1 = std::chrono::steady_clock::now();

    const double elapsed_s = std::chrono::duration<double>(t1 - t0).count();
    const double stars_processed = static_cast<double>(n_star) * measured_steps;
    const double throughput_mstar_per_s = stars_processed / elapsed_s / 1.0e6;

    // Effective memory traffic proxy for hot arrays touched per star update.
    const double bytes_per_star = (4.0 + 4.0) * sizeof(double);
    const double gb_per_s = stars_processed * bytes_per_star / elapsed_s / 1.0e9;

    std::cout << "bench_stellar_evolution\n";
    std::cout << "build_type=unknown (set by CMake generator)\n";
    std::cout << "threads=1\n";
    std::cout << "features=scalar_cpu\n";
    std::cout << "setup_steps=" << warmup_steps << " measured_steps=" << measured_steps << "\n";
    std::cout << "steady_state_elapsed_s=" << elapsed_s << "\n";
    std::cout << "throughput_Mstar_per_s=" << throughput_mstar_per_s << "\n";
    std::cout << "effective_memory_proxy_GB_per_s=" << gb_per_s << "\n";

    return 0;
}
