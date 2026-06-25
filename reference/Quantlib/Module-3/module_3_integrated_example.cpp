#include "module_3_bonds.hpp"
#include "module_3_forwards_futures.hpp"
#include "module_3_ytm_solver.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace Module3;

namespace {

struct OptionContract {
    std::string symbol;
    std::string expiry;
    std::string optionType;
    double strike = 0.0;
    double lastPrice = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    double midPrice = 0.0;
    double impliedVolatility = 0.0;
    long openInterest = 0;
    long volume = 0;
    
    // Injected fields from row metadata to power calculators dynamically
    double spotPrice = 0.0;
    double timeToMaturity = 0.0;
    double riskFreeRate = 0.0;
    double dividendYield = 0.0;
};

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;

    for (char ch : line) {
        if (ch == '"') {
            inQuotes = !inQuotes;
        } else if (ch == ',' && !inQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field.push_back(ch);
        }
    }

    fields.push_back(field);
    return fields;
}

double parseDouble(const std::string& value) {
    if (value.empty() || value == "nan" || value == "NaN")
        return 0.0;

    try {
        return std::stod(value);
    } catch (...) {
        return 0.0;
    }
}

long parseLong(const std::string& value) {
    if (value.empty() || value == "nan" || value == "NaN")
        return 0;

    try {
        return std::stol(value);
    } catch (...) {
        return 0;
    }
}

std::string getField(const std::map<std::string, std::size_t>& columns,
                     const std::vector<std::string>& row,
                     const std::string& name) {
    auto it = columns.find(name);
    if (it == columns.end() || it->second >= row.size())
        return "";
    return row[it->second];
}

std::vector<OptionContract> readOptionCsv(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "Option CSV not found: " << path << "\n";
        std::cout << "  Run: python3 data_collector.py\n\n";
        return {};
    }

    std::string line;
    if (!std::getline(file, line))
        return {};

    std::vector<std::string> header = splitCsvLine(line);
    std::map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < header.size(); ++i) {
        std::string colName = header[i];
        if (!colName.empty() && colName.back() == '\r') colName.pop_back(); // Clean windows line breaks safely
        columns[colName] = i;
    }

    std::vector<OptionContract> contracts;
    while (std::getline(file, line)) {
        if (line.empty())
            continue;

        std::vector<std::string> row = splitCsvLine(line);
        OptionContract contract;
        
        // ========================================================================
        // ALIGNED WITH CORRECT DATA COLLECTOR CSV FORMAT
        // ========================================================================
        contract.symbol            = getField(columns, row, "underlying_symbol");
        contract.expiry            = getField(columns, row, "expiry_date_str");
        contract.optionType        = getField(columns, row, "option_type");
        contract.strike            = parseDouble(getField(columns, row, "strike_price"));
        contract.lastPrice         = parseDouble(getField(columns, row, "ltp"));
        contract.bid               = parseDouble(getField(columns, row, "bid"));
        contract.ask               = parseDouble(getField(columns, row, "ask"));
        contract.midPrice          = parseDouble(getField(columns, row, "mid_price"));
        contract.impliedVolatility = parseDouble(getField(columns, row, "iv"));
        contract.openInterest      = parseLong(getField(columns, row, "open_interest"));
        contract.volume            = parseLong(getField(columns, row, "volume"));
        
        // Context parameters extraction
        contract.spotPrice         = parseDouble(getField(columns, row, "spot_price"));
        contract.timeToMaturity    = parseDouble(getField(columns, row, "time_to_maturity"));
        contract.riskFreeRate      = parseDouble(getField(columns, row, "risk_free_rate"));
        contract.dividendYield     = parseDouble(getField(columns, row, "dividend_yield"));

        contracts.push_back(contract);
    }

    return contracts;
}

std::string findOptionCsv(const std::string& filename) {
    for (const auto& directory : {"./data/", "../data/"}) {
        std::string path = std::string(directory) + filename;
        std::ifstream file(path);
        if (file.is_open())
            return path;
    }

    return std::string("./data/") + filename;
}

