#ifndef MODULE_1_GREEKS_SOLVER_HPP
#define MODULE_1_GREEKS_SOLVER_HPP

#include <string>

// Structure to bundle theoretical option metrics neatly
struct OptionResults {
    double price;
    double delta;
    double gamma;
    double vega;
    double theta;
};

// High-level wrapper function handling raw numbers
OptionResults calculate_european_option(
    double spot,
    double strike,
    double volatility,
    double risk_free_rate,
    double dividend_yield,
    int days_to_expiry,
    const std::string& option_type
);

#endif // MODULE_1_GREEKS_SOLVER_HPP