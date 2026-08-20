#include "module_1_greeks_solver.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "====================================================\n";
    std::cout << " ONITRO QUANTLIB MODULE-1 VALIDATION RUNNER\n";
    std::cout << "====================================================\n";

    double spot = 2515.0;
    double strike = 2520.0;
    double vol = 0.24;          
    double rate = 0.07;         
    double dividend = 0.01;     
    int days_to_expiry = 32;    

    OptionResults call_res = calculate_european_option(spot, strike, vol, rate, dividend, days_to_expiry, "CALL");

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Contract Style: European Call Option\n";
    std::cout << "Spot Price:     " << spot << "\n";
    std::cout << "Strike Price:   " << strike << "\n\n";
    std::cout << "--- Computed Theoretical Outputs ---\n";
    std::cout << "Option Price:   " << call_res.price << "\n";
    std::cout << "Delta (Δ):      " << call_res.delta << "\n";
    std::cout << "Gamma (Γ):      " << call_res.gamma << "\n";
    std::cout << "Vega:           " << call_res.vega  << "\n";
    std::cout << "Theta (Θ/Year): " << call_res.theta << "\n";
    std::cout << "====================================================\n";

    return 0;
}