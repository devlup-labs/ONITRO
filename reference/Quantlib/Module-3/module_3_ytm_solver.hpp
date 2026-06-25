#pragma once

#include "module_3_bonds.hpp"
#include <ql/quantlib.hpp>
#include <map>
#include <string>
#include <vector>

namespace Module3 {

struct SensitivityRow {
    double yieldChange;
    double newYield;
    double actualPrice;
    double estimatedPrice;
    double estimationError;
    double priceChangePct;
};

struct ScenarioResult {
    double scenarioYield;
    double bondPrice;
    double pnl;
    double pnlPercentage;
};

class ComprehensiveYTMAnalyzer {
public:
    ComprehensiveYTMAnalyzer(double faceValue = 1000.0,
                             double couponRate = 0.06,
                             double currentYield = 0.05,
                             double yearsToMaturity = 5.0,
                             int paymentFrequency = 2);

    void printSummary() const;

    std::vector<SensitivityRow> priceSensitivityAnalysis(const std::vector<double>& yieldChanges = {}) const;

    std::map<std::string, ScenarioResult> scenarioAnalysis(const std::map<std::string, double>& yieldScenarios = {}) const;

    YtmResult ytmFromPrice(double bondPrice,
                           double tolerance = 1e-8,
                           int maxIterations = 100) const;

    std::map<std::string, std::string> exportAnalysis(const std::string& outputPath) const;

private:
    double faceValue_;
    double couponRate_;
    double currentYield_;
    double yearsToMaturity_;
    int paymentFrequency_;
    double currentPrice_;
    double macaulayDuration_;
    double modifiedDuration_;
    double convexity_;

    double couponBondPrice(double yieldRate) const;
};

} // namespace Module3
