#pragma once

#include <ql/quantlib.hpp>
#include <string>
#include <vector>

namespace Module3 {

struct YtmResult {
    double ytm;
    int iterations;
    std::string convergence;
    double bondPriceAtYtm;
    std::string error;
};

struct DurationConvexityResult {
    double macaulayDuration;
    double modifiedDuration;
    double convexity;
};

class BondPricingCalculator {
public:
    static double zeroCouponBondPrice(double faceValue,
                                      double yieldRate,
                                      double yearsToMaturity);

    static double couponBondPrice(double faceValue,
                                  double couponRate,
                                  double yieldRate,
                                  double yearsToMaturity,
                                  int paymentFrequency = 2);

    static std::vector<double> bondPriceAtDifferentYields(double faceValue,
                                                          double couponRate,
                                                          double currentYield,
                                                          double yearsToMaturity,
                                                          int paymentFrequency = 2,
                                                          double yieldRange = 0.02);
};

class YieldToMaturitySolver {
public:
    static double ytmZeroCouponBond(double faceValue,
                                    double bondPrice,
                                    double yearsToMaturity);

    static YtmResult ytmCouponBond(double faceValue,
                                   double couponRate,
                                   double bondPrice,
                                   double yearsToMaturity,
                                   int paymentFrequency = 2,
                                   double initialGuess = 0.05,
                                   double tolerance = 1e-8,
                                   int maxIterations = 100);
};

class DurationConvexity {
public:
    static double macaulayDuration(double faceValue,
                                   double couponRate,
                                   double yieldRate,
                                   double yearsToMaturity,
                                   int paymentFrequency = 2);

    static double modifiedDuration(double macaulayDuration,
                                   double yieldRate,
                                   int paymentFrequency = 2);

    static double convexity(double faceValue,
                            double couponRate,
                            double yieldRate,
                            double yearsToMaturity,
                            int paymentFrequency = 2);

    static DurationConvexityResult bondMetrics(double faceValue,
                                               double couponRate,
                                               double yieldRate,
                                               double yearsToMaturity,
                                               int paymentFrequency = 2);

    static double priceChangeEstimate(double bondPrice,
                                      double modifiedDuration,
                                      double convexity,
                                      double yieldChange);
};

} // namespace Module3
