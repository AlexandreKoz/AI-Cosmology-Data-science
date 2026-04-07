#include "cosmosim/stellar_evolution_bookkeeping.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cosmosim {

namespace {

std::vector<double> splitCsvLine(const std::string& line) {
    std::vector<double> values;
    std::stringstream line_stream(line);
    std::string token;
    while (std::getline(line_stream, token, ',')) {
        values.push_back(std::stod(token));
    }
    return values;
}

void validateTableShape(const EvolutionTable& table) {
    const std::size_t n = table.age_yr.size();
    if (n < 2) {
        throw std::invalid_argument("Evolution table must contain at least two rows");
    }
    if (table.cumulative_return_fraction.size() != n ||
        table.cumulative_metals_fraction.size() != n ||
        table.cumulative_alpha_fraction.size() != n ||
        table.cumulative_iron_fraction.size() != n ||
        table.cumulative_energy_erg_per_msun.size() != n) {
        throw std::invalid_argument("Evolution table columns are not size-consistent");
    }
    for (std::size_t i = 1; i < n; ++i) {
        if (table.age_yr[i] <= table.age_yr[i - 1]) {
            throw std::invalid_argument("Evolution table age grid must be strictly increasing");
        }
    }
}

}  // namespace

bool ChannelWeights::isNormalized(double tolerance) const {
    const double sum = agb_fraction + ccsn_fraction + snia_fraction;
    return std::abs(sum - 1.0) <= tolerance && agb_fraction >= 0.0 && ccsn_fraction >= 0.0 &&
           snia_fraction >= 0.0;
}

std::size_t StarPopulationState::size() const {
    return current_mass_msun.size();
}

EvolutionTable StellarEvolutionBookkeeper::loadEvolutionTableCsv(
    const std::string& file_path, const EvolutionTableProvenance& provenance) {
    std::ifstream in(file_path);
    if (!in.good()) {
        throw std::runtime_error("Failed to open evolution table: " + file_path);
    }

    EvolutionTable table;
    table.provenance = provenance;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto values = splitCsvLine(line);
        if (values.size() != 6) {
            throw std::invalid_argument("Evolution table rows must have 6 numeric columns");
        }
        table.age_yr.push_back(values[0]);
        table.cumulative_return_fraction.push_back(values[1]);
        table.cumulative_metals_fraction.push_back(values[2]);
        table.cumulative_alpha_fraction.push_back(values[3]);
        table.cumulative_iron_fraction.push_back(values[4]);
        table.cumulative_energy_erg_per_msun.push_back(values[5]);
    }

    validateTableShape(table);
    return table;
}

StellarEvolutionBookkeeper::StellarEvolutionBookkeeper(EvolutionTable table,
                                                       ChannelWeights channel_weights)
    : m_table(std::move(table)), m_channel_weights(channel_weights) {
    validateTableShape(m_table);
    if (!m_channel_weights.isNormalized()) {
        throw std::invalid_argument("Channel weights must be non-negative and normalized");
    }
}

StellarEvolutionBookkeeper::InterpolatedCumulative StellarEvolutionBookkeeper::interpolateAll(
    double age_yr) const {
    const double clamped_age = std::clamp(age_yr, m_table.age_yr.front(), m_table.age_yr.back());
    const auto upper =
        std::lower_bound(m_table.age_yr.begin(), m_table.age_yr.end(), clamped_age);
    if (upper == m_table.age_yr.begin()) {
        return {m_table.cumulative_return_fraction.front(),
                m_table.cumulative_metals_fraction.front(),
                m_table.cumulative_alpha_fraction.front(),
                m_table.cumulative_iron_fraction.front(),
                m_table.cumulative_energy_erg_per_msun.front()};
    }
    if (upper == m_table.age_yr.end()) {
        return {m_table.cumulative_return_fraction.back(),
                m_table.cumulative_metals_fraction.back(),
                m_table.cumulative_alpha_fraction.back(),
                m_table.cumulative_iron_fraction.back(),
                m_table.cumulative_energy_erg_per_msun.back()};
    }

    const std::size_t idx1 = static_cast<std::size_t>(std::distance(m_table.age_yr.begin(), upper));
    const std::size_t idx0 = idx1 - 1;

    const double t0 = m_table.age_yr[idx0];
    const double t1 = m_table.age_yr[idx1];
    const double w = (clamped_age - t0) / (t1 - t0);

    auto blend = [w](double v0, double v1) { return v0 + w * (v1 - v0); };

    return {blend(m_table.cumulative_return_fraction[idx0], m_table.cumulative_return_fraction[idx1]),
            blend(m_table.cumulative_metals_fraction[idx0], m_table.cumulative_metals_fraction[idx1]),
            blend(m_table.cumulative_alpha_fraction[idx0], m_table.cumulative_alpha_fraction[idx1]),
            blend(m_table.cumulative_iron_fraction[idx0], m_table.cumulative_iron_fraction[idx1]),
            blend(m_table.cumulative_energy_erg_per_msun[idx0],
                  m_table.cumulative_energy_erg_per_msun[idx1])};
}

