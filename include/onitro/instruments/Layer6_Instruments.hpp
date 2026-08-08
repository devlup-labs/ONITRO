#ifndef ONITRO_LAYER6_INSTRUMENTS_HPP
#define ONITRO_LAYER6_INSTRUMENTS_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>

namespace ONITRO {
namespace Layer6 {

// ============================================================================
// System Status Codes
// ============================================================================
enum class StatusCode : uint8_t {
    Success = 0,
    NullPointer,
    InvalidWindowSize
};

// ============================================================================
// 1. Welford State Blueprint (Cache Line Aligned - 64 Bytes)
// ============================================================================
struct alignas(64) WelfordState {
    double mean  = 0.0;
    double m2    = 0.0;
    double count = 0.0;
    
    double _pad[5]; 
};

// ============================================================================
// 2. Rolling Window State (Inverse Welford) - 64 Bytes
// ============================================================================
struct alignas(64) RollingWindowState {
    double mean  = 0.0;
    double m2    = 0.0;
    double count = 0.0;
    
    double _pad[5];
};

// ============================================================================
// 3. EWMA State Blueprint (Cache Line Aligned - 64 Bytes)
// ============================================================================
struct alignas(64) EwmaState {
    double current_ewma = 0.0;
    double alpha        = 0.1;
    
    double _pad[6];
};

// ============================================================================
// 4. Time-Decay EWMA State Blueprint - 64 Bytes
// ============================================================================
struct alignas(64) TimeDecayEwmaState {
    double   current_ewma      = 0.0; 
    double   tau_ns            = 1e9; // Time constant in nanoseconds
    uint64_t last_timestamp_ns = 0;   
    
    double   _pad[5]; 
};

// ============================================================================
// 5. Bivariate State Blueprint (Cache Line Aligned - 64 Bytes)
// ============================================================================
struct alignas(64) BivariateState {
    double mean_x = 0.0;
    double mean_y = 0.0;
    double m2_x   = 0.0;
    double m2_y   = 0.0;
    double c_xy   = 0.0; 
    double count  = 0.0;
    
    double _pad[2]; 
};

// ============================================================================
// 6. Exponential Bivariate State (EWMCOV / EWMCORR) - 64 Bytes
// ============================================================================
struct alignas(64) EwmaBivariateState {
    double mean_x = 0.0;
    double mean_y = 0.0;
    double var_x  = 0.0;
    double var_y  = 0.0;
    double cov_xy = 0.0;
    double alpha  = 0.1; 
    
    double _pad[2];      
};

// ============================================================================
// 7. Higher-Order Moments State (Cache Line Aligned - 64 Bytes)
// ============================================================================
struct alignas(64) HigherMomentsState {
    long double mean  = 0.0;
    long double m2    = 0.0;   
    long double m3    = 0.0;   
    long double m4    = 0.0;   
    long double count = 0.0;
    
    long double _pad[3];       
};

// ============================================================================
// 8. 1D Kalman Filter State (Cache Line Aligned - 64 Bytes)
// ============================================================================
struct alignas(64) KalmanState1D {
    double x_est = 0.0; // Post-estimate state
    double p_est = 1.0; // Post-estimate error covariance
    double q     = 1e-4; // Process noise variance (tunable)
    double r     = 1e-2; // Measurement noise variance (tunable)
    
    double _pad[4];      
};

// ============================================================================
// Function Declarations (Public API)
// ============================================================================

WelfordState process_welford_batch_simd(
    const double* __restrict__ in_values, 
    std::size_t batch_size) noexcept;

StatusCode process_rolling_window_welford(
    RollingWindowState* __restrict__ state,
    const double* __restrict__ in_values,
    double* __restrict__ out_variance,
    std::size_t batch_size,
    std::size_t window_size) noexcept;

StatusCode process_ewma_batch_simd(
    EwmaState* __restrict__ state,
    const double* __restrict__ in_values,
    double* __restrict__ out_ewma,
    std::size_t batch_size) noexcept;

StatusCode process_time_decay_ewma(
    TimeDecayEwmaState* __restrict__ state,
    const double* __restrict__ in_values,
    const uint64_t* __restrict__ in_timestamps,
    double* __restrict__ out_ewma,
    std::size_t batch_size) noexcept;

BivariateState process_bivariate_batch_simd(
    const double* __restrict__ in_x,
    const double* __restrict__ in_y,
    std::size_t batch_size) noexcept;

StatusCode process_ewmcov_batch_simd(
    EwmaBivariateState* __restrict__ state,
    const double* __restrict__ in_x,
    const double* __restrict__ in_y,
    double* __restrict__ out_cov,
    double* __restrict__ out_corr,
    std::size_t batch_size) noexcept;

HigherMomentsState process_higher_moments_batch_simd(
    const double* __restrict__ in_values, 
    std::size_t batch_size) noexcept;

StatusCode process_parkinson_volatility_batch_simd(
    const double* __restrict__ in_highs,
    const double* __restrict__ in_lows,
    double* __restrict__ out_volatility,
    std::size_t batch_size) noexcept;

StatusCode process_zscore_batch_simd(
    const double* __restrict__ in_values,
    const double* __restrict__ in_means,
    const double* __restrict__ in_variances,
    double* __restrict__ out_zscores,
    std::size_t batch_size) noexcept;

StatusCode process_kalman_1d_batch(
    KalmanState1D* __restrict__ state,
    const double* __restrict__ in_measurements,
    double* __restrict__ out_estimates,
    std::size_t batch_size) noexcept;

// ============================================================================
// Extraction Helpers (Fast Math without std::pow)
// ============================================================================
inline double extract_skewness(const HigherMomentsState& state) noexcept {
    if (state.count < 3.0 || state.m2 <= 1e-12) return 0.0;
    const double variance = state.m2 / state.count;
    return (state.m3 / state.count) / (variance * std::sqrt(variance));
}

inline double extract_kurtosis(const HigherMomentsState& state) noexcept {
    if (state.count < 4.0 || state.m2 <= 1e-12) return 0.0;
    const double variance = state.m2 / state.count;
    return ((state.m4 / state.count) / (variance * variance)) - 3.0; 
}

} // namespace Layer6
} // namespace ONITRO

#endif // ONITRO_LAYER6_INSTRUMENTS_HPP