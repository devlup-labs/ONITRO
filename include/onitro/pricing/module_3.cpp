#include "module_3.hpp"
#include <cmath>
#include <algorithm>

namespace ONITRO {
namespace Layer3 {
namespace FixedIncome {

    // 1. Forward Price: F0 = S0 * e^(rT)
    void calculate_forward_price_batch(const double* __restrict__ spots,
                                       const double* __restrict__ strikes,
                                       const double* __restrict__ maturities,
                                       double rate, std::size_t N,
                                       double* __restrict__ out_forwards,
                                       double* __restrict__ out_net_values) noexcept 
    {
        #pragma GCC ivdep
        #pragma omp simd
        for (std::size_t i = 0; i < N; ++i) {
            const double T = maturities[i];
            const double exp_pos = std::exp(rate * T);
            const double exp_neg = 1.0 / exp_pos;
            
            out_forwards[i]   = spots[i] * exp_pos;
            out_net_values[i] = spots[i] - (strikes[i] * exp_neg);
        }
    }

    // 2. Zero Coupon Pricing: P = M * e^(-rT)
    void price_zero_coupon_batch(const double* __restrict__ pars,
                                 const double* __restrict__ maturities,
                                 const double* __restrict__ rates,
                                 std::size_t N,
                                 double* __restrict__ out_prices) noexcept 
    {
        #pragma GCC ivdep
        #pragma omp simd
        for (std::size_t i = 0; i < N; ++i) {
            out_prices[i] = pars[i] * std::exp(-rates[i] * maturities[i]);
        }
    }

    // 3. Fully Optimized Fixed Bond Pricing Loop (std::pow replaced by incremental vector multiplication)
    void price_fixed_bond_simd(const HomogeneousBondBatch& bonds,
                               const double* __restrict__ yields,
                               std::size_t N,
                               std::size_t uniform_periods,
                               double freq,
                               double* __restrict__ out_prices) noexcept 
    {
        constexpr std::size_t CHUNK_SIZE = 1024;
        
        for (std::size_t chunk_offset = 0; chunk_offset < N; chunk_offset += CHUNK_SIZE) {
            const std::size_t current_chunk_size = std::min(CHUNK_SIZE, N - chunk_offset);
            
            alignas(64) double current_df[CHUNK_SIZE];
            alignas(64) double inv_base[CHUNK_SIZE];

            #pragma GCC ivdep
            #pragma omp simd
            for (std::size_t i = 0; i < current_chunk_size; ++i) {
                const std::size_t global_idx = chunk_offset + i;
                out_prices[global_idx] = 0.0;
                const double rate_per_period = yields[global_idx] / freq;
                inv_base[i] = 1.0 / (1.0 + rate_per_period);
                current_df[i] = inv_base[i];
            }

            // Loop Inversion: Time periods outside, pure SIMD vector multiplication inside
            for (std::size_t j = 1; j <= uniform_periods; ++j) {
                #pragma GCC ivdep
                #pragma omp simd
                for (std::size_t i = 0; i < current_chunk_size; ++i) {
                    const std::size_t global_idx = chunk_offset + i;
                    const double coupon = (bonds.pars[global_idx] * bonds.coupon_rates[global_idx]) / freq;
                    out_prices[global_idx] += coupon * current_df[i];
                    current_df[i] *= inv_base[i];
                }
            }

            #pragma GCC ivdep
            #pragma omp simd
            for (std::size_t i = 0; i < current_chunk_size; ++i) {
                const std::size_t global_idx = chunk_offset + i;
                const double rate_per_period = yields[global_idx] / freq;
                // Re-adjust trailing product factor to exactly match uniform_periods power
                const double final_df = current_df[i] * (1.0 + rate_per_period);
                out_prices[global_idx] += bonds.pars[global_idx] * final_df;
            }
        }
    }