double StellarEvolutionBookkeeper::interpolateCumulativeReturnFraction(double age_yr) const {
    return interpolateAll(age_yr).return_fraction;
}

BookkeepingOutput StellarEvolutionBookkeeper::advancePopulation(StarPopulationState& star_population,
                                                                double timestep_yr,
                                                                double conservation_tolerance) const {
    if (star_population.birth_mass_msun.size() != star_population.size() ||
        star_population.age_yr.size() != star_population.size() ||
        star_population.metallicity_mass_fraction.size() != star_population.size()) {
        throw std::invalid_argument("StarPopulationState arrays must have identical lengths");
    }
    if (timestep_yr <= 0.0) {
        throw std::invalid_argument("timestep_yr must be strictly positive");
    }

    BookkeepingOutput output;
    output.provenance = m_table.provenance;
    output.per_star.resize(star_population.size());
    output.channel_totals.resize(static_cast<std::size_t>(YieldChannel::count));

    for (std::size_t i = 0; i < star_population.size(); ++i) {
        const double age_old_yr = star_population.age_yr[i];
        const double age_new_yr = age_old_yr + timestep_yr;

        const auto old_cumulative = interpolateAll(age_old_yr);
        const auto new_cumulative = interpolateAll(age_new_yr);

        const double delta_return_fraction =
            std::max(0.0, new_cumulative.return_fraction - old_cumulative.return_fraction);
        const double delta_metals_fraction =
            std::max(0.0, new_cumulative.metals_fraction - old_cumulative.metals_fraction);
        const double delta_alpha_fraction =
            std::max(0.0, new_cumulative.alpha_fraction - old_cumulative.alpha_fraction);
        const double delta_iron_fraction =
            std::max(0.0, new_cumulative.iron_fraction - old_cumulative.iron_fraction);
        const double delta_energy_erg_per_msun =
            std::max(0.0,
                     new_cumulative.energy_erg_per_msun - old_cumulative.energy_erg_per_msun);

        const double birth_mass_msun = star_population.birth_mass_msun[i];
        const double old_mass_msun = star_population.current_mass_msun[i];

        const double returned_mass_msun = birth_mass_msun * delta_return_fraction;
        const double returned_metals_msun = birth_mass_msun * delta_metals_fraction;
        const double returned_alpha_msun = birth_mass_msun * delta_alpha_fraction;
        const double returned_iron_msun = birth_mass_msun * delta_iron_fraction;
        const double feedback_energy_erg = birth_mass_msun * delta_energy_erg_per_msun;

        const double new_mass_msun = std::max(0.0, old_mass_msun - returned_mass_msun);
        const double remnant_change_msun = old_mass_msun - new_mass_msun - returned_mass_msun;

        if (std::abs(remnant_change_msun) > conservation_tolerance) {
            throw std::runtime_error("Mass conservation tolerance violated in bookkeeping step");
        }

        StarBookkeepingDelta delta;
        delta.old_mass_msun = old_mass_msun;
        delta.new_mass_msun = new_mass_msun;
        delta.returned_mass_msun = returned_mass_msun;
        delta.remnant_change_msun = remnant_change_msun;
        delta.returned_metals_msun = returned_metals_msun;
        delta.returned_alpha_msun = returned_alpha_msun;
        delta.returned_iron_msun = returned_iron_msun;
        delta.feedback_energy_erg = feedback_energy_erg;
        delta.per_channel.resize(static_cast<std::size_t>(YieldChannel::count));

        const double channel_weights[] = {m_channel_weights.agb_fraction, m_channel_weights.ccsn_fraction,
                                          m_channel_weights.snia_fraction};

        for (std::size_t channel = 0; channel < static_cast<std::size_t>(YieldChannel::count);
             ++channel) {
            PerChannelBudget budget;
            const double f = channel_weights[channel];
            budget.returned_mass_msun = returned_mass_msun * f;
            budget.returned_metals_msun = returned_metals_msun * f;
            budget.returned_alpha_msun = returned_alpha_msun * f;
            budget.returned_iron_msun = returned_iron_msun * f;
            budget.feedback_energy_erg = feedback_energy_erg * f;
            delta.per_channel[channel] = budget;

            output.channel_totals[channel].returned_mass_msun += budget.returned_mass_msun;
            output.channel_totals[channel].returned_metals_msun += budget.returned_metals_msun;
            output.channel_totals[channel].returned_alpha_msun += budget.returned_alpha_msun;
            output.channel_totals[channel].returned_iron_msun += budget.returned_iron_msun;
            output.channel_totals[channel].feedback_energy_erg += budget.feedback_energy_erg;
        }

        star_population.current_mass_msun[i] = new_mass_msun;
        star_population.age_yr[i] = age_new_yr;
        output.per_star[i] = delta;
    }

    return output;
}

}  // namespace cosmosim
