#pragma once
#include <cmath>
#include <stdexcept>

namespace onitro {

class EWMAStats {
public:
    explicit EWMAStats(double lambda = 0.94)
        : lambda_(lambda), alpha_(1.0 - lambda),
          init_(false), mean_(0.0), var_(0.0)
    {
        if (lambda <= 0.0 || lambda >= 1.0)
            throw std::invalid_argument("lambda must be in (0, 1)");
    }

    void push(double x) {
        if (!init_) {
            mean_ = x;
            var_  = 0.0;
            init_ = true;
        } else {
            double old_mean = mean_;
            mean_ = lambda_ * mean_ + alpha_ * x;
            var_  = lambda_ * var_  + alpha_ * (x - old_mean) * (x - old_mean);
        }
    }

    bool   ready()  const { return init_; }
    double mean()   const { check(); return mean_; }
    double var()    const { check(); return var_;  }
    double stddev() const { check(); return std::sqrt(var_); }

    double zscore(double x) const {
        double s = stddev();
        if (s < 1e-15) throw std::runtime_error("stddev near-zero");
        return (x - mean_) / s;
    }

    static EWMAStats fromSpan(double span) {
        if (span < 1.0) throw std::invalid_argument("span must be >= 1");
        return EWMAStats(1.0 - 2.0 / (span + 1.0));
    }

    static EWMAStats fromHalflife(double halflife) {
        if (halflife <= 0.0) throw std::invalid_argument("halflife must be > 0");
        return EWMAStats(std::exp(-std::log(2.0) / halflife));
    }

private:
    void check() const {
        if (!init_) throw std::runtime_error("no observations yet");
    }

    double lambda_, alpha_;
    bool   init_;
    double mean_, var_;
};

} // namespace onitro