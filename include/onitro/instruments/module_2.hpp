#pragma once
#include <cstddef>
#include <cstdint>

namespace ONITRO {
namespace Layer3 {
namespace Volatility {

    constexpr uint8_t WANT_IV=1<<0;
    constexpr uint8_t WANT_DELTA=1<<1;
    constexpr uint8_t WANT_GAMMA=1<<2;
    constexpr uint8_t WANT_VEGA=1<<3;
    constexpr uint8_t WANT_THETA=1<<4;
    constexpr uint8_t WANT_RHO=1<<5;

     struct alignas(64) HomogenousOptionBatch{
        const double* __restrict__ target_prices;
        const double* __restrict__ spots;
        const double* __restrict__ strikes;
        const double* __restrict__ maturities;
        const double* __restrict__ div_yields;
        const double* __restrict__ rates; 
        const uint8_t*__restrict__ is_call;
    };

    void iv_chain_bsm_simd (const HomogenousOptionBatch& batch,
                            std::size_t N,
                            double epsilon,
                            std::size_t max_iter,
                            double* __restrict__ out_ivs) noexcept;

    void iv_and_greeks_chain_simd(const HomogenousOptionBatch& batch,
                                std::size_t N,
                                double epsilon,
                                std::size_t max_iter,
                                uint8_t output_mask,
                                double* __restrict__ out_ivs,
                                double* __restrict__ out_deltas,
                                double* __restrict__ out_gammas,       
                                double* __restrict__ out_vegas,
                                double* __restrict__ out_thetas,
                                double* __restrict__ out_rhos) noexcept;
}
}
}