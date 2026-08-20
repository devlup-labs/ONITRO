#include "module_6_correlation.hpp"
#include <ql/math/statistics/sequencestatistics.hpp>
#include <ql/math/matrix.hpp>
#include <stdexcept>

namespace onitro {

double CorrelationCalculator::calculateCovariance(const std::vector<double>& returnsA, const std::vector<double>& returnsB) {
    if (returnsA.size() != returnsB.size() || returnsA.empty()) {
        throw std::invalid_argument("Vectors must be of the same non-zero size to calculate covariance.");
    }
    
    QuantLib::SequenceStatistics stats(2);
    std::vector<double> sample(2);
    for (std::size_t i = 0; i < returnsA.size(); ++i) {
        sample[0] = returnsA[i];
        sample[1] = returnsB[i];
        stats.add(sample);
    }
    
    QuantLib::Matrix cov = stats.covariance();
    return cov[0][1];
}

double CorrelationCalculator::calculateCorrelation(const std::vector<double>& returnsA, const std::vector<double>& returnsB) {
    if (returnsA.size() != returnsB.size() || returnsA.empty()) {
        throw std::invalid_argument("Vectors must be of the same non-zero size to calculate correlation.");
    }
    
    QuantLib::SequenceStatistics stats(2);
    std::vector<double> sample(2);
    for (std::size_t i = 0; i < returnsA.size(); ++i) {
        sample[0] = returnsA[i];
        sample[1] = returnsB[i];
        stats.add(sample);
    }
    
    QuantLib::Matrix cor = stats.correlation();
    return cor[0][1];
}

double CorrelationCalculator::calculateBeta(const std::vector<double>& assetReturns, const std::vector<double>& benchmarkReturns) {
    if (assetReturns.size() != benchmarkReturns.size() || assetReturns.empty()) {
        throw std::invalid_argument("Vectors must be of the same non-zero size to calculate Beta.");
    }

    QuantLib::SequenceStatistics stats(2);
    std::vector<double> sample(2);
    for (std::size_t i = 0; i < assetReturns.size(); ++i) {
        sample[0] = assetReturns[i];
        sample[1] = benchmarkReturns[i];
        stats.add(sample);
    }
    
    QuantLib::Matrix cov = stats.covariance();
    if (cov[1][1] == 0.0) return 0.0;
    return cov[0][1] / cov[1][1];
}

std::vector<double> CorrelationCalculator::calculateRollingCorrelation(const std::vector<double>& returnsA, const std::vector<double>& returnsB, int windowSize) {
    if (returnsA.size() != returnsB.size()) {
        throw std::invalid_argument("Vectors must be of the same size to calculate rolling correlation.");
    }
    
    std::vector<double> rollingCorr(returnsA.size(), 0.0);
    
    if (returnsA.size() < static_cast<std::size_t>(windowSize) || windowSize < 2) {
        return rollingCorr;
    }
    
    for (std::size_t i = windowSize - 1; i < returnsA.size(); ++i) {
        std::vector<double> windowA(returnsA.begin() + (i - windowSize + 1), returnsA.begin() + i + 1);
        std::vector<double> windowB(returnsB.begin() + (i - windowSize + 1), returnsB.begin() + i + 1);
        
        rollingCorr[i] = calculateCorrelation(windowA, windowB);
    }
    
    return rollingCorr;
}

std::vector<double> CorrelationCalculator::calculateRollingBeta(const std::vector<double>& assetReturns, const std::vector<double>& benchmarkReturns, int windowSize) {
    if (assetReturns.size() != benchmarkReturns.size()) {
        throw std::invalid_argument("Vectors must be of the same size to calculate rolling beta.");
    }
    
    std::vector<double> rollingBeta(assetReturns.size(), 0.0);
    
    if (assetReturns.size() < static_cast<std::size_t>(windowSize) || windowSize < 2) {
        return rollingBeta;
    }
    
    for (std::size_t i = windowSize - 1; i < assetReturns.size(); ++i) {
        std::vector<double> windowAsset(assetReturns.begin() + (i - windowSize + 1), assetReturns.begin() + i + 1);
        std::vector<double> windowBench(benchmarkReturns.begin() + (i - windowSize + 1), benchmarkReturns.begin() + i + 1);
        
        rollingBeta[i] = calculateBeta(windowAsset, windowBench);
    }
    
    return rollingBeta;
}

std::vector<std::vector<double>> CorrelationCalculator::calculateCorrelationMatrix(const std::vector<std::vector<double>>& multiAssetReturns) {
    if (multiAssetReturns.empty()) return {};
    std::size_t numAssets = multiAssetReturns.size();
    if (numAssets == 0 || multiAssetReturns[0].empty()) return {};
    std::size_t numPeriods = multiAssetReturns[0].size();
    
    QuantLib::SequenceStatistics stats(numAssets);
    std::vector<double> sample(numAssets);
    
    for (std::size_t i = 0; i < numPeriods; ++i) {
        for (std::size_t j = 0; j < numAssets; ++j) {
            sample[j] = multiAssetReturns[j][i];
        }
        stats.add(sample);
    }
    
    QuantLib::Matrix cor = stats.correlation();
    std::vector<std::vector<double>> corrMatrix(numAssets, std::vector<double>(numAssets, 1.0));
    
    for (std::size_t i = 0; i < numAssets; ++i) {
        for (std::size_t j = 0; j < numAssets; ++j) {
            corrMatrix[i][j] = cor[i][j];
        }
    }
    return corrMatrix;
}

std::vector<double> CorrelationCalculator::calculateMultiAssetBetas(const std::vector<std::vector<double>>& multiAssetReturns, const std::vector<double>& benchmarkReturns) {
    if (multiAssetReturns.empty()) return {};
    std::size_t numAssets = multiAssetReturns.size();
    if (numAssets == 0 || multiAssetReturns[0].empty()) return {};
    std::size_t numPeriods = multiAssetReturns[0].size();
    
    if (benchmarkReturns.size() != numPeriods) {
        throw std::invalid_argument("Benchmark returns size mismatch.");
    }
    
    QuantLib::SequenceStatistics stats(numAssets + 1);
    std::vector<double> sample(numAssets + 1);
    
    for (std::size_t i = 0; i < numPeriods; ++i) {
        for (std::size_t j = 0; j < numAssets; ++j) {
            sample[j] = multiAssetReturns[j][i];
        }
        sample[numAssets] = benchmarkReturns[i];
        stats.add(sample);
    }
    
    QuantLib::Matrix cov = stats.covariance();
    std::vector<double> betas(numAssets, 0.0);
    double benchVar = cov[numAssets][numAssets];
    
    if (benchVar != 0.0) {
        for (std::size_t i = 0; i < numAssets; ++i) {
            betas[i] = cov[i][numAssets] / benchVar;
        }
    }
    
    return betas;
}

} // namespace onitro
