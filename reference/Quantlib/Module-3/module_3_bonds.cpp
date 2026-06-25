#include "module_3_bonds.hpp"

#include <ql/quantlib.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
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

double BondPricingCalculator::zeroCouponBondPrice(double faceValue,
                                                  double yieldRate,
                                                  double yearsToMaturity) {
    Date d = today();
    Date maturity = d + Period(int(std::round(yearsToMaturity * 365.0)), Days);

    ZeroCouponBond bond(2,
                        TARGET(),
                        faceValue,
                        maturity,
                        Following,
                        faceValue,
                        d);

    Handle<YieldTermStructure> curve = flatYieldCurve(d, yieldRate);
    boost::shared_ptr<PricingEngine> engine(new DiscountingBondEngine(curve));
    bond.setPricingEngine(engine);
    return bond.cleanPrice();
}

double BondPricingCalculator::couponBondPrice(double faceValue,
                                              double couponRate,
                                              double yieldRate,
                                              double yearsToMaturity,
                                              int paymentFrequency) {
    auto bond = buildCouponBond(faceValue, couponRate, yearsToMaturity, paymentFrequency);
    Handle<YieldTermStructure> curve = flatYieldCurve(today(), yieldRate);
    bond->setPricingEngine(boost::shared_ptr<PricingEngine>(new DiscountingBondEngine(curve)));
    return bond->cleanPrice();
}

std::vector<double> BondPricingCalculator::bondPriceAtDifferentYields(double faceValue,
                                                                      double couponRate,
                                                                      double currentYield,
                                                                      double yearsToMaturity,
                                                                      int paymentFrequency,
                                                                      double yieldRange) {
    std::vector<double> prices;
    for (double yield = currentYield - yieldRange; yield <= currentYield + yieldRange + 1e-12; yield += yieldRange / 10.0) {
        prices.push_back(couponBondPrice(faceValue, couponRate, yield, yearsToMaturity, paymentFrequency));
    }
    return prices;
}

double YieldToMaturitySolver::ytmZeroCouponBond(double faceValue,
                                                double bondPrice,
                                                double yearsToMaturity) {
    if (bondPrice <= 0.0 || yearsToMaturity == 0.0)
        return 0.0;
    return std::pow(faceValue / bondPrice, 1.0 / yearsToMaturity) - 1.0;
}

YtmResult YieldToMaturitySolver::ytmCouponBond(double faceValue,
                                               double couponRate,
                                               double bondPrice,
                                               double yearsToMaturity,
                                               int paymentFrequency,
                                               double initialGuess,
                                               double tolerance,
                                               int maxIterations) {
    auto bond = buildCouponBond(faceValue, couponRate, yearsToMaturity, paymentFrequency);
    Date d = today();
    Handle<YieldTermStructure> curve = flatYieldCurve(d, initialGuess);
    bond->setPricingEngine(boost::shared_ptr<PricingEngine>(new DiscountingBondEngine(curve)));

    try {
        Rate ytm = BondFunctions::yield(*bond,
                                       bondPrice,
                                       Actual365Fixed(),
                                       Compounded,
                                       Frequency(paymentFrequency),
                                       bond->settlementDate(),
                                       tolerance,
                                       maxIterations,
                                       initialGuess);

        double priceAtYtm = BondFunctions::cleanPrice(*bond,
                                                     ytm,
                                                     Actual365Fixed(),
                                                     Compounded,
                                                     Frequency(paymentFrequency),
                                                     bond->settlementDate());

        return {ytm, 0, "Success", priceAtYtm, ""};
    } catch (const std::exception& e) {
        return {0.0, maxIterations, "Failed", 0.0, e.what()};
    }
}

double DurationConvexity::macaulayDuration(double faceValue,
                                           double couponRate,
                                           double yieldRate,
                                           double yearsToMaturity,
                                           int paymentFrequency) {
    auto bond = buildCouponBond(faceValue, couponRate, yearsToMaturity, paymentFrequency);
    return BondFunctions::duration(*bond,
                                   yieldRate,
                                   Actual365Fixed(),
                                   Compounded,
                                   Frequency(paymentFrequency),
                                   Duration::Macaulay,
                                   bond->settlementDate());
}

double DurationConvexity::modifiedDuration(double macaulayDuration,
                                           double yieldRate,
                                           int paymentFrequency) {
    return macaulayDuration / (1.0 + yieldRate / paymentFrequency);
}

double DurationConvexity::convexity(double faceValue,
                                    double couponRate,
                                    double yieldRate,
                                    double yearsToMaturity,
                                    int paymentFrequency) {
    auto bond = buildCouponBond(faceValue, couponRate, yearsToMaturity, paymentFrequency);
    return BondFunctions::convexity(*bond,
                                   yieldRate,
                                   Actual365Fixed(),
                                   Compounded,
                                   Frequency(paymentFrequency),
                                   bond->settlementDate());
}

