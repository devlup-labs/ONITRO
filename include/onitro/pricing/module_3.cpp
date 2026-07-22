#include "module_3.hpp"
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <limits>

namespace ONITRO {
namespace Layer3 {
namespace FixedIncome {

    namespace {
        constexpr double YIELD_EPSILON = 1e-12;
        constexpr double DERIVATIVE_EPSILON = 1e-12;
        constexpr double MIN_YIELD = -0.999999;
        constexpr double INITIAL_YIELD = 0.05;
        constexpr std::size_t CHUNK_SIZE = 512;

        // Integer exponentiation by squaring.
        inline double ipow(double base, std::size_t exp) noexcept
        {
            double result = 1.0;

            while (exp != 0) {
                if (exp & 1U) {
                    result *= base;
                }
                base *= base;
                exp >>= 1U;
            }

            return result;
        }

        // Closed-form fixed-rate bond price and derivative.
        //
        // r = y / freq
        // q = 1 / (1 + r)
        // P = C * (1 - q^N) / r + M * q^N
        //
        // dP/dy = (1/freq) * dP/dr
        //
        // This replaces the O(N_periods) coupon loop inside every Newton
        // iteration with O(1) arithmetic per iteration.
        inline void bond_price_and_derivative(
            double par,
            double coupon_rate,
            double yield,
            double inv_freq,
            std::size_t periods,
            double& price,
            double& derivative) noexcept
        {
            const double r = yield * inv_freq;
            const double base = 1.0 + r;

            if (!std::isfinite(base) || base <= 0.0) {
                price = std::numeric_limits<double>::quiet_NaN();
                derivative = std::numeric_limits<double>::quiet_NaN();
                return;
            }

            const double q = 1.0 / base;
            const double qn = ipow(q, periods);
            const double coupon = par * coupon_rate * inv_freq;

            if (std::abs(r) < YIELD_EPSILON) {
                // Limit of the annuity factor as r -> 0.
                price = coupon * static_cast<double>(periods) + par;

                // d/dr [sum(k=1..N) (1+r)^(-k)] at r=0
                // = -N(N+1)/2
                const double n = static_cast<double>(periods);
                const double annuity_derivative =
                    -0.5 * n * (n + 1.0);

                derivative =
                    (coupon * annuity_derivative -
                     par * n) * inv_freq;
                return;
            }

            const double one_minus_qn = 1.0 - qn;
            const double annuity = one_minus_qn / r;

            price = coupon * annuity + par * qn;

            // d/dr [(1 - q^N) / r]
            const double n = static_cast<double>(periods);
            const double qn_plus_one = qn * q;
            const double annuity_derivative =
                (n * qn_plus_one * r - one_minus_qn) / (r * r);

            const double qn_derivative =
                -n * qn_plus_one;

            derivative =
                (coupon * annuity_derivative +
                 par * qn_derivative) * inv_freq;
        }
    }

    // 1. Forward Price: F0 = S0 * e^(rT)
    void calculate_forward_price_batch(
        const double* __restrict__ spots,
        const double* __restrict__ strikes,
        const double* __restrict__ maturities,
        double rate,
        std::size_t N,
        double* __restrict__ out_forwards,
        double* __restrict__ out_net_values) noexcept
    {
        #pragma omp simd
        for (std::size_t i = 0; i < N; ++i) {
            const double discount_exponent = rate * maturities[i];
            const double exp_pos = std::exp(discount_exponent);
            const double exp_neg = 1.0 / exp_pos;

            out_forwards[i] = spots[i] * exp_pos;
            out_net_values[i] =
                spots[i] - strikes[i] * exp_neg;
        }
    }

    // 2. Zero Coupon Pricing: P = M * e^(-rT)
    void price_zero_coupon_batch(
        const double* __restrict__ pars,
        const double* __restrict__ maturities,
        const double* __restrict__ rates,
        std::size_t N,
        double* __restrict__ out_prices) noexcept
    {
        #pragma omp simd
        for (std::size_t i = 0; i < N; ++i) {
            out_prices[i] =
                pars[i] * std::exp(-rates[i] * maturities[i]);
        }
    }

    // 3. Closed-form fixed-rate bond pricing.
    // O(N) instead of O(N * uniform_periods).
    void price_fixed_bond_simd(
        const HomogeneousBondBatch& bonds,
        const double* __restrict__ yields,
        std::size_t N,
        std::size_t uniform_periods,
        double freq,
        double* __restrict__ out_prices) noexcept
    {
        const double inv_freq = 1.0 / freq;
        const double periods = static_cast<double>(uniform_periods);

        #pragma omp simd
        for (std::size_t i = 0; i < N; ++i) {
            const double par = bonds.pars[i];
            const double coupon =
                par * bonds.coupon_rates[i] * inv_freq;
            const double r = yields[i] * inv_freq;

            if (std::abs(r) < YIELD_EPSILON) {
                out_prices[i] =
                    coupon * periods + par;
            } else {
                const double discount =
                    ipow(1.0 / (1.0 + r), uniform_periods);
                const double annuity =
                    (1.0 - discount) / r;

                out_prices[i] =
                    coupon * annuity + par * discount;
            }
        }
    }

