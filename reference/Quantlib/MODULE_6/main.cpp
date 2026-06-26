#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

#include "EWMAStats.hpp"
#include "RollingStats.hpp"
#include "module_6_correlation.hpp"

struct StockData {
    std::string date;
    double close;
};

std::vector<StockData> readCSV(const std::string& filename)
{
    std::ifstream file(filename);

    if (!file.is_open())
        throw std::runtime_error("Cannot open " + filename);

    std::string line;
    std::getline(file, line);   // Skip header

    std::vector<StockData> data;

    while (std::getline(file, line))
    {
        std::stringstream ss(line);

        std::string date, open, high, low, close, adjclose, volume;

        std::getline(ss, date, ',');
        std::getline(ss, open, ',');
        std::getline(ss, high, ',');
        std::getline(ss, low, ',');
        std::getline(ss, close, ',');
        std::getline(ss, adjclose, ',');
        std::getline(ss, volume, ',');

        if (close.empty())
            continue;

        data.push_back({date, std::stod(close)});
    }

    return data;
}

int main()
{
    try
    {
        // Read Asset and Market CSVs
        std::vector<StockData> asset = readCSV("TCS.csv");
        std::vector<StockData> market = readCSV("NIFTY.csv"); // Change filename if needed

        if (asset.size() != market.size())
        {
            std::cerr << "Error: Asset and Market CSV sizes do not match.\n";
            return 1;
        }

        const int WINDOW = 20;

        onitro::EWMAStats ewma = onitro::EWMAStats::fromSpan(WINDOW);
        onitro::RollingStats rolling(WINDOW);

        // Compute log returns
        std::vector<double> assetReturns;
        std::vector<double> marketReturns;

        for (size_t i = 1; i < asset.size(); ++i)
        {
            assetReturns.push_back(
                std::log(asset[i].close / asset[i - 1].close));

            marketReturns.push_back(
                std::log(market[i].close / market[i - 1].close));
        }

        // Rolling Correlation
        std::vector<double> rollingCorr =
            CorrelationCalculator::calculateRollingCorrelation(
                assetReturns,
                marketReturns,
                WINDOW);

        // Rolling Beta
        std::vector<double> rollingBeta =
            CorrelationCalculator::calculateRollingBeta(
                assetReturns,
                marketReturns,
                WINDOW);

        // Output CSV
        std::ofstream out("output.csv");

        out << "Date,"
               "AssetPrice,"
               "MarketPrice,"
               "EWMA_Mean,"
               "EWMA_Stddev,"
               "EWMA_Zscore,"
               "Rolling_Mean,"
               "Rolling_Stddev,"
               "Rolling_Zscore,"
               "RollingCorrelation,"
               "RollingBeta\n";

        for (size_t i = 0; i < asset.size(); ++i)
        {
            ewma.push(asset[i].close);
            rolling.push(asset[i].close);

            if (!ewma.ready() || !rolling.ready())
                continue;

            double corr = 0.0;
            double beta = 0.0;

            // Returns start from index 1
            if (i >= WINDOW)
            {
                corr = rollingCorr[i - 1];
                beta = rollingBeta[i - 1];
            }

            out << asset[i].date << ","
                << asset[i].close << ","
                << market[i].close << ","
                << ewma.mean() << ","
                << ewma.stddev() << ","
                << ewma.zscore(asset[i].close) << ","
                << rolling.mean() << ","
                << rolling.stddev() << ","
                << rolling.zscore(asset[i].close) << ","
                << corr << ","
                << beta << "\n";
        }

        out.close();

        std::cout << "Done. Check output.csv\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}