    // 4. Predicated SIMD Vectorized YTM Solver with Early-Termination Subroutines
    StatusCode solve_ytm_simd(const HomogeneousBondBatch& bonds,
                              std::size_t N,
                              std::size_t uniform_periods,
                              double freq,
                              double epsilon,
                              std::size_t max_iter,
                              double* __restrict__ out_yields) noexcept 
    {
        constexpr std::size_t CHUNK_SIZE = 1024;

        for (std::size_t chunk_offset = 0; chunk_offset < N; chunk_offset += CHUNK_SIZE) {
            const std::size_t current_chunk_size = std::min(CHUNK_SIZE, N - chunk_offset);

            alignas(64) double y_n[CHUNK_SIZE]; 
            alignas(64) uint8_t active_mask[CHUNK_SIZE]; 

            #pragma GCC ivdep
            #pragma omp simd
            for (std::size_t i = 0; i < current_chunk_size; ++i) {
                const std::size_t global_idx = chunk_offset + i;
                y_n[i] = 0.05;          
                active_mask[i] = 1;     
                out_yields[global_idx] = NAN;    
            }

            for (std::size_t iter = 0; iter < max_iter; ++iter) {
                alignas(64) double p_calc[CHUNK_SIZE] = {0.0};
                alignas(64) double p_plus[CHUNK_SIZE] = {0.0};
                
                alignas(64) double current_df[CHUNK_SIZE];
                alignas(64) double current_df_plus[CHUNK_SIZE];
                alignas(64) double inv_base[CHUNK_SIZE];
                alignas(64) double inv_base_plus[CHUNK_SIZE];

                #pragma GCC ivdep
                #pragma omp simd
                for (std::size_t i = 0; i < current_chunk_size; ++i) {
                    inv_base[i] = 1.0 / (1.0 + (y_n[i] / freq));
                    inv_base_plus[i] = 1.0 / (1.0 + ((y_n[i] + 0.0001) / freq));
                    current_df[i] = inv_base[i];
                    current_df_plus[i] = inv_base_plus[i];
                }

                for (std::size_t j = 1; j <= uniform_periods; ++j) {
                    #pragma GCC ivdep
                    #pragma omp simd
                    for (std::size_t i = 0; i < current_chunk_size; ++i) {
                        const std::size_t global_idx = chunk_offset + i;
                        const double mask = static_cast<double>(active_mask[i]);
                        const double coupon = (bonds.pars[global_idx] * bonds.coupon_rates[global_idx]) / freq;
                        
                        p_calc[i] += (coupon * current_df[i]) * mask;
                        p_plus[i] += (coupon * current_df_plus[i]) * mask;
                        
                        current_df[i] *= inv_base[i];
                        current_df_plus[i] *= inv_base_plus[i];
                    }
                }

                std::size_t active_count = 0;

                #pragma GCC ivdep
                #pragma omp simd reduction(+:active_count)
                for (std::size_t i = 0; i < current_chunk_size; ++i) {
                    if (active_mask[i]) {
                        const std::size_t global_idx = chunk_offset + i;
                        const double final_df = current_df[i] * (1.0 + (y_n[i] / freq));
                        const double final_df_plus = current_df_plus[i] * (1.0 + ((y_n[i] + 0.0001) / freq));

                        p_calc[i] += bonds.pars[global_idx] * final_df;
                        p_plus[i] += bonds.pars[global_idx] * final_df_plus;

                        const double f_val = p_calc[i] - bonds.market_prices[global_idx];
                        
                        if (std::abs(f_val) < epsilon) {
                            out_yields[global_idx] = y_n[i];
                            active_mask[i] = 0; 
                        } else {
                            const double f_prime = (p_plus[i] - p_calc[i]) / 0.0001;
                            if (std::abs(f_prime) > 1e-8) {
                                y_n[i] -= f_val / f_prime; 
                                active_count += 1;
                            } else {
                                active_mask[i] = 0; 
                            }
                        }
                    }
                }

                // Hot branch breakout if entire batch vector resolves early
                if (active_count == 0) {
                    break;
                }
            }
        }
        return StatusCode::Success;
    }