    // 4. O(1)-per-Newton-iteration SIMD YTM solver.
    //
    // Previous implementation recomputed every coupon period for every
    // Newton iteration:
    //
    //     O(N * max_iter * uniform_periods)
    //
    // This version uses the closed-form bond price and analytical derivative:
    //
    //     O(N * max_iter)
    //
    // No break statements are used in the SIMD loop. Each lane is frozen
    // after convergence or failure.
    StatusCode solve_ytm_simd(
        const HomogeneousBondBatch& bonds,
        std::size_t N,
        std::size_t uniform_periods,
        double freq,
        double epsilon,
        std::size_t max_iter,
        double* __restrict__ out_yields) noexcept
    {
        const double inv_freq = 1.0 / freq;

        #pragma omp simd
        for (std::size_t i = 0; i < N; ++i) {
            const double par = bonds.pars[i];
            const double coupon_rate =
                bonds.coupon_rates[i];
            const double market_price =
                bonds.market_prices[i];

            double y = INITIAL_YIELD;
            bool active = true;
            bool converged = false;

            for (std::size_t iter = 0;
                 iter < max_iter;
                 ++iter) {

                double price;
                double derivative;

                bond_price_and_derivative(
                    par,
                    coupon_rate,
                    y,
                    inv_freq,
                    uniform_periods,
                    price,
                    derivative);

                const double error =
                    price - market_price;

                const bool price_ok =
                    std::isfinite(price) &&
                    std::isfinite(error);

                const bool just_converged =
                    active &&
                    price_ok &&
                    std::abs(error) <= epsilon;

                const bool derivative_ok =
                    active &&
                    price_ok &&
                    std::isfinite(derivative) &&
                    std::abs(derivative) >
                        DERIVATIVE_EPSILON;

                const double next_y =
                    derivative_ok
                        ? y - error / derivative
                        : y;

                const bool next_ok =
                    derivative_ok &&
                    std::isfinite(next_y) &&
                    next_y > MIN_YIELD;

                converged =
                    converged || just_converged;

                const bool keep_running =
                    active &&
                    !just_converged &&
                    next_ok;

                y = keep_running ? next_y : y;
                active = keep_running;
            }

            out_yields[i] =
                converged
                    ? y
                    : std::numeric_limits<double>::quiet_NaN();
        }

        return StatusCode::Success;
    }

    // 5. SIMD Risk Metric Engine with chunking.
    StatusCode calculate_bond_risk_simd(
        const HomogeneousBondBatch& bonds,
        const double* __restrict__ yields,
        std::size_t N,
        std::size_t uniform_periods,
        double freq,
        double* __restrict__ out_mac_durations,
        double* __restrict__ out_mod_durations,
        double* __restrict__ out_convexities) noexcept
    {
        const double inv_freq = 1.0 / freq;
        const double final_t =
            static_cast<double>(uniform_periods) * inv_freq;

        for (std::size_t chunk_offset = 0;
             chunk_offset < N;
             chunk_offset += CHUNK_SIZE) {

            const std::size_t current_chunk_size =
                std::min(CHUNK_SIZE, N - chunk_offset);

            alignas(64) double price[CHUNK_SIZE] = {};
            alignas(64) double weighted_pv[CHUNK_SIZE] = {};
            alignas(64) double convexity_pv[CHUNK_SIZE] = {};
            alignas(64) double current_df[CHUNK_SIZE];
            alignas(64) double inv_base[CHUNK_SIZE];
            alignas(64) double coupons[CHUNK_SIZE];

            #pragma omp simd
            for (std::size_t i = 0;
                 i < current_chunk_size;
                 ++i) {

                const std::size_t idx =
                    chunk_offset + i;

                const double y = yields[idx];
                const double r = y * inv_freq;

                inv_base[i] =
                    1.0 / (1.0 + r);
                current_df[i] =
                    inv_base[i];

                coupons[i] =
                    bonds.pars[idx] *
                    bonds.coupon_rates[idx] *
                    inv_freq;
            }

            for (std::size_t j = 1;
                 j <= uniform_periods;
                 ++j) {

                const double t =
                    static_cast<double>(j) * inv_freq;
                const double convexity_weight =
                    t * t + t * inv_freq;

                #pragma omp simd
                for (std::size_t i = 0;
                     i < current_chunk_size;
                     ++i) {

                    const double pv =
                        coupons[i] * current_df[i];

                    price[i] += pv;
                    weighted_pv[i] += t * pv;
                    convexity_pv[i] +=
                        convexity_weight * pv;

                    current_df[i] *= inv_base[i];
                }
            }

            const double final_convexity_weight =
                final_t * final_t +
                final_t * inv_freq;

            #pragma omp simd
            for (std::size_t i = 0;
                 i < current_chunk_size;
                 ++i) {

                const std::size_t idx =
                    chunk_offset + i;

                const double r =
                    yields[idx] * inv_freq;

                // current_df = q^(N+1), so multiplying by (1+r)
                // produces q^N.
                const double final_pv =
                    bonds.pars[idx] *
                    current_df[i] *
                    (1.0 + r);

                price[i] += final_pv;
                weighted_pv[i] +=
                    final_t * final_pv;
                convexity_pv[i] +=
                    final_convexity_weight * final_pv;

                const double denominator =
                    price[i] * (1.0 + r);

                if (price[i] > 1e-9 &&
                    (1.0 + r) > 1e-9 &&
                    std::isfinite(denominator)) {

                    const double mac_duration =
                        weighted_pv[i] / price[i];

                    out_mac_durations[idx] =
                        mac_duration;

                    out_mod_durations[idx] =
                        mac_duration / (1.0 + r);

                    out_convexities[idx] =
                        convexity_pv[i] /
                        (denominator * (1.0 + r));
                } else {
                    out_mac_durations[idx] =
                        std::numeric_limits<double>::quiet_NaN();
                    out_mod_durations[idx] =
                        std::numeric_limits<double>::quiet_NaN();
                    out_convexities[idx] =
                        std::numeric_limits<double>::quiet_NaN();
                }
            }
        }

        return StatusCode::Success;
    }

