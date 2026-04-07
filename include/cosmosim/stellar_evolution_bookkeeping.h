#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cosmosim {

enum class YieldChannel : std::uint8_t {
    agb = 0,
    ccsn = 1,
    snia = 2,
    count = 3
};

struct EvolutionTableProvenance {
    std::string table_version;
    std::string source_path;
    std::string schema_version;
};

struct EvolutionTable {
    std::vector<double> age_yr;
    std::vector<double> cumulative_return_fraction;
    std::vector<double> cumulative_metals_fraction;
    std::vector<double> cumulative_alpha_fraction;
    std::vector<double> cumulative_iron_fraction;
    std::vector<double> cumulative_energy_erg_per_msun;
    EvolutionTableProvenance provenance;
};

struct ChannelWeights {
    // Fractions of each timestep-integrated budget routed to channels.
    double agb_fraction;
    double ccsn_fraction;
    double snia_fraction;

    bool isNormalized(double tolerance = 1.0e-12) const;
};

struct StarPopulationState {
    // Hot structure-of-arrays for active star bookkeeping state.
    std::vector<double> current_mass_msun;
    std::vector<double> birth_mass_msun;
    std::vector<double> age_yr;
    std::vector<double> metallicity_mass_fraction;

    std::size_t size() const;
};

struct PerChannelBudget {
    double returned_mass_msun = 0.0;
    double returned_metals_msun = 0.0;
    double returned_alpha_msun = 0.0;
    double returned_iron_msun = 0.0;
    double feedback_energy_erg = 0.0;
};

struct StarBookkeepingDelta {
    double old_mass_msun = 0.0;
    double new_mass_msun = 0.0;
    double returned_mass_msun = 0.0;
    double remnant_change_msun = 0.0;

    double returned_metals_msun = 0.0;
    double returned_alpha_msun = 0.0;
    double returned_iron_msun = 0.0;
    double feedback_energy_erg = 0.0;

    std::vector<PerChannelBudget> per_channel;
};

struct BookkeepingOutput {
    std::vector<StarBookkeepingDelta> per_star;
    std::vector<PerChannelBudget> channel_totals;
    EvolutionTableProvenance provenance;
};

class StellarEvolutionBookkeeper {
  public:
    static EvolutionTable loadEvolutionTableCsv(const std::string& file_path,
                                                const EvolutionTableProvenance& provenance);

    StellarEvolutionBookkeeper(EvolutionTable table, ChannelWeights channel_weights);

    BookkeepingOutput advancePopulation(StarPopulationState& star_population,
                                        double timestep_yr,
                                        double conservation_tolerance) const;

    double interpolateCumulativeReturnFraction(double age_yr) const;

  private:
    struct InterpolatedCumulative {
        double return_fraction;
        double metals_fraction;
        double alpha_fraction;
        double iron_fraction;
        double energy_erg_per_msun;
    };

    InterpolatedCumulative interpolateAll(double age_yr) const;

    EvolutionTable m_table;
    ChannelWeights m_channel_weights;
};

}  // namespace cosmosim
