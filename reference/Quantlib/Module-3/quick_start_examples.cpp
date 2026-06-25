#include "module_3_bonds.hpp"
#include "module_3_forwards_futures.hpp"
#include "module_3_ytm_solver.hpp"

#include <iostream>
#include <vector>

using namespace Module3;

int main() {
    std::cout << "MODULE 3 QUICK START EXAMPLES\n";
    std::cout << "===========================\n\n";

    std::cout << "EXAMPLE 1: FORWARD & FUTURES PRICING\n";
    ForwardFuturesCalculator futuresCalc(0.06, 0.01);
    double spotPrice = 2000.0;
    for (int months : {1, 3, 6, 12}) {
        double t = months / 12.0;
        double forward = futuresCalc.forwardPrice(spotPrice, t);
        double premium = futuresCalc.forwardDiscountPremium(spotPrice, forward);
        std::cout << "  " << months << "M: Forward = " << forward << "  Premium = " << premium << "%\n";
    }

    std::cout << "\nEXAMPLE 2: ZERO-COUPON BOND PRICING\n";
    for (int maturity : {1, 3, 5, 10}) {
        double price = BondPricingCalculator::zeroCouponBondPrice(1000.0, 0.05, maturity);
        std::cout << "  " << maturity << "Y at 5% = " << price << "\n";
    }

    std::cout << "\nEXAMPLE 3: COUPON BOND PRICING\n";
    for (double couponRate : {0.04, 0.06, 0.08}) {
        double price = BondPricingCalculator::couponBondPrice(1000.0, couponRate, 0.05, 5, 2);
        std::cout << "  Coupon " << couponRate * 100.0 << "% = " << price << "\n";
    }

    std::cout << "\nEXAMPLE 4: YIELD-TO-MATURITY CALCULATION\n";
    for (double marketPrice : {900.0, 950.0, 1000.0, 1050.0}) {
        YtmResult result = YieldToMaturitySolver::ytmCouponBond(1000.0, 0.06, marketPrice, 5.0, 2);
        std::cout << "  Price " << marketPrice << " => YTM = " << result.ytm * 100.0 << "%  (" << result.convergence << ")\n";
    }

    std::cout << "\nEXAMPLE 5: DURATION & CONVEXITY\n";
    double macaulay = DurationConvexity::macaulayDuration(1000.0, 0.06, 0.05, 5.0, 2);
    double modified = DurationConvexity::modifiedDuration(macaulay, 0.05, 2);
    double convexity = DurationConvexity::convexity(1000.0, 0.06, 0.05, 5.0, 2);
    std::cout << "  Macaulay Duration = " << macaulay << "\n";
    std::cout << "  Modified Duration = " << modified << "\n";
    std::cout << "  Convexity = " << convexity << "\n";

    std::cout << "\nEXAMPLE 6: COMPREHENSIVE YTM ANALYZER\n";
    ComprehensiveYTMAnalyzer analyzer(1000.0, 0.06, 0.05, 5.0, 2);
    analyzer.printSummary();

    auto sensitivities = analyzer.priceSensitivityAnalysis();
    std::cout << "\n  Price sensitivity table:\n";
    for (const auto& row : sensitivities) {
        std::cout << "    Yield change " << row.yieldChange << "% => Actual = " << row.actualPrice << ", Estimated = " << row.estimatedPrice << "\n";
    }

    auto scenarios = analyzer.scenarioAnalysis();
    std::cout << "\n  Scenario analysis:\n";
    for (const auto& kv : scenarios) {
        std::cout << "    " << kv.first << ": Price = " << kv.second.bondPrice << ", P&L = " << kv.second.pnl << "\n";
    }

    auto ytmResult = analyzer.ytmFromPrice(950.0);
    std::cout << "\n  YTM from price 950 = " << ytmResult.ytm * 100.0 << "%\n";

    return 0;
}