    // 6. Floating Rate Note pricing.
    //
    // forward_curves uses PERIOD-MAJOR layout:
    //
    //     forward_curves[period * N + asset_index]
    //
    // This provides contiguous access across assets in the SIMD loop.
    void price_frn_batch(
        const double* __restrict__ pars,
        const double* __restrict__ spreads,
        const double* __restrict__ maturities,
        const double* __restrict__ frequencies,
        const double* __restrict__ forward_curves,
        std::size_t max_periods,
        std::size_t N,
        double* __restrict__ out_prices) noexcept
    {
        for (std::size_t chunk_offset = 0;
             chunk_offset < N;
             chunk_offset += CHUNK_SIZE) {

            const std::size_t current_chunk_size =
                std::min(CHUNK_SIZE, N - chunk_offset);

            alignas(64) double price[CHUNK_SIZE] = {};
            alignas(64) double discount_accumulator[CHUNK_SIZE];
            alignas(64) std::size_t target_periods[CHUNK_SIZE];
            alignas(64) double inv_frequencies[CHUNK_SIZE];
            alignas(64) double spread_values[CHUNK_SIZE];
            alignas(64) double par_values[CHUNK_SIZE];

            #pragma omp simd
            for (std::size_t i = 0;
                 i < current_chunk_size;
                 ++i) {

                const std::size_t idx =
                    chunk_offset + i;

                discount_accumulator[i] = 1.0;

                target_periods[i] =
                    static_cast<std::size_t>(
                        maturities[idx] *
                        frequencies[idx]);

                inv_frequencies[i] =
                    1.0 / frequencies[idx];

                spread_values[i] =
                    spreads[idx];

                par_values[i] =
                    pars[idx];
            }

            for (std::size_t j = 0;
                 j < max_periods;
                 ++j) {

                const double* const period_forward_rates =
                    forward_curves + j * N;

                #pragma omp simd
                for (std::size_t i = 0;
                     i < current_chunk_size;
                     ++i) {

                    const std::size_t idx =
                        chunk_offset + i;

                    const bool active =
                        j < target_periods[i];

                    if (active) {
                        const double projected_rate =
                            period_forward_rates[idx];

                        const double coupon =
                            par_values[i] *
                            (projected_rate +
                             spread_values[i]) *
                            inv_frequencies[i];

                        discount_accumulator[i] *=
                            1.0 +
                            projected_rate *
                            inv_frequencies[i];

                        price[i] +=
                            coupon /
                            discount_accumulator[i];
                    }
                }
            }

            #pragma omp simd
            for (std::size_t i = 0;
                 i < current_chunk_size;
                 ++i) {

                const std::size_t idx =
                    chunk_offset + i;

                price[i] +=
                    par_values[i] /
                    discount_accumulator[i];

                out_prices[idx] =
                    price[i];
            }
        }
    }

    // 7. Fair Swap Rate: PV(Float) / Annuity
    void calculate_fair_swap_rate_batch(
        const double* __restrict__ notionals,
        const double* __restrict__ maturities,
        const double* __restrict__ floating_leg_pvs,
        const double* __restrict__ discount_factors,
        std::size_t N,
        double* __restrict__ out_swap_rates) noexcept
    {
        #pragma omp simd
        for (std::size_t i = 0; i < N; ++i) {
            const double annuity =
                maturities[i] *
                discount_factors[i] *
                notionals[i];

            out_swap_rates[i] =
                (annuity > 0.0)
                    ? floating_leg_pvs[i] / annuity
                    : std::numeric_limits<double>::quiet_NaN();
        }
    }

}
}
}