#include "module_3_forwards_futures.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>

using namespace QuantLib;

namespace Module3 {

ForwardFuturesCalculator::ForwardFuturesCalculator(double riskFreeRate, double dividendYield)
    : riskFreeRate_(riskFreeRate), dividendYield_(dividendYield) {
    Settings::instance().evaluationDate() = Date::todaysDate();
}

double ForwardFuturesCalculator::timeToMaturity(const Date& spotDate,
                                                const Date& maturityDate) const {
    return Actual365Fixed().yearFraction(spotDate, maturityDate);
}

double ForwardFuturesCalculator::forwardPrice(double spotPrice,
                                              double timeToMaturity,
                                              double riskFreeRate,
                                              double dividendYield) const {
    double r = std::isnan(riskFreeRate) ? riskFreeRate_ : riskFreeRate;
    double q = std::isnan(dividendYield) ? dividendYield_ : dividendYield;
    return spotPrice * std::exp((r - q) * timeToMaturity);
}

double ForwardFuturesCalculator::futuresPrice(double spotPrice,
                                              double timeToMaturity,
                                              double riskFreeRate,
                                              double dividendYield) const {
    return forwardPrice(spotPrice, timeToMaturity, riskFreeRate, dividendYield);
}

double ForwardFuturesCalculator::costOfCarry(double spotPrice,
                                             double forwardPrice,
                                             double timeToMaturity) const {
    if (timeToMaturity == 0.0 || spotPrice <= 0.0 || forwardPrice <= 0.0)
        return 0.0;
    return std::log(forwardPrice / spotPrice) / timeToMaturity;
}

double ForwardFuturesCalculator::forwardDiscountPremium(double spotPrice,
                                                        double forwardPrice) const {
    if (spotPrice == 0.0)
        return 0.0;
    return ((forwardPrice - spotPrice) / spotPrice) * 100.0;
}

PnlResult ForwardFuturesCalculator::pnlAtMaturity(double spotPriceToday,
                                                   double spotPriceMaturity,
                                                   bool longPosition,
                                                   double contractSize) const {
    double pnl = longPosition ?
        (spotPriceMaturity - spotPriceToday) * contractSize :
        (spotPriceToday - spotPriceMaturity) * contractSize;

    return PnlResult{
        pnl,
        (spotPriceToday != 0.0 ? pnl / (spotPriceToday * contractSize) * 100.0 : 0.0),
        longPosition ? "long" : "short"
    };
}

DataDrivenForwardCalculator::DataDrivenForwardCalculator(const std::string& csvFilePath,
                                                         double riskFreeRate,
                                                         double dividendYield)
    : ForwardFuturesCalculator(riskFreeRate, dividendYield),
      csvFilePath_(csvFilePath) {}

// ====================================================================
// MODIFIED LIVE INGESTION LOOP ONLY
// ====================================================================
std::vector<ForwardRow> DataDrivenForwardCalculator::computeForwardsForMaturities(const std::vector<int>& maturityMonths) const {
    std::ifstream file(csvFilePath_);
    if (!file.is_open()) {
        std::cerr << "Unable to open CSV file: " << csvFilePath_ << std::endl;
        return {};
    }

    std::string line, val;
    std::getline(file, line); // Read Python header row

    // Build a dynamic column index map to handle changes in data column order automatically
    std::map<std::string, int> colIdx;
    std::stringstream headerStream(line);
    int currentIdx = 0;
    while (std::getline(headerStream, val, ',')) {
        if (!val.empty() && val.back() == '\r') val.pop_back(); // Trim carriage return
        colIdx[val] = currentIdx++;
    }

    std::vector<ForwardRow> results;

    // Stream and process every active live contract row in the CSV file
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream lineStream(line);
        std::vector<std::string> columns;
        while (std::getline(lineStream, val, ',')) {
            columns.push_back(val);
        }

        // Parse metrics dynamically from the mapped column names
        double spotPrice = std::stod(columns[colIdx["spot_price"]]);
        double marketLtp = std::stod(columns[colIdx["ltp"]]);
        
        // Skip dead rows with zero pricing metrics (untraded illiquid strikes)
        if (marketLtp == 0.0) continue;

        // Extract dates and convert safely from string formats
        std::string evalDateStr = columns[colIdx["evaluation_date"]];
        std::string expiryDateStr = columns[colIdx["expiry_date"]];
        
        // Parsing layout: YYYY-MM-DD (Standard Pandas Output Format)
        Date evalDate(std::stoi(evalDateStr.substr(8, 2)), 
                      static_cast<Month>(std::stoi(evalDateStr.substr(5, 2))), 
                      std::stoi(evalDateStr.substr(0, 4)));
                      
        Date expiryDate(std::stoi(expiryDateStr.substr(8, 2)), 
                        static_cast<Month>(std::stoi(expiryDateStr.substr(5, 2))), 
                        std::stoi(expiryDateStr.substr(0, 4)));

        // Synchronize evaluation calendar date with QuantLib core global environment
        Settings::instance().evaluationDate() = evalDate;

        // Call your unchanged calculator math functions directly
        double T = timeToMaturity(evalDate, expiryDate);
        double fwdPrice = forwardPrice(spotPrice, T);
        double discountPremium = forwardDiscountPremium(spotPrice, fwdPrice);
        double carry = costOfCarry(spotPrice, fwdPrice, T);

        // Keep structural tracking parameters consistent with the original return schema (using arbitrary month tracker as 0)
        results.push_back(ForwardRow{0, expiryDate, T, spotPrice, fwdPrice, discountPremium, carry});
    }

    file.close();
    return results;
}

// Left as an unused signature helper to maintain header compatibility without compilation breaks
double DataDrivenForwardCalculator::parseCloseFromCsv(std::string& dateString,
                                                      double& closePrice,
                                                      Date& lastDate) const {
    return -1.0; 
}

} // namespace Module3