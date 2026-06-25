#include "module_1_greeks_solver.hpp"
#include <ql/quantlib.hpp>
#include <algorithm>
#include <cctype>

OptionResults calculate_european_option(
    double spot,
    double strike,
    double volatility,
    double risk_free_rate,
    double dividend_yield,
    int days_to_expiry,
    const std::string& option_type
) {
    using namespace QuantLib;
    OptionResults results = {0.0, 0.0, 0.0, 0.0, 0.0};

    try {
        // Match anchor temporal context to June 26, 2026
        Date today(26, June, 2026);
        Settings::instance().evaluationDate() = today;

        DayCounter day_counter = Actual365Fixed();
        Calendar calendar = NullCalendar();
        Date expiry_date = today + days_to_expiry;

        ext::shared_ptr<Quote> spot_quote(new SimpleQuote(spot));
        Handle<Quote> spot_handle(spot_quote);

        ext::shared_ptr<YieldTermStructure> rate_curve(new FlatForward(today, risk_free_rate, day_counter));
        Handle<YieldTermStructure> rate_handle(rate_curve);

        ext::shared_ptr<YieldTermStructure> div_curve(new FlatForward(today, dividend_yield, day_counter));
        Handle<YieldTermStructure> div_handle(div_curve);

        ext::shared_ptr<BlackVolTermStructure> vol_surface(new BlackConstantVol(today, calendar, volatility, day_counter));
        Handle<BlackVolTermStructure> vol_handle(vol_surface);

        ext::shared_ptr<BlackScholesMertonProcess> bsm_process(
            new BlackScholesMertonProcess(spot_handle, div_handle, rate_handle, vol_handle)
        );

        std::string upper_type = option_type;
        std::transform(upper_type.begin(), upper_type.end(), upper_type.begin(), ::toupper);
        Option::Type type = (upper_type == "PUT") ? Option::Put : Option::Call;

        ext::shared_ptr<PlainVanillaPayoff> payoff(new PlainVanillaPayoff(type, strike));
        ext::shared_ptr<Exercise> exercise(new EuropeanExercise(expiry_date));

        VanillaOption option(payoff, exercise);
        ext::shared_ptr<PricingEngine> engine(new AnalyticEuropeanEngine(bsm_process));
        option.setPricingEngine(engine);

        results.price = option.NPV();
        results.delta = option.delta();
        results.gamma = option.gamma();
        results.vega  = option.vega();
        results.theta = option.theta();

    } catch (const std::exception& e) {
        results.price = -1.0; 
    }
    return results;
}