DurationConvexityResult DurationConvexity::bondMetrics(double faceValue,
                                                       double couponRate,
                                                       double yieldRate,
                                                       double yearsToMaturity,
                                                       int paymentFrequency) {
    double macaulay = macaulayDuration(faceValue, couponRate, yieldRate, yearsToMaturity, paymentFrequency);
    double modified = modifiedDuration(macaulay, yieldRate, paymentFrequency);
    double convex = convexity(faceValue, couponRate, yieldRate, yearsToMaturity, paymentFrequency);
    return {macaulay, modified, convex};
}

double DurationConvexity::priceChangeEstimate(double bondPrice,
                                              double modifiedDuration,
                                              double convexity,
                                              double yieldChange) {
    return -modifiedDuration * bondPrice * yieldChange + 0.5 * convexity * bondPrice * yieldChange * yieldChange;
}

// ====================================================================
// NEW REPLACEMENT LIVE INGESTION LOOP ONLY
// ====================================================================
void runLiveBondCalculationsFromOptionChain(const std::string& csvFilePath) {
    std::ifstream file(csvFilePath);
    if (!file.is_open()) {
        std::cerr << "[Bonds Ingestion] Failed to open CSV data path: " << csvFilePath << std::endl;
        return;
    }

    std::string line, val;
    std::getline(file, line); // Ingest pandas header record

    // Generate positional column lookup dictionary to bind data streams cleanly
    std::map<std::string, int> colIdx;
    std::stringstream headerStream(line);
    int currentIdx = 0;
    while (std::getline(headerStream, val, ',')) {
        if (!val.empty() && val.back() == '\r') val.pop_back(); // Clean line breaks
        colIdx[val] = currentIdx++;
    }

    // Capture the first data point row to map active global interest metrics
    if (std::getline(file, line)) {
        std::stringstream lineStream(line);
        std::vector<std::string> columns;
        while (std::getline(lineStream, val, ',')) {
            columns.push_back(val);
        }

        std::string ticker = columns[colIdx["underlying_symbol"]];
        double liveYieldRate = std::stod(columns[colIdx["risk_free_rate"]]);

        std::cout << "\n======================================================================" << std::endl;
        std::cout << "BOND ENGINE INGESTION COMPLETED FROM LIVE CHAIN MATRIX (" << ticker << ")" << std::endl;
        std::cout << "======================================================================" << std::endl;
        std::cout << "Extracted Live Market Reference Rate Yield: " << (liveYieldRate * 100.0) << "%" << std::endl;

        // Custom standard corporate/sovereign proxy parameters to run your engines
        double faceValue = 100.0;
        double couponRate = 0.075;     // 7.50% Coupon
        double yearsToMaturity = 5.0;  // 5 Year Tenure
        int frequency = 2;             // Semiannual compounding

        // Execute untouched calculator: Coupon Bond Pricing
        double cleanBondPrice = BondPricingCalculator::couponBondPrice(
            faceValue, couponRate, liveYieldRate, yearsToMaturity, frequency
        );
        std::cout << "-> Calculated Coupon Bond Clean Price: " << cleanBondPrice << std::endl;

        // Execute untouched calculator: Backing out YTM from a simulated traded discount price
        double marketTradedPrice = cleanBondPrice - 1.25; 
        YtmResult ytmRes = YieldToMaturitySolver::ytmCouponBond(
            faceValue, couponRate, marketTradedPrice, yearsToMaturity, frequency, 0.06, 1.0e-5, 1000
        );
        std::cout << "-> Implied Solver YTM (Traded Price @ " << marketTradedPrice << "): " << (ytmRes.ytm * 100.0) << "%" << std::endl;

        // Execute untouched calculator: Volatility Risk Parameters (Duration & Convexity Metrics)
        DurationConvexityResult metrics = DurationConvexity::bondMetrics(
            faceValue, couponRate, liveYieldRate, yearsToMaturity, frequency
        );
        std::cout << "-> Macaulay Duration: " << metrics.macaulayDuration << " Years" << std::endl;
        std::cout << "-> Modified Duration: " << metrics.modifiedDuration << " Years" << std::endl;
        std::cout << "-> Bond Convexity   : " << metrics.convexity << std::endl;
    }
    file.close();
}

} // namespace Module3

// ====================================================================
// PIPELINE RUNTIME TEST ENTRY
// ====================================================================
#ifndef MODULE3_LIBRARY_BUILD
int main() {
    // Routes dynamically to the data folder written by datacollector.py
    std::string csvPath = "./data/options_tcs.csv";
    Module3::runLiveBondCalculationsFromOptionChain(csvPath);
    return 0;
}
#endif