#include "Layer6_Instruments.hpp"
#include <algorithm> 

namespace ONITRO {
namespace Layer6 {

namespace {
    constexpr std::size_t SIMD_WIDTH = 4; 

    // Chan's Algorithm (Welford)
    inline WelfordState merge_welford(const WelfordState& a, const WelfordState& b) noexcept {
        if (a.count == 0.0) return b;
        if (b.count == 0.0) return a;
        WelfordState merged;
        merged.count = a.count + b.count;
        const double delta = b.mean - a.mean;
        merged.mean  = a.mean + delta * (b.count / merged.count);
        merged.m2    = a.m2 + b.m2 + (delta * delta) * ((a.count * b.count) / merged.count);
        return merged;
    }

    // Chan's Algorithm (Bivariate)
    inline BivariateState merge_bivariate(const BivariateState& a, const BivariateState& b) noexcept {
        if (a.count == 0.0) return b;
        if (b.count == 0.0) return a;
        BivariateState merged;
        merged.count = a.count + b.count;
        const double dx = b.mean_x - a.mean_x;
        const double dy = b.mean_y - a.mean_y;
        const double weight = b.count / merged.count;
        const double cross_factor = (a.count * b.count) / merged.count;
        merged.mean_x = a.mean_x + dx * weight;
        merged.mean_y = a.mean_y + dy * weight;
        merged.m2_x = a.m2_x + b.m2_x + (dx * dx) * cross_factor;
        merged.m2_y = a.m2_y + b.m2_y + (dy * dy) * cross_factor;
        merged.c_xy = a.c_xy + b.c_xy + (dx * dy) * cross_factor;
        return merged;
    }

    // Pébay's Equations (Higher Moments)
    inline HigherMomentsState merge_higher_moments(const HigherMomentsState& a, const HigherMomentsState& b) noexcept {
        if (a.count == 0.0) return b;
        if (b.count == 0.0) return a;
        HigherMomentsState merged;
        merged.count = a.count + b.count;
        const double n_a = a.count, n_b = b.count, n = merged.count, n_sq = n * n;
        const double delta = b.mean - a.mean;
        const double delta_sq = delta * delta;
        const double delta_cub = delta_sq * delta;
        const double delta_quad = delta_sq * delta_sq;
        merged.mean = a.mean + delta * (n_b / n);
        merged.m2 = a.m2 + b.m2 + delta_sq * (n_a * n_b) / n;
        merged.m3 = a.m3 + b.m3 + delta_cub * (n_a * n_b * (n_a - n_b)) / n_sq + 3.0 * delta * (n_a * b.m2 - n_b * a.m2) / n;
        merged.m4 = a.m4 + b.m4 + delta_quad * (n_a * n_b * (n_a * n_a - n_a * n_b + n_b * n_b)) / (n_sq * n) 
                  + 6.0 * delta_sq * (n_a * n_a * b.m2 + n_b * n_b * a.m2) / n_sq + 4.0 * delta * (n_a * b.m3 - n_b * a.m3) / n;
        return merged;
    }
} // anonymous namespace

// ============================================================================
// 1. SIMD-Friendly Parallel Welford Batch Processor 
// ============================================================================
WelfordState process_welford_batch_simd(const double* __restrict__ in_values, std::size_t batch_size) noexcept {
    WelfordState lanes[SIMD_WIDTH] = {}; 
    std::size_t i = 0;
    const std::size_t simd_limit = batch_size - (batch_size % SIMD_WIDTH);

    for (; i < simd_limit; i += SIMD_WIDTH) {
        #pragma GCC unroll 4
        for (std::size_t lane = 0; lane < SIMD_WIDTH; ++lane) {
            const double x = in_values[i + lane];
            lanes[lane].count += 1.0;
            const double delta = x - lanes[lane].mean;
            lanes[lane].mean += delta / lanes[lane].count;
            lanes[lane].m2 += delta * (x - lanes[lane].mean);
        }
    }

    WelfordState final_state = lanes[0];
    for (std::size_t lane = 1; lane < SIMD_WIDTH; ++lane) {
        final_state = merge_welford(final_state, lanes[lane]);
    }

    for (; i < batch_size; ++i) {
        WelfordState tail_state;
        tail_state.count = 1.0;
        tail_state.mean = in_values[i];
        tail_state.m2 = 0.0;
        final_state = merge_welford(final_state, tail_state);
    }
    return final_state;
}

// ============================================================================
// 2. O(1) Fixed-Size Rolling Window Variance (Inverse Welford)
// ============================================================================
StatusCode process_rolling_window_welford(
    RollingWindowState* __restrict__ state,
    const double* __restrict__ in_values,
    double* __restrict__ out_variance,
    std::size_t batch_size,
    std::size_t window_size) noexcept
{
    if (!state || !in_values || !out_variance) return StatusCode::NullPointer;
    if (window_size < 2) return StatusCode::InvalidWindowSize;

    double mean = state->mean;
    double m2   = state->m2;
    double count = state->count;
    const double double_W = static_cast<double>(window_size);

    for (std::size_t i = 0; i < batch_size; ++i) {
        const double x_in = in_values[i];

        if (count < double_W) {
            count += 1.0;
            const double delta = x_in - mean;
            mean += delta / count;
            m2 += delta * (x_in - mean);
            out_variance[i] = (count > 1.0) ? (m2 / (count - 1.0)) : 0.0;
        } 
        else {
            const double x_out = in_values[i - window_size];
            const double delta_out = x_out - mean;
            const double mean_temp = mean - (delta_out / (double_W - 1.0));
            m2 -= delta_out * (x_out - mean_temp);
            
            const double delta_in = x_in - mean_temp;
            mean = mean_temp + (delta_in / double_W);
            m2 += delta_in * (x_in - mean);
            out_variance[i] = m2 / (double_W - 1.0);
        }
    }

    state->mean  = mean;
    state->m2    = m2;
    state->count = count;
    return StatusCode::Success;
}

// ============================================================================
// 3. SIMD-Unrolled EWMA Batch Processor
// ============================================================================
StatusCode process_ewma_batch_simd(
    EwmaState* __restrict__ state,
    const double* __restrict__ in_values,
    double* __restrict__ out_ewma,
    std::size_t batch_size) noexcept
{
    if (!state || !in_values || !out_ewma) return StatusCode::NullPointer;

    const double alpha = state->alpha, beta = 1.0 - alpha;
    const double beta2 = beta * beta, beta3 = beta2 * beta, beta4 = beta3 * beta;

    double current = state->current_ewma;
    std::size_t i = 0;
    const std::size_t simd_limit = batch_size - (batch_size % SIMD_WIDTH);

    for (; i < simd_limit; i += SIMD_WIDTH) {
        const double x0 = in_values[i], x1 = in_values[i+1], x2 = in_values[i+2], x3 = in_values[i+3];
        const double s0 = alpha * x0 + beta * current;
        const double s1 = alpha * x1 + alpha * beta * x0 + beta2 * current;
        const double s2 = alpha * x2 + alpha * beta * x1 + alpha * beta2 * x0 + beta3 * current;
        const double s3 = alpha * x3 + alpha * beta * x2 + alpha * beta2 * x1 + alpha * beta3 * x0 + beta4 * current;
        out_ewma[i] = s0; out_ewma[i+1] = s1; out_ewma[i+2] = s2; out_ewma[i+3] = s3;
        current = s3;
    }

    for (; i < batch_size; ++i) {
        current = alpha * in_values[i] + beta * current;
        out_ewma[i] = current;
    }
    state->current_ewma = current;
    return StatusCode::Success;
}

// ============================================================================
// 4. Time-Decay / Time-Weighted EWMA 
// ============================================================================
StatusCode process_time_decay_ewma(
    TimeDecayEwmaState* __restrict__ state,
    const double* __restrict__ in_values,
    const uint64_t* __restrict__ in_timestamps,
    double* __restrict__ out_ewma,
    std::size_t batch_size) noexcept
{
    if (!state || !in_values || !in_timestamps || !out_ewma) return StatusCode::NullPointer;

    const double inv_tau = 1.0 / state->tau_ns;
    double current = state->current_ewma;
    uint64_t last_ts = state->last_timestamp_ns;

    for (std::size_t i = 0; i < batch_size; ++i) {
        const double x = in_values[i];
        const uint64_t ts = in_timestamps[i];

        if (last_ts == 0) {
            current = x;
            last_ts = ts;
            out_ewma[i] = x;
            continue;
        }

        const double dt = static_cast<double>(ts - last_ts);
        const double alpha = 1.0 - std::exp(-dt * inv_tau);

        current = alpha * x + (1.0 - alpha) * current;
        last_ts = ts;
        out_ewma[i] = current;
    }

    state->current_ewma = current;
    state->last_timestamp_ns = last_ts;
    return StatusCode::Success;
}

// ============================================================================
// 5. SIMD-Friendly Bivariate Batch Processor (Tree Reduction)
// ============================================================================
BivariateState process_bivariate_batch_simd(const double* __restrict__ in_x, const double* __restrict__ in_y, std::size_t batch_size) noexcept {
    BivariateState lanes[SIMD_WIDTH] = {};
    std::size_t i = 0;
    const std::size_t simd_limit = batch_size - (batch_size % SIMD_WIDTH);

    for (; i < simd_limit; i += SIMD_WIDTH) {
        #pragma GCC unroll 4
        for (std::size_t lane = 0; lane < SIMD_WIDTH; ++lane) {
            const double x = in_x[i + lane], y = in_y[i + lane];
            lanes[lane].count += 1.0;
            const double inv_count = 1.0 / lanes[lane].count;
            const double delta_x = x - lanes[lane].mean_x;
            lanes[lane].mean_x  += delta_x * inv_count;
            const double delta_y = y - lanes[lane].mean_y;
            lanes[lane].mean_y  += delta_y * inv_count;
            lanes[lane].m2_x += delta_x * (x - lanes[lane].mean_x);
            lanes[lane].m2_y += delta_y * (y - lanes[lane].mean_y);
            lanes[lane].c_xy += delta_x * (y - lanes[lane].mean_y);
        }
    }

    BivariateState final_state = lanes[0];
    for (std::size_t lane = 1; lane < SIMD_WIDTH; ++lane) {
        final_state = merge_bivariate(final_state, lanes[lane]);
    }

    for (; i < batch_size; ++i) {
        BivariateState tail_state;
        tail_state.count = 1.0;
        tail_state.mean_x = in_x[i]; tail_state.mean_y = in_y[i];
        final_state = merge_bivariate(final_state, tail_state);
    }
    return final_state;
}

// ============================================================================
// 6. SIMD-Unrolled EWMCOV / EWMCORR Engine 
// ============================================================================
StatusCode process_ewmcov_batch_simd(
    EwmaBivariateState* __restrict__ state,
    const double* __restrict__ in_x,
    const double* __restrict__ in_y,
    double* __restrict__ out_cov,
    double* __restrict__ out_corr,
    std::size_t batch_size) noexcept
{
    if (!state || !in_x || !in_y || !out_cov || !out_corr) return StatusCode::NullPointer;

    const double alpha = state->alpha, beta = 1.0 - alpha;
    const double beta2 = beta * beta, beta3 = beta2 * beta, beta4 = beta3 * beta;

    double mx = state->mean_x, my = state->mean_y;
    double vx = state->var_x, vy = state->var_y, cov = state->cov_xy;
    std::size_t i = 0;
    const std::size_t simd_limit = batch_size - (batch_size % SIMD_WIDTH);

    for (; i < simd_limit; i += SIMD_WIDTH) {
        const double x0 = in_x[i], x1 = in_x[i+1], x2 = in_x[i+2], x3 = in_x[i+3];
        const double y0 = in_y[i], y1 = in_y[i+1], y2 = in_y[i+2], y3 = in_y[i+3];

        const double mx0 = alpha * x0 + beta * mx, my0 = alpha * y0 + beta * my;
        const double mx1 = alpha * x1 + alpha * beta * x0 + beta2 * mx, my1 = alpha * y1 + alpha * beta * y0 + beta2 * my;
        const double mx2 = alpha * x2 + alpha * beta * x1 + alpha * beta2 * x0 + beta3 * mx, my2 = alpha * y2 + alpha * beta * y1 + alpha * beta2 * y0 + beta3 * my;
        const double mx3 = alpha * x3 + alpha * beta * x2 + alpha * beta2 * x1 + alpha * beta3 * x0 + beta4 * mx, my3 = alpha * y3 + alpha * beta * y2 + alpha * beta2 * y1 + alpha * beta3 * y0 + beta4 * my;

        const double v_xx0 = (x0 - mx) * (x0 - mx0), v_yy0 = (y0 - my) * (y0 - my0), v_xy0 = (x0 - mx) * (y0 - my0); 
        const double v_xx1 = (x1 - mx0) * (x1 - mx1), v_yy1 = (y1 - my0) * (y1 - my1), v_xy1 = (x1 - mx0) * (y1 - my1);
        const double v_xx2 = (x2 - mx1) * (x2 - mx2), v_yy2 = (y2 - my1) * (y2 - my2), v_xy2 = (x2 - mx1) * (y2 - my2);
        const double v_xx3 = (x3 - mx2) * (x3 - mx3), v_yy3 = (y3 - my2) * (y3 - my3), v_xy3 = (x3 - mx2) * (y3 - my3);

        const double c0 = beta * cov + alpha * v_xy0;
        const double c1 = beta2 * cov + alpha * beta * v_xy0 + alpha * v_xy1;
        const double c2 = beta3 * cov + alpha * beta2 * v_xy0 + alpha * beta * v_xy1 + alpha * v_xy2;
        const double c3 = beta4 * cov + alpha * beta3 * v_xy0 + alpha * beta2 * v_xy1 + alpha * beta * v_xy2 + alpha * v_xy3;

        const double vx0 = beta * vx + alpha * v_xx0, vy0 = beta * vy + alpha * v_yy0;
        const double vx1 = beta2 * vx + alpha * beta * v_xx0 + alpha * v_xx1, vy1 = beta2 * vy + alpha * beta * v_yy0 + alpha * v_yy1;
        const double vx2 = beta3 * vx + alpha * beta2 * v_xx0 + alpha * beta * v_xx1 + alpha * v_xx2, vy2 = beta3 * vy + alpha * beta2 * v_yy0 + alpha * beta * v_yy1 + alpha * v_yy2;
        const double vx3 = beta4 * vx + alpha * beta3 * v_xx0 + alpha * beta2 * v_xx1 + alpha * beta * v_xx2 + alpha * v_xx3, vy3 = beta4 * vy + alpha * beta3 * v_yy0 + alpha * beta2 * v_yy1 + alpha * beta * v_yy2 + alpha * v_yy3;

        out_cov[i] = c0; out_cov[i+1] = c1; out_cov[i+2] = c2; out_cov[i+3] = c3;
        out_corr[i] = c0 / std::sqrt(std::max(vx0 * vy0, 1e-12)); out_corr[i+1] = c1 / std::sqrt(std::max(vx1 * vy1, 1e-12));
        out_corr[i+2] = c2 / std::sqrt(std::max(vx2 * vy2, 1e-12)); out_corr[i+3] = c3 / std::sqrt(std::max(vx3 * vy3, 1e-12));

        mx = mx3; my = my3; vx = vx3; vy = vy3; cov = c3;
    }

    for (; i < batch_size; ++i) {
        const double dx_prev = in_x[i] - mx, dy_prev = in_y[i] - my;
        mx = alpha * in_x[i] + beta * mx; my = alpha * in_y[i] + beta * my;
        const double dx_curr = in_x[i] - mx, dy_curr = in_y[i] - my;
        vx = beta * vx + alpha * (dx_prev * dx_curr); vy = beta * vy + alpha * (dy_prev * dy_curr);
        cov = beta * cov + alpha * (dx_prev * dy_curr);
        out_cov[i] = cov; out_corr[i] = cov / std::sqrt(std::max(vx * vy, 1e-12));
    }
    state->mean_x = mx; state->mean_y = my; state->var_x = vx; state->var_y = vy; state->cov_xy = cov;
    return StatusCode::Success;
}

// ============================================================================
// 7. SIMD-Friendly Higher Moments Batch Processor
// ============================================================================
HigherMomentsState process_higher_moments_batch_simd(const double* __restrict__ in_values, std::size_t batch_size) noexcept {
    HigherMomentsState lanes[SIMD_WIDTH] = {};
    std::size_t i = 0;
    const std::size_t simd_limit = batch_size - (batch_size % SIMD_WIDTH);

    for (; i < simd_limit; i += SIMD_WIDTH) {
        #pragma GCC unroll 4
        for (std::size_t lane = 0; lane < SIMD_WIDTH; ++lane) {
            const double x = in_values[i + lane];
            const double n1 = lanes[lane].count;
            lanes[lane].count += 1.0;
            const double n = lanes[lane].count;
            const double inv_n = 1.0 / n;
            const double delta = x - lanes[lane].mean;
            const double delta_n = delta * inv_n;
            const double delta_n2 = delta_n * delta_n;
            const double term1 = delta * delta_n * n1;

            lanes[lane].mean += delta_n;
            lanes[lane].m4 += term1 * delta_n2 * (n * n - 3.0 * n + 3.0) + 6.0 * delta_n2 * lanes[lane].m2 - 4.0 * delta_n * lanes[lane].m3;
            lanes[lane].m3 += term1 * delta_n * (n - 2.0) - 3.0 * delta_n * lanes[lane].m2;
            lanes[lane].m2 += term1;
        }
    }

    HigherMomentsState final_state = lanes[0];
    for (std::size_t lane = 1; lane < SIMD_WIDTH; ++lane) {
        final_state = merge_higher_moments(final_state, lanes[lane]);
    }

    for (; i < batch_size; ++i) {
        HigherMomentsState tail;
        tail.count = 1.0; tail.mean = in_values[i];
        final_state = merge_higher_moments(final_state, tail);
    }
    return final_state;
}

} // namespace Layer6
} // namespace ONITRO