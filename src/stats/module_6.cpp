#include <cstdint>
#include <cstddef>
#include <cmath>

namespace ONITRO {
namespace Layer6 {

// ============================================================================
// Feature Block: Welford's Online Variance
// Numerically stable single-pass mean / variance accumulation.
// ============================================================================
struct alignas(64) WelfordState {
    double  mean;   // running mean
    double  m2;     // sum of squares of differences from the current mean
    double  count;  // number of samples ingested (double to avoid int->fp casts in hot loop)
    // --- padding to fill the cache line ---
    double  _pad[5];
};

// Updates the Welford accumulator with a single sample and writes back the
// current rolling mean and (population) variance for this index.
static inline __attribute__((always_inline))
void welford_update(WelfordState* __restrict__ state,
                    double x,
                    double* __restrict__ out_mean,
                    double* __restrict__ out_variance)
{
    state->count += 1.0;
    const double delta  = x - state->mean;
    state->mean        += delta / state->count;
    const double delta2 = x - state->mean;
    state->m2          += delta * delta2;

    const double inv_count = 1.0 / state->count;
    *out_mean     = state->mean;
    *out_variance = state->m2 * inv_count;
}

// ============================================================================
// Feature Block: EWMA (Exponentially Weighted Moving Average)
// current_ewma = alpha * x + (1 - alpha) * current_ewma
// ============================================================================
struct alignas(64) EwmaState {
    double  current_ewma;  // last computed EWMA value
    double  alpha;         // smoothing factor in (0, 1]
    // --- padding to fill the cache line ---
    double  _pad[6];
};

// Advances the EWMA state by one sample and writes back the smoothed value.
static inline __attribute__((always_inline))
void ewma_update(EwmaState* __restrict__ state,
                 double x,
                 double* __restrict__ out_ewma)
{
    state->current_ewma = state->alpha * x
                        + (1.0 - state->alpha) * state->current_ewma;
    *out_ewma = state->current_ewma;
}

// ============================================================================
// SIMD Hot-Loop: master batch processor
// Consumes Structure-of-Arrays inputs from Layer 2's mmap region (read-only,
// __restrict__-qualified) and emits Structure-of-Arrays outputs.
// ============================================================================
void process_batch(const double*   __restrict__ in_values,
                   const uint64_t* __restrict__ in_timestamps,
                   double*         __restrict__ out_mean,
                   double*         __restrict__ out_variance,
                   double*         __restrict__ out_ewma,
                   WelfordState*   __restrict__ welford,
                   EwmaState*      __restrict__ ewma,
                   std::size_t batch_size)
{
    (void)in_timestamps; // reserved for future time-decay weighting (Layer 6.x)

#pragma GCC ivdep
    for (std::size_t i = 0; i < batch_size; ++i) {
        const double x = in_values[i];

        welford_update(welford, x, &out_mean[i], &out_variance[i]);
        ewma_update(ewma, x, &out_ewma[i]);
    }
}

} // namespace Layer6
} // namespace ONITRO