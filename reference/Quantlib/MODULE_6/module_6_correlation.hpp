#ifndef MODULE_6_CORRELATION_HPP
#define MODULE_6_CORRELATION_HPP

#include <vector>

namespace onitro {

class CorrelationCalculator {
public:
    // Calculates Pearson correlation coefficient between two assets' returns
    static double calculateCorrelation(const std::vector<double>& returnsA, const std::vector<double>& returnsB);
    
    // Calculates Covariance between two assets' returns
    static double calculateCovariance(const std::vector<double>& returnsA, const std::vector<double>& returnsB);
    
    // Calculates Beta of an asset against a benchmark (market)
    static double calculateBeta(const std::vector<double>& assetReturns, const std::vector<double>& benchmarkReturns);

    // Calculates Rolling Correlation between two assets over a given window
    static std::vector<double> calculateRollingCorrelation(const std::vector<double>& returnsA, const std::vector<double>& returnsB, int windowSize);

    // Calculates Rolling Beta of an asset against a benchmark
    static std::vector<double> calculateRollingBeta(const std::vector<double>& assetReturns, const std::vector<double>& benchmarkReturns, int windowSize);

    // Vectorized for multi-asset workloads: Computes correlation matrix for N assets
    // Input is a vector of return vectors (one for each asset)
    // Output is an NxN matrix (represented as vector of vectors)
    static std::vector<std::vector<double>> calculateCorrelationMatrix(const std::vector<std::vector<double>>& multiAssetReturns);

    // Vectorized for multi-asset workloads: Computes Betas for N assets against a single benchmark
    // Returns a vector of Betas, one for each asset
    static std::vector<double> calculateMultiAssetBetas(const std::vector<std::vector<double>>& multiAssetReturns, const std::vector<double>& benchmarkReturns);
};

} // namespace onitro

#endif // MODULE_6_CORRELATION_HPP