    // 5. Fully Vectorized Risk Metric Engine with Chunking Safeguards
    StatusCode calculate_bond_risk_simd(const HomogeneousBondBatch& bonds,
                                         const double* __restrict__ yields,
                                         std::size_t N,
                                         std::size_t uniform_periods,
                                         double freq,
                                         double* __restrict__ out_mac_durations,
                                         double* __restrict__ out_mod_durations,
                                         double* __restrict__ out_convexities) noexcept 
    {
        constexpr std::size_t CHUNK_SIZE = 1024;

        for (std::size_t chunk_offset = 0; chunk_offset < N; chunk_offset += CHUNK_SIZE) {
            const std::size_t current_chunk_size = std::min(CHUNK_SIZE, N - chunk_offset);

            alignas(64) double price[CHUNK_SIZE] = {0.0};
            alignas(64) double weighted_pv[CHUNK_SIZE] = {0.0};
            alignas(64) double convexity_pv[CHUNK_SIZE] = {0.0};
            
            alignas(64) double current_df[CHUNK_SIZE];
            alignas(64) double inv_base[CHUNK_SIZE];

            #pragma GCC ivdep
            #pragma omp simd
            for (std::size_t i = 0; i < current_chunk_size; ++i) {
                const std::size_t global_idx = chunk_offset + i;
                const double r = yields[global_idx] / freq;
                inv_base[i] = 1.0 / (1.0 + r);
                current_df[i] = inv_base[i];
            }

            for (std::size_t j = 1; j <= uniform_periods; ++j) {
                const double t = static_cast<double>(j) / freq;

                #pragma GCC ivdep
                #pragma omp simd
                for (std::size_t i = 0; i < current_chunk_size; ++i) {
                    const std::size_t global_idx = chunk_offset + i;
                    const double coupon = (bonds.pars[global_idx] * bonds.coupon_rates[global_idx]) / freq;
                    const double pv = coupon * current_df[i];
                    
                    price[i]        += pv;
                    weighted_pv[i]  += t * pv;
                    convexity_pv[i] += (t * t + t / freq) * pv;

                    current_df[i] *= inv_base[i];
                }
            }

            const double final_t = static_cast<double>(uniform_periods) / freq;

            #pragma GCC ivdep
            #pragma omp simd
            for (std::size_t i = 0; i < current_chunk_size; ++i) {
                const std::size_t global_idx = chunk_offset + i;
                const double r = yields[global_idx] / freq;
                const double final_pv = bonds.pars[global_idx] * current_df[i] * (1.0 + r);
                
                price[i]        += final_pv;
                weighted_pv[i]  += final_t * final_pv;
                convexity_pv[i] += (final_t * final_t + final_t / freq) * final_pv;

                const bool is_denominator_valid = price[i] > 1e-9 && (1.0 + r) > 1e-9;

                if (__builtin_expect(is_denominator_valid, 1)) {
                    out_mac_durations[global_idx] = weighted_pv[i] / price[i];
                    out_mod_durations[global_idx] = out_mac_durations[global_idx] / (1.0 + r);
                    out_convexities[global_idx]   = (1.0 / (price[i] * (1.0 + r) * (1.0 + r))) * convexity_pv[i];
                } else {
                    out_mac_durations[global_idx] = NAN;
                    out_mod_durations[global_idx] = NAN;
                    out_convexities[global_idx]   = NAN;
                }
            }
        }
        return StatusCode::Success;
    }

    // 6. Floating Rate Note (FRN) Pricing Model - Realigned for Strict Outward Vectorization
    void price_frn_batch(const double* __restrict__ pars,
                         const double* __restrict__ spreads,
                         const double* __restrict__ maturities,
                         const double* __restrict__ frequencies,
                         const double* __restrict__ forward_curves,
                         std::size_t max_periods,
                         std::size_t N,
                         double* __restrict__ out_prices) noexcept 
    {
        constexpr std::size_t CHUNK_SIZE = 1024;
        
        for (std::size_t chunk_offset = 0; chunk_offset < N; chunk_offset += CHUNK_SIZE) {
            const std::size_t current_chunk_size = std::min(CHUNK_SIZE, N - chunk_offset);

            alignas(64) double price[CHUNK_SIZE] = {0.0};
            alignas(64) double discount_accumulator[CHUNK_SIZE];
            alignas(64) std::size_t target_periods[CHUNK_SIZE];

            #pragma GCC ivdep
            #pragma omp simd
            for (std::size_t i = 0; i < current_chunk_size; ++i) {
                const std::size_t global_idx = chunk_offset + i;
                discount_accumulator[i] = 1.0;
                target_periods[i] = static_cast<std::size_t>(maturities[global_idx] * frequencies[global_idx]);
            }

            // Inverted execution logic: time steps outermost, vectorized assets innermost via register masks
            for (std::size_t j = 0; j < max_periods; ++j) {
                #pragma GCC ivdep
                #pragma omp simd
                for (std::size_t i = 0; i < current_chunk_size; ++i) {
                    if (j < target_periods[i]) {
                        const std::size_t global_idx = chunk_offset + i;
                        const double freq = frequencies[global_idx];
                        const double projected_rate = forward_curves[global_idx * max_periods + j];
                        const double coupon = pars[global_idx] * (projected_rate + spreads[global_idx]) / freq;
                        
                        discount_accumulator[i] *= (1.0 + projected_rate / freq);
                        price[i] += coupon / discount_accumulator[i];
                    }
                }
            }

            #pragma GCC ivdep
            #pragma omp simd
            for (std::size_t i = 0; i < current_chunk_size; ++i) {
                const std::size_t global_idx = chunk_offset + i;
                price[i] += pars[global_idx] / discount_accumulator[i];
                out_prices[global_idx] = price[i];
            }
        }
    }

    // 7. Fair Swap Rate: PV(Float) / Annuity
    void calculate_fair_swap_rate_batch(const double* __restrict__ notionals,
                                        const double* __restrict__ maturities,
                                        const double* __restrict__ floating_leg_pvs,
                                        const double* __restrict__ discount_factors,
                                        std::size_t N,
                                        double* __restrict__ out_swap_rates) noexcept 
    {
        #pragma GCC ivdep
        #pragma omp simd
        for (std::size_t i = 0; i < N; ++i) {
            double annuity = maturities[i] * discount_factors[i] * notionals[i];
            out_swap_rates[i] = (annuity > 0.0) ? (floating_leg_pvs[i] / annuity) : NAN;
        }
    }
}
}
}