#include <ql/quantlib.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace QuantLib;

struct OptionData
{
    double spot;
    double strike;
    std::string optionType;
    double maturity;
    double riskFreeRate;
    double dividendYield;
    double marketPrice;
};

int main()
{
    std::ifstream file("data/options_tcs.csv");

    if (!file.is_open())
    {
        std::cout << "Could not open input CSV\n";
        return 1;
    }

    std::ofstream outFile("data/options_tcs_iv.csv");

    if (!outFile.is_open())
    {
        std::cout << "Could not create output CSV\n";
        return 1;
    }

    // Write header
    outFile << "Spot,Strike,OptionType,MarketPrice,"
            << "TimeToMaturity,RiskFreeRate,"
            << "DividendYield,ImpliedVolatility\n";

    // Skip header of input CSV
    std::string header;
    std::getline(file, header);

    Calendar calendar = India();

    Date today = Date::todaysDate();
    Settings::instance().evaluationDate() = today;

    DayCounter dayCounter = Actual365Fixed();

    std::string row;
    int rowNo = 0;

    while (std::getline(file, row))
    {
        rowNo++;

        std::stringstream ss(row);
        std::string cell;
        std::vector<std::string> fields;

        while (std::getline(ss, cell, ','))
        {
            fields.push_back(cell);
        }

        if (fields.size() < 22)
            continue;

        OptionData option;

        option.spot = std::stod(fields[1]);
        option.strike = std::stod(fields[2]);
        option.optionType = fields[4];
        option.marketPrice = std::stod(fields[21]);
        option.maturity = std::stod(fields[18]);
        option.riskFreeRate = std::stod(fields[19]);
        option.dividendYield = std::stod(fields[20]);

        std::cout << "\n=========================================\n";
        std::cout << "Row : " << rowNo << std::endl;
        std::cout << "Spot Price      : " << option.spot << std::endl;
        std::cout << "Strike Price    : " << option.strike << std::endl;
        std::cout << "Option Type     : " << option.optionType << std::endl;
        std::cout << "Market Price    : " << option.marketPrice << std::endl;

        try
        {
            Handle<YieldTermStructure> riskFreeCurve(
                ext::make_shared<FlatForward>(
                    today,
                    option.riskFreeRate,
                    dayCounter));

            Handle<YieldTermStructure> dividendCurve(
                ext::make_shared<FlatForward>(
                    today,
                    option.dividendYield,
                    dayCounter));

            Handle<BlackVolTermStructure> volatility(
                ext::make_shared<BlackConstantVol>(
                    today,
                    calendar,
                    0.20,
                    dayCounter));

            Handle<Quote> spot(
                ext::make_shared<SimpleQuote>(
                    option.spot));

            auto process =
                ext::make_shared<BlackScholesMertonProcess>(
                    spot,
                    dividendCurve,
                    riskFreeCurve,
                    volatility);

            Option::Type type =
                (option.optionType == "CALL")
                    ? Option::Call
                    : Option::Put;

            auto payoff =
                ext::make_shared<PlainVanillaPayoff>(
                    type,
                    option.strike);

            Date maturityDate =
                today + Integer(option.maturity * 365);

            auto exercise =
                ext::make_shared<EuropeanExercise>(
                    maturityDate);

            VanillaOption europeanOption(
                payoff,
                exercise);

            europeanOption.setPricingEngine(
                ext::make_shared<AnalyticEuropeanEngine>(
                    process));

            Volatility impliedVol =
                europeanOption.impliedVolatility(
                    option.marketPrice,
                    process);

            std::cout << "Implied Volatility : "
                      << impliedVol * 100
                      << "%\n";

            outFile
                << option.spot << ","
                << option.strike << ","
                << option.optionType << ","
                << option.marketPrice << ","
                << option.maturity << ","
                << option.riskFreeRate << ","
                << option.dividendYield << ","
                << impliedVol * 100
                << "\n";
        }
        catch (std::exception &e)
        {
            std::cout << "Failed : "
                      << e.what()
                      << std::endl;

            outFile
                << option.spot << ","
                << option.strike << ","
                << option.optionType << ","
                << option.marketPrice << ","
                << option.maturity << ","
                << option.riskFreeRate << ","
                << option.dividendYield << ","
                << "NA"
                << "\n";
        }
    }

    file.close();
    outFile.close();

    std::cout << "\n=========================================\n";
    std::cout << "Finished processing all options.\n";
    std::cout << "Results saved to data/options_tcs_iv.csv\n";
    std::cout << "=========================================\n";

    return 0;
}