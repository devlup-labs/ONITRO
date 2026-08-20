# Module 6: Streaming Analytics and Correlation Workspace

This workspace contains implementations for streaming financial statistics and correlation analysis, designed to process time-series data (like daily stock prices) and generate rolling metrics.

## Contents

- `main.cpp`: The main entry point. Reads asset and market data from CSV files, computes various streaming statistics (EWMA, Rolling Stats, Correlation, Beta), and outputs the results to `output.csv`.
- `EWMAStats.hpp`: A class for computing Exponentially Weighted Moving Averages (EWMA) and related statistics (Variance, Standard Deviation, Z-Score) in a streaming fashion.
- `RollingStats.hpp`: A class that utilizes a rolling window (using `std::deque` and `QuantLib::Statistics`) to compute rolling mean, variance, standard deviation, z-score, min, max, and skewness.
- `module_6_correlation.hpp` & `.cpp`: A utility class (`CorrelationCalculator`) for calculating static and rolling Pearson correlation, covariance, and beta between assets and a benchmark.

## Dependencies

- C++17 or higher
- [QuantLib](https://www.quantlib.org/) (Required for `QuantLib::Statistics` used in `RollingStats.hpp`)

## Usage

1. Ensure you have your input CSV files ready (e.g., `TCS.csv` and `NIFTY.csv`). The CSVs should have a header row and contain at least the `Date` (column 1) and `Close` (column 5) prices.
2. Compile the project linking against QuantLib.
   ```bash
   g++ -std=c++17 main.cpp module_6_correlation.cpp -o stats_engine -lQuantLib
   ```
3. Run the executable:
   ```bash
   ./stats_engine
   ```
4. The results will be written to `output.csv` containing the merged dates, prices, and all calculated streaming statistics.
