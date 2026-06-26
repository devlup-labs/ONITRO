#include "module_6_correlation.hpp"
#include <cmath>
#include <numeric>
#include <stdexcept>

double CorrelationCalculator::calculateCovariance(const std::vector<double>& returnsA, const std::vector<double>& returnsB) {
    if (returnsA.size() != returnsB.size() || returnsA.empty()) {
        throw std::invalid_argument("Vectors must be of the same non-zero size to calculate covariance.");
    }
    
    double meanA = std::accumulate(returnsA.begin(), returnsA.end(), 0.0) / returnsA.size();
    double meanB = std::accumulate(returnsB.begin(), returnsB.end(), 0.0) / returnsB.size();
    
    double covSum = 0.0;
    for (std::size_t i = 0; i < returnsA.size(); ++i) {
        covSum += (returnsA[i] - meanA) * (returnsB[i] - meanB);
    }
    
    return covSum / (returnsA.size() - 1);
}

double CorrelationCalculator::calculateCorrelation(const std::vector<double>& returnsA, const std::vector<double>& returnsB) {
    if (returnsA.size() != returnsB.size() || returnsA.empty()) {
        throw std::invalid_argument("Vectors must be of the same non-zero size to calculate correlation.");
    }
    
    double covariance = calculateCovariance(returnsA, returnsB);
    
    double meanA = std::accumulate(returnsA.begin(), returnsA.end(), 0.0) / returnsA.size();
    double meanB = std::accumulate(returnsB.begin(), returnsB.end(), 0.0) / returnsB.size();
    
    double varSumA = 0.0;
    double varSumB = 0.0;
    
    for (std::size_t i = 0; i < returnsA.size(); ++i) {
        varSumA += (returnsA[i] - meanA) * (returnsA[i] - meanA);
        varSumB += (returnsB[i] - meanB) * (returnsB[i] - meanB);
    }
    
    double stdDevA = std::sqrt(varSumA / (returnsA.size() - 1));
    double stdDevB = std::sqrt(varSumB / (returnsB.size() - 1));
    
    if (stdDevA == 0.0 || stdDevB == 0.0) return 0.0;
    
    return covariance / (stdDevA * stdDevB);
}

double CorrelationCalculator::calculateBeta(const std::vector<double>& assetReturns, const std::vector<double>& benchmarkReturns) {
    if (assetReturns.size() != benchmarkReturns.size() || assetReturns.empty()) {
        throw std::invalid_argument("Vectors must be of the same non-zero size to calculate Beta.");
    }

    double covariance = calculateCovariance(assetReturns, benchmarkReturns);
    
    // Variance of the benchmark
    double meanB = std::accumulate(benchmarkReturns.begin(), benchmarkReturns.end(), 0.0) / benchmarkReturns.size();
    double varSumB = 0.0;
    for (double r : benchmarkReturns) {
        varSumB += (r - meanB) * (r - meanB);
    }
    double varianceB = varSumB / (benchmarkReturns.size() - 1);

    if (varianceB == 0.0) return 0.0;
    
    return covariance / varianceB;
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
    std::size_t numAssets = multiAssetReturns.size();
    std::vector<std::vector<double>> corrMatrix(numAssets, std::vector<double>(numAssets, 1.0));
    
    for (std::size_t i = 0; i < numAssets; ++i) {
        for (std::size_t j = i + 1; j < numAssets; ++j) {
            double corr = calculateCorrelation(multiAssetReturns[i], multiAssetReturns[j]);
            corrMatrix[i][j] = corr;
            corrMatrix[j][i] = corr; // symmetric
        }
    }
    return corrMatrix;
}

std::vector<double> CorrelationCalculator::calculateMultiAssetBetas(const std::vector<std::vector<double>>& multiAssetReturns, const std::vector<double>& benchmarkReturns) {
    std::vector<double> betas;
    betas.reserve(multiAssetReturns.size());
    for (const auto& assetReturns : multiAssetReturns) {
        betas.push_back(calculateBeta(assetReturns, benchmarkReturns));
    }
    return betas;
}
