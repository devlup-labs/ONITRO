// onitro/streaming/RollingStats.hpp
#pragma once
#include <ql/math/statistics/statistics.hpp>
#include <deque>
#include <stdexcept>
#include <cmath>

namespace onitro {

class RollingStats {
public:
    explicit RollingStats(std::size_t window)
        : window_(window)
    {
        if (window == 0)
            throw std::invalid_argument("window must be > 0");
    }

    void push(double x) {
        buf_.push_back(x);
        if (buf_.size() > window_)
            buf_.pop_front();//O(1)
        rebuild();        
    }

    bool        ready()   const { return buf_.size() >= 2; }
    std::size_t count()   const { return buf_.size(); }

    double mean()   const { check(); return stats_.mean(); }
    double var()    const { check(); return stats_.variance(); }      
    double stddev() const { check(); return stats_.standardDeviation(); }

    double zscore(double x) const {
        double s = stddev();
        if (s < 1e-15) throw std::runtime_error("stddev is near-zero");
        return (x - mean()) / s;
    }

    
    double zscore() const {
        if (buf_.empty()) throw std::runtime_error("no data");
        return zscore(buf_.back());
    }

    
    double min()  const { check(); return stats_.min(); }
    double max()  const { check(); return stats_.max(); }
    double skew() const { check(); return stats_.skewness(); }

private:
    void rebuild() {//O(n)
        stats_.reset();
        for (double v : buf_)
            stats_.add(v);
    }

    void check() const {
        if (buf_.size() < 2)
            throw std::runtime_error("need at least 2 observations");
    }

    std::size_t           window_;
    std::deque<double>    buf_;
    QuantLib::Statistics  stats_;  
};

} // namespace onitro