#include "module_3_ytm_solver.hpp"
#include "module_3_bonds.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <vector>
#include <sstream>
#include <cmath>

using namespace QuantLib;

namespace Module3 {

namespace {
    Date today() {
        Date d = Date::todaysDate();
        Settings::instance().evaluationDate() = d;
        return d;
    }

    Handle<YieldTermStructure> flatYieldCurve(const Date& referenceDate,
                                              double yieldRate) {
        return Handle<YieldTermStructure>(boost::shared_ptr<YieldTermStructure>(
            new FlatForward(referenceDate, yieldRate, Actual365Fixed())));
    }

    boost::shared_ptr<FixedRateBond> buildCouponBond(double faceValue,
                                                     double couponRate,
                                                     double yearsToMaturity,
                                                     int paymentFrequency) {
        Date issueDate = today();
        Date maturity = issueDate + Period(int(std::round(yearsToMaturity * 365.0)), Days);
        Schedule schedule(issueDate,
                          maturity,
                          Period(paymentFrequency == 1 ? Annual : Semiannual),
                          TARGET(),
                          Unadjusted,
                          Unadjusted,
                          DateGeneration::Backward,
                          false);

        return boost::shared_ptr<FixedRateBond>(new FixedRateBond(
            2,
            faceValue,
            schedule,
            std::vector<Rate>{couponRate},
            ActualActual(ActualActual::ISMA),
            Following,
            faceValue,
            issueDate));
    }
}

ComprehensiveYTMAnalyzer::ComprehensiveYTMAnalyzer(double faceValue,
                                                   double couponRate,
                                                   double currentYield,
                                                   double yearsToMaturity,
                                                   int paymentFrequency)
    : faceValue_(faceValue),
      couponRate_(couponRate),
      currentYield_(currentYield),
      yearsToMaturity_(yearsToMaturity),
      paymentFrequency_(paymentFrequency) {
    currentPrice_ = couponBondPrice(currentYield_);
    macaulayDuration_ = DurationConvexity::macaulayDuration(faceValue_, couponRate_, currentYield_, yearsToMaturity_, paymentFrequency_);
    modifiedDuration_ = DurationConvexity::modifiedDuration(macaulayDuration_, currentYield_, paymentFrequency_);
    convexity_ = DurationConvexity::convexity(faceValue_, couponRate_, currentYield_, yearsToMaturity_, paymentFrequency_);
}

double ComprehensiveYTMAnalyzer::couponBondPrice(double yieldRate) const {
    return BondPricingCalculator::couponBondPrice(faceValue_, couponRate_, yieldRate, yearsToMaturity_, paymentFrequency_);
}

void ComprehensiveYTMAnalyzer::printSummary() const {
    std::cout << "\n=== BOND SUMMARY ===\n";
    std::cout << "Face Value: " << faceValue_ << "\n";
    std::cout << "Coupon Rate: " << couponRate_ * 100.0 << "%\n";
    std::cout << "Current Yield: " << currentYield_ * 100.0 << "%\n";
    std::cout << "Years to Maturity: " << yearsToMaturity_ << "\n";
    std::cout << "Current Price: " << currentPrice_ << "\n";
    std::cout << "Macaulay Duration: " << macaulayDuration_ << "\n";
    std::cout << "Modified Duration: " << modifiedDuration_ << "\n";
    std::cout << "Convexity: " << convexity_ << "\n";
}

std::vector<SensitivityRow> ComprehensiveYTMAnalyzer::priceSensitivityAnalysis(const std::vector<double>& yieldChanges) const {
    std::vector<double> changes = yieldChanges;
    if (changes.empty()) {
        changes = {-0.02, -0.01, -0.005, 0.0, 0.005, 0.01, 0.02};
    }

    std::vector<SensitivityRow> results;
    for (double dy : changes) {
        double newYield = currentYield_ + dy;
        double actualPrice = couponBondPrice(newYield);
        double estimatedPrice = currentPrice_ + DurationConvexity::priceChangeEstimate(currentPrice_, modifiedDuration_, convexity_, dy);
        results.push_back({dy * 100.0,
                           newYield * 100.0,
                           actualPrice,
                           estimatedPrice,
                           std::abs(actualPrice - estimatedPrice),
                           ((actualPrice - currentPrice_) / currentPrice_) * 100.0});
    }
    return results;
}

std::map<std::string, ScenarioResult> ComprehensiveYTMAnalyzer::scenarioAnalysis(const std::map<std::string, double>& yieldScenarios) const {
    std::map<std::string, double> scenarios = yieldScenarios;
    if (scenarios.empty()) {
        scenarios = { {"Bear_Market", 0.07}, {"Base_Case", 0.05}, {"Bull_Market", 0.03} };
    }

    std::map<std::string, ScenarioResult> results;
    for (const auto& kv : scenarios) {
        double price = couponBondPrice(kv.second);
        double pnl = price - currentPrice_;
        results[kv.first] = {kv.second, price, pnl, (pnl / currentPrice_) * 100.0};
    }
    return results;
}

YtmResult ComprehensiveYTMAnalyzer::ytmFromPrice(double bondPrice,
                                                 double tolerance,
                                                 int maxIterations) const {
    return YieldToMaturitySolver::ytmCouponBond(faceValue_, couponRate_, bondPrice, yearsToMaturity_, paymentFrequency_, currentYield_, tolerance, maxIterations);
}

std::map<std::string, std::string> ComprehensiveYTMAnalyzer::exportAnalysis(const std::string& outputPath) const {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return {{"status", "failed"}, {"message", "Unable to open file"}};
    }

    auto scenarioResults = scenarioAnalysis();
    out << "{\n";
    out << "  \"bond_summary\": {\n";
    out << "    \"face_value\": " << faceValue_ << ",\n";
    out << "    \"coupon_rate\": " << couponRate_ << ",\n";
    out << "    \"current_yield\": " << currentYield_ << ",\n";
    out << "    \"current_price\": " << currentPrice_ << "\n";
    out << "  },\n";
    out << "  \"scenarios\": {\n";

    bool first = true;
    for (const auto& kv : scenarioResults) {
        if (!first) out << ",\n";
        first = false;
        out << "    \"" << kv.first << "\": {\n";
        out << "      \"scenario_yield\": " << kv.second.scenarioYield << ",\n";
        out << "      \"bond_price\": " << kv.second.bondPrice << ",\n";
        out << "      \"pnl\": " << kv.second.pnl << ",\n";
        out << "      \"pnl_percentage\": " << kv.second.pnlPercentage << "\n";
        out << "    }";
    }

    out << "\n  }\n";
    out << "}\n";
    out.close();

    return {{"status", "success"}, {"path", outputPath}};
}

} // namespace Module3