void printOptionSummary(const std::string& label,
                        const std::string& path,
                        const std::vector<OptionContract>& contracts) {
    std::cout << label << " option-chain data:\n";
    std::cout << "  Source: " << path << "\n";

    if (contracts.empty()) {
        std::cout << "  No option contracts loaded.\n\n";
        return;
    }

    int calls = 0;
    int puts = 0;
    long totalOpenInterest = 0;
    double ivSum = 0.0;
    int ivCount = 0;
    std::map<std::string, int> expiryCounts;

    for (const auto& contract : contracts) {
        if (contract.optionType == "CALL")
            ++calls;
        if (contract.optionType == "PUT")
            ++puts;
        if (!contract.expiry.empty())
            ++expiryCounts[contract.expiry];
        totalOpenInterest += contract.openInterest;
        if (contract.impliedVolatility > 0.0) {
            ivSum += contract.impliedVolatility;
            ++ivCount;
        }
    }

    std::vector<OptionContract> mostActive = contracts;
    std::sort(mostActive.begin(), mostActive.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.openInterest > rhs.openInterest;
    });

    std::cout << "  Contracts: " << contracts.size() << "  Calls: " << calls << "  Puts: " << puts << "\n";
    std::cout << "  Expiries: " << expiryCounts.size() << "  Open Interest: " << totalOpenInterest << "\n";
    if (ivCount > 0)
        std::cout << "  Average implied volatility: " << std::fixed << std::setprecision(2) << (ivSum / ivCount) * 100.0 << "%\n";

    std::cout << "  Top contracts by open interest:\n";
    std::size_t rowsToPrint = std::min<std::size_t>(5, mostActive.size());
    for (std::size_t i = 0; i < rowsToPrint; ++i) {
        const auto& row = mostActive[i];
        std::cout << "    " << row.symbol
                  << " " << row.optionType
                  << " expiry=" << row.expiry
                  << " strike=" << row.strike
                  << " last=" << row.lastPrice
                  << " bid=" << row.bid
                  << " ask=" << row.ask
                  << " OI=" << row.openInterest
                  << "\n";
    }

    std::cout << "\n";
}

} // namespace

int main() {
    std::cout << "MODULE 3 INTEGRATED EXAMPLE\n";
    std::cout << "=========================\n\n";

    std::string stableOptionsPath = findOptionCsv("options_tcs.csv");
    std::string volatileOptionsPath = findOptionCsv("options_icicibank.csv");
    
    std::vector<OptionContract> stableOptions = readOptionCsv(stableOptionsPath);
    std::vector<OptionContract> volatileOptions = readOptionCsv(volatileOptionsPath);

    printOptionSummary("Stable underlying", stableOptionsPath, stableOptions);
    printOptionSummary("Volatile underlying", volatileOptionsPath, volatileOptions);

    if (stableOptions.empty() || volatileOptions.empty()) {
        std::cerr << "[FATAL] Ingestion vectors failed to initialize. Aborting calculation engine.\n";
        return 1;
    }

    // ========================================================================
    // DYNAMIC DOCKING OF REAL TIME PARAMETERS (NO MORE HARDCODED LOOPS)
    // ========================================================================
    double stableSpot   = stableOptions[0].spotPrice;
    double volatileSpot = volatileOptions[0].spotPrice;
    double liveRfRate   = stableOptions[0].riskFreeRate;
    double liveDivYield = stableOptions[0].dividendYield;

    std::cout << "---------------------------------------------------------\n";
    std::cout << "LIVE PIPELINE RECOVERY COMPLETED\n";
    std::cout << "  Extracted Yield (r): " << (liveRfRate * 100.0) << "%\n";
    std::cout << "  Dividend Term  (q): " << (liveDivYield * 100.0) << "%\n";
    std::cout << "---------------------------------------------------------\n\n";

    ForwardFuturesCalculator forwardCalc(liveRfRate, liveDivYield);

    auto printForwards = [&](const std::string& label, double spot) {
        std::cout << label << "\n";
        for (int months : {1, 3, 6, 12}) {
            double t = months / 12.0;
            double forward = forwardCalc.forwardPrice(spot, t);
            double premium = forwardCalc.forwardDiscountPremium(spot, forward);
            double carry = forwardCalc.costOfCarry(spot, forward, t);
            std::cout << "  " << months << "M -> Forward=" << forward << " Premium=" << premium << "% Carry=" << carry << "\n";
        }
        std::cout << "\n";
    };

    printForwards("Stable stock forward pricing:", stableSpot);
    printForwards("Volatile stock forward pricing:", volatileSpot);

    std::cout << "Zero-coupon and coupon bond pricing (Using Live Market Yields):\n";
    double zcb = BondPricingCalculator::zeroCouponBondPrice(1000.0, liveRfRate, 5.0);
    double coupon = BondPricingCalculator::couponBondPrice(1000.0, 0.06, liveRfRate, 5.0, 2);
    std::cout << "  5Y zero-coupon price = " << zcb << "\n";
    std::cout << "  5Y coupon bond price = " << coupon << "\n\n";

    ComprehensiveYTMAnalyzer analyzer(1000.0, 0.06, liveRfRate, 5.0, 2);
    analyzer.printSummary();

    std::cout << "\nExporting scenario analysis to module3_analysis.json...\n";
    auto exportStatus = analyzer.exportAnalysis("module3_analysis.json");
    std::cout << "  status=" << exportStatus["status"] << " path=" << exportStatus["path"] << "\n";

    return 0;
}