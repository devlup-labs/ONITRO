#pragma once

#include <cmath>
#include <limits>
#include <ql/quantlib.hpp>
#include <string>
#include <vector>

namespace Module3 {

struct PnlResult {
    double pnl;
    double pnlPercentage;
    std::string positionType;
};

struct ForwardRow {
    int maturityMonths;
    QuantLib::Date maturityDate;
    double timeToMaturity;
    double spotPrice;
    double forwardPrice;
    double forwardDiscountPremium;
    double costOfCarry;
};

class ForwardFuturesCalculator {
public:
    ForwardFuturesCalculator(double riskFreeRate = 0.06, double dividendYield = 0.01);

    double timeToMaturity(const QuantLib::Date& spotDate,
                          const QuantLib::Date& maturityDate) const;

    double forwardPrice(double spotPrice,
                        double timeToMaturity,
                        double riskFreeRate = std::numeric_limits<double>::quiet_NaN(),
                        double dividendYield = std::numeric_limits<double>::quiet_NaN()) const;

    double futuresPrice(double spotPrice,
                        double timeToMaturity,
                        double riskFreeRate = std::numeric_limits<double>::quiet_NaN(),
                        double dividendYield = std::numeric_limits<double>::quiet_NaN()) const;

    double costOfCarry(double spotPrice,
                       double forwardPrice,
                       double timeToMaturity) const;

    double forwardDiscountPremium(double spotPrice,
                                  double forwardPrice) const;

    PnlResult pnlAtMaturity(double spotPriceToday,
                            double spotPriceMaturity,
                            bool longPosition = true,
                            double contractSize = 1.0) const;

private:
    double riskFreeRate_;
    double dividendYield_;
};

class DataDrivenForwardCalculator : public ForwardFuturesCalculator {
public:
    DataDrivenForwardCalculator(const std::string& csvFilePath,
                                double riskFreeRate = 0.06,
                                double dividendYield = 0.01);

    std::vector<ForwardRow> computeForwardsForMaturities(const std::vector<int>& maturityMonths = {1, 3, 6, 12}) const;

private:
    std::string csvFilePath_;
    double parseCloseFromCsv(std::string& dateString, double& closePrice, QuantLib::Date& lastDate) const;
};

} // namespace Module3
