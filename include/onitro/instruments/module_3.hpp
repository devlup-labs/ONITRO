#pragma once




#include <cstddef>
#include <cstdint>

namespace ONITRO {
namespace Layer3 {
namespace FixedIncome {


    // Common status/result code returned by primitives that can encounter
    // non-convergence or degenerate input.
    enum class StatusCode : std::uint8_t {
        Success = 0,
        Failure = 1
    };

    // Structure-of-arrays view over a homogeneous batch of fixed-coupon bonds.
    //
    // All arrays are non-owning and must contain at least N elements.
    struct HomogeneousBondBatch {

    // Layer 4: Aligned Data Definition
    // Enforces strict 64-byte cache-line alignment to optimize SIMD vector register loads
    struct alignas(64) HomogeneousBondBatch {

        const double* __restrict__ pars;
        const double* __restrict__ coupon_rates;
        const double* __restrict__ market_prices;
    };


    // 1. Forward Price:
    //
    // F0    = S0 * exp(rT)
    // V_net = S0 - K * exp(-rT)
    void calculate_forward_price_batch(
        const double* __restrict__ spots,
        const double* __restrict__ strikes,
        const double* __restrict__ maturities,
        double rate,
        std::size_t N,
        double* __restrict__ out_forwards,
        double* __restrict__ out_net_values) noexcept;

    // 2. Zero Coupon Pricing:
    //
    // P = M * exp(-rT)
    void price_zero_coupon_batch(
        const double* __restrict__ pars,
        const double* __restrict__ maturities,
        const double* __restrict__ rates,
        std::size_t N,
        double* __restrict__ out_prices) noexcept;

    // 3. Fixed-coupon bond pricing:
    //
    // P = C * (1 - q^N) / r + M * q^N
    //
    // where:
    //     r = y / freq
    //     q = 1 / (1 + r)
    //
    // Uses closed-form pricing with O(1) work per bond.
    void price_fixed_bond_simd(
        const HomogeneousBondBatch& bonds,
        const double* __restrict__ yields,
        std::size_t N,
        std::size_t uniform_periods,
        double freq,
        double* __restrict__ out_prices) noexcept;

    // 4. Newton-Raphson yield-to-maturity solver.
    //
    // Uses closed-form bond pricing and analytical derivative.
    // Complexity: O(N * max_iter), independent of uniform_periods
    // for each Newton iteration.
    //
    // SIMD is applied across independent bonds.
    StatusCode solve_ytm_simd(
        const HomogeneousBondBatch& bonds,
        std::size_t N,
        std::size_t uniform_periods,
        double freq,
        double epsilon,
        std::size_t max_iter,
        double* __restrict__ out_yields) noexcept;

    // 5. Bond risk metrics:
    //
    // - Macaulay duration
    // - Modified duration
    // - Convexity
    //
    // SIMD processing is performed in cache-sized chunks.
    StatusCode calculate_bond_risk_simd(
        const HomogeneousBondBatch& bonds,
        const double* __restrict__ yields,
        std::size_t N,
        std::size_t uniform_periods,
        double freq,
        double* __restrict__ out_mac_durations,
        double* __restrict__ out_mod_durations,
        double* __restrict__ out_convexities) noexcept;

    // 6. Floating Rate Note (FRN) pricing.
    //
    // forward_curves uses PERIOD-MAJOR layout:
    //
    //     forward_curves[j * N + i]
    //
    // where:
    //     j = period index
    //     i = instrument index
    //
    // This layout provides contiguous access across instruments inside
    // the SIMD loop.
    void price_frn_batch(
        const double* __restrict__ pars,
        const double* __restrict__ spreads,
        const double* __restrict__ maturities,
        const double* __restrict__ frequencies,
        const double* __restrict__ forward_curves,
        std::size_t max_periods,
        std::size_t N,
        double* __restrict__ out_prices) noexcept;

    // 7. Fair Swap Rate:
    //
    // R_swap = PV(Floating Leg) / Annuity
    void calculate_fair_swap_rate_batch(
        const double* __restrict__ notionals,
        const double* __restrict__ maturities,
        const double* __restrict__ floating_leg_pvs,
        const double* __restrict__ discount_factors,
        std::size_t N,
        double* __restrict__ out_swap_rates) noexcept;

}
}
}

    // No-exception execution status tracking codes
    enum class StatusCode : int32_t {
        Success = 0,
        InvalidBatchSize = -1,
        MathDomainError = -2,
        ConvergenceFailure = -3
    };

    // 1. Forward & Network Value Primitive
    void calculate_forward_price_batch(const double* __restrict__ spots,
                                       const double* __restrict__ strikes,
                                       const double* __restrict__ maturities,
                                       double rate, std::size_t N,
                                       double* __restrict__ out_forwards,
                                       double* __restrict__ out_net_values) noexcept;

    // 2. Zero-Coupon Bond Pricing Engine
    void price_zero_coupon_batch(const double* __restrict__ pars,
                                 const double* __restrict__ maturities,
                                 const double* __restrict__ rates,
                                 std::size_t N,
                                 double* __restrict__ out_prices) noexcept;

    // 3. Fixed-Coupon Bond Pricing Engine - Fully Vectorized Across Instruments
    void price_fixed_bond_simd(const HomogeneousBondBatch& bonds,
                               const double* __restrict__ yields,
                               std::size_t N,
                               std::size_t uniform_periods, 
                               double freq,                 
                               double* __restrict__ out_prices) noexcept;

    // 4. Yield-to-Maturity (YTM) Solver - Internal Chunking Protects Against Truncation Faults
    StatusCode solve_ytm_simd(const HomogeneousBondBatch& bonds,
                              std::size_t N,
                              std::size_t uniform_periods,
                              double freq,
                              double epsilon,
                              std::size_t max_iter,
                              double* __restrict__ out_yields) noexcept;

    // 5. High-Speed Risk Sensitivities (Macaulay, Modified Duration, and Convexity)
    StatusCode calculate_bond_risk_simd(const HomogeneousBondBatch& bonds,
                                         const double* __restrict__ yields,
                                         std::size_t N,
                                         std::size_t uniform_periods,
                                         double freq,
                                         double* __restrict__ out_mac_durations,
                                         double* __restrict__ out_mod_durations,
                                         double* __restrict__ out_convexities) noexcept;

    // 6. Floating Rate Notes (FRN) Pricing Engine - Fixed Contiguous Matrix Stride
    void price_frn_batch(const double* __restrict__ pars,
                         const double* __restrict__ spreads,
                         const double* __restrict__ maturities,
                         const double* __restrict__ frequencies,
                         const double* __restrict__ forward_curves, 
                         std::size_t max_periods,
                         std::size_t N,
                         double* __restrict__ out_prices) noexcept;

    // 7. Fair Swap Rate Valuation Engine
    void calculate_fair_swap_rate_batch(const double* __restrict__ notionals,
                                        const double* __restrict__ maturities,
                                        const double* __restrict__ floating_leg_pvs,
                                        const double* __restrict__ discount_factors,
                                        std::size_t N,
                                        double* __restrict__ out_swap_rates) noexcept;
}
}
}

