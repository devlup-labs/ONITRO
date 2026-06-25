# Module 3: Forwards, Futures, and Fixed Income

Complete implementation of Module 3 with **live NSE option chain data collection**, forward/futures pricing, bond pricing, duration, convexity, and yield-to-maturity calculations.

## Overview

This module implements four core components of fixed income and derivatives analysis:

1. **Data Collection**: Fetch live NSE option chain data for Indian equities (production-ready)
2. **Forward/Futures Pricing**: Fair value computation using cost-of-carry model
3. **Bond Pricing**: Zero-coupon and coupon bond valuation
4. **YTM & Duration Analysis**: Yield-to-maturity, duration, and convexity calculations

## Files

### 1. `data_collector.py` ⭐ PRODUCTION-READY NSE DATA FETCHER
Fetches live option chain data directly from NSE (National Stock Exchange of India) and bridges it with QuantLib analytical functions.

**Key Classes**:
- `NSESessionManager`: Secure NSE API sessions with retry logic and rotating User-Agents
- `NSEOptionChainParser`: Parses JSON and enriches with QuantLib fields
- `NSEOptionChainDataCollector`: Main orchestrator for 2-stock option chain collection

**Features**:
- ✅ Live NSE API integration (no yfinance limitations)
- ✅ Session management with connection block bypass
- ✅ Automatic date conversion to QuantLib.Date format
- ✅ Time-to-Maturity calculation using Actual365Fixed day counter
- ✅ Structured CSV output for forward, bond, and YTM analytics

**Usage**:
```python
from data_collector import NSEOptionChainDataCollector

collector = NSEOptionChainDataCollector(
    stable_symbol='TCS',           # Tata Consultancy Services
    volatile_symbol='ICICIBANK',   # ICICI Bank  
    output_dir='./data',
    risk_free_rate=0.07,           # 7% (India RBI rate)
    dividend_yield=0.02,           # 2% expected dividend
    timeout=10
)

# Collect live data and save
stable_path, volatile_path = collector.collect_and_save()

# Print summary
collector.get_summary_statistics()
```

**Output CSV Columns** (QuantLib-compatible):
| Column | Type | Purpose |
|--------|------|---------|
| `spot_price` | float | Underlying S |
| `strike_price` | float | Option strike K |
| `option_type` | str | CALL or PUT |
| `ltp` | float | Market price (option value) |
| `time_to_maturity` | float | T in years (Actual365Fixed) |
| `risk_free_rate` | float | r (decimal) |
| `dividend_yield` | float | q (decimal) |
| `bid`, `ask`, `mid_price` | float | Pricing bounds |
| `volume`, `open_interest` | int | Liquidity metrics |

**NSE API Details**:
- Endpoint: `https://www.nseindia.com/api/optionchain?mode=STRIKEPRICE&symbol=SYMBOL`
- Spot Price: `https://www.nseindia.com/api/quote-derivative?symbol=SYMBOL`
- Headers: Rotating User-Agents + CORS compliance
- Retry: Exponential backoff (max 3 retries)
- Timeout: Configurable (default 10 seconds)

**See Also**: `NSE_DATA_COLLECTOR_GUIDE.md` for comprehensive documentation

### 2. `integration_example.py` ⭐ NEW: QuantLib Bridge
End-to-end example showing how to use NSE data with QuantLib calculators.

**Key Class**:
- `NSEQuantLibBridge`: Transforms NSE data into QuantLib inputs and runs analysis

**Includes**:
- Forward price calculations
- Bond pricing
- YTM-equivalent calculations
- P&L scenario analysis
- Comprehensive reporting

**Usage**:
```bash
python integration_example.py
```

**Output**: Analysis results in `./results/` directory with:
- `forward_pricing.csv` - Forward prices and cost-of-carry
- `ytm_equivalent.csv` - Bond-like yield analysis

### 3. `module_3_forwards_futures.py`
Implements forward and futures fair-value pricing using cost-of-carry model.

**Key Classes**:
- `ForwardFuturesCalculator`: Core forward/futures pricing
- `DataDrivenForwardCalculator`: Works with CSV data

**Features**:
- Forward fair value: F = S × e^((r-q)T)
- Futures pricing
- Cost-of-carry extraction
- Forward discount/premium calculation
- P&L analysis at maturity

**Usage**:
```python
from module_3_forwards_futures import ForwardFuturesCalculator

calc = ForwardFuturesCalculator(risk_free_rate=0.06, dividend_yield=0.01)

# 3-month forward on stock trading at 2000
spot_price = 2000
T = 3/12  # 0.25 years
forward_price = calc.forward_price(spot_price, T)
discount_premium = calc.forward_discount_premium(spot_price, forward_price)

print(f"Forward Price: {forward_price:.2f}")
print(f"Discount/Premium: {discount_premium:.2f}%")
```

### 3. `module_3_forwards_futures.py`
Implements forward and futures fair-value pricing using cost-of-carry model.

**Key Classes**:
- `ForwardFuturesCalculator`: Core forward/futures pricing
- `DataDrivenForwardCalculator`: Works with CSV data from NSE collector

**Features**:
- Forward fair value: F = S × e^((r-q)T)
- Futures pricing
- Cost-of-carry extraction
- Forward discount/premium calculation
- P&L analysis at maturity

**Usage**:
```python
from module_3_forwards_futures import ForwardFuturesCalculator

calc = ForwardFuturesCalculator(risk_free_rate=0.06, dividend_yield=0.01)

# 3-month forward on stock trading at 2000
spot_price = 2000
T = 3/12  # 0.25 years
forward_price = calc.forward_price(spot_price, T)
discount_premium = calc.forward_discount_premium(spot_price, forward_price)

print(f"Forward Price: {forward_price:.2f}")
print(f"Discount/Premium: {discount_premium:.2f}%")
```

### 4. `module_3_bonds.py`
Comprehensive bond pricing and analysis module.

**Key Classes**:
- `BondPricingCalculator`: Zero-coupon and coupon bond pricing
- `YieldToMaturitySolver`: YTM calculation using Newton-Raphson
- `DurationConvexity`: Duration and convexity calculations

**Features**:
- Zero-coupon bond pricing
- Coupon bond pricing (vectorized)
- YTM solver (closed-form for ZCB, Newton-Raphson for coupon bonds)
- Macaulay duration
- Modified duration
- Bond convexity
- Price change estimation

**Usage**:
```python
from module_3_bonds import BondPricingCalculator, YieldToMaturitySolver, DurationConvexity

# Zero-coupon bond
zcb_price = BondPricingCalculator.zero_coupon_bond_price(
    face_value=1000,
    yield_rate=0.05,
    years_to_maturity=5
)

# Coupon bond
cb_price = BondPricingCalculator.coupon_bond_price_vectorized(
    face_value=1000,
    coupon_rate=0.06,
    yield_rate=0.05,
    years_to_maturity=5,
    payment_frequency=2
)

# YTM solver
ytm = YieldToMaturitySolver.ytm_coupon_bond(
    face_value=1000,
    coupon_rate=0.06,
    bond_price=950,
    years_to_maturity=5
)

# Duration and convexity
duration = DurationConvexity.macaulay_duration(1000, 0.06, 0.05, 5)
convexity = DurationConvexity.convexity(1000, 0.06, 0.05, 5)
```

### 5. `module_3_ytm_solver.py`
Comprehensive YTM, duration, and convexity analyzer with scenario analysis.

**Key Classes**:
- `ComprehensiveYTMAnalyzer`: Full bond analysis suite

**Features**:
- YTM solving from bond price
- Price sensitivity analysis
- Scenario analysis (Bear, Base, Bull markets)
- Bond summary metrics
- Export to JSON and CSV

**Usage**:
```python
from module_3_ytm_solver import ComprehensiveYTMAnalyzer

analyzer = ComprehensiveYTMAnalyzer(
    face_value=1000,
    coupon_rate=0.06,
    current_yield=0.05,
    years_to_maturity=5,
    payment_frequency=2
)

# Print summary
analyzer.print_summary()

# Price sensitivity
sensitivity = analyzer.price_sensitivity_analysis()

# Scenario analysis
scenarios = analyzer.scenario_analysis({
    'Bear_Market': 0.07,
    'Base_Case': 0.05,
    'Bull_Market': 0.03
})

# YTM from bond price
ytm_result = analyzer.ytm_from_price(bond_price=950)

# Export analysis
analyzer.export_analysis('ytm_analysis.json')
```

### 6. `module_3_integrated_example.py`
Complete end-to-end example integrating all Module 3 components with NSE data.

**Class**:
- `Module3Integration`: Orchestrates all analysis steps

**Steps**:
1. NSE option chain data collection
2. Forward/futures pricing analysis
3. Bond pricing analysis
4. YTM, duration, and convexity analysis

**Usage**:
```bash
python module_3_integrated_example.py
```

**Output Files**:
- `results/data/stable_stock_TCS.csv` - Option chain data
- `results/data/volatile_stock_ICICIBANK.csv` - Option chain data
- `results/forwards_futures_analysis.csv` - Forward prices
- `results/zero_coupon_bonds.csv` - ZCB prices
- `results/coupon_bonds.csv` - Coupon bond prices
- `results/ytm_analysis.json` - YTM analysis
- `results/price_sensitivity.csv` - Sensitivity analysis

## Quick Start

### Installation

```bash
# Install dependencies
pip install -r requirements.txt
```

**Dependencies**:
- `requests>=2.28.0` - NSE API communication
- `pandas>=1.3.0` - Data manipulation
- `numpy>=1.21.0` - Numerical computing
- `urllib3>=1.26.0` - HTTP connection management

### Basic Workflow

#### Step 1: Collect Live NSE Option Chain Data
```bash
python data_collector.py
```

Outputs:
- `./data/options_tcs.csv` - TCS option chain (low volatility)
- `./data/options_icicibank.csv` - ICICIBANK option chain (high volatility)

#### Step 2: Bridge to QuantLib Analytics
```bash
python integration_example.py
```

This integrates NSE data with QuantLib calculators and produces:
- Forward pricing analysis
- Bond pricing output
- YTM-equivalent calculations
- P&L scenarios

Results saved to `./results/` directory

#### Step 3: Analyze Results
```python
import pandas as pd

# Load forward pricing results
fwd_df = pd.read_csv('./results/forward_pricing.csv')
print(fwd_df[['strike', 'spot_price', 'forward_price', 'forward_premium_pct']])

# Load forward pricing results
fwd_df = pd.read_csv('./results/forward_pricing.csv')
print(fwd_df[['strike', 'spot_price', 'forward_price', 'forward_premium_pct']])

# Load YTM results
ytm_df = pd.read_csv('./results/ytm_equivalent.csv')
print(ytm_df[['strike', 'option_type', 'time_value_pct', 'ytm_equivalent']])
```

### Data Flow Architecture

```
NSE API (LIVE DATA)
    ↓
NSESessionManager (Connection Mgmt)
    ↓
NSEOptionChainParser (JSON → DataFrame)
    ↓
NSEOptionChainDataCollector (Orchestration)
    ↓
CSV Output (QuantLib-Compatible)
    ↓
NSEQuantLibBridge (Data Transformation)
    ↓
QuantLib Calculators
    ├─ ForwardPricesCalculator
    ├─ BondPricingCalculator
    └─ YTMAnalyzer
    ↓
Results CSV + JSON
```

## Mathematical Formulas

### Forward Fair Value (Cost-of-Carry Model)
```
F = S × e^((r-q)T)
```
where:
- F = Forward price
- S = Spot price
- r = Risk-free rate (from NSE data)
- q = Dividend yield (from NSE data)
- T = Time to maturity in years (Actual365Fixed)


### Zero-Coupon Bond Price
```
P = FV / (1 + y)^T
```
where:
- P = Bond price
- FV = Face value
- y = Yield
- T = Time to maturity

### Coupon Bond Price
```
P = Σ[C/(1+y)^t] + FV/(1+y)^T
```
where:
- C = Coupon payment
- t = Time period
- T = Total periods

### Macaulay Duration
```
D_M = Σ[t × PV(CF_t)] / Bond_Price
```

### Modified Duration
```
D_M* = D_M / (1 + y)
```

### Bond Convexity
```
Convexity = Σ[t(t+1) × PV(CF_t)] / (Bond_Price × (1+y)^2)
```

### Price Change Estimation
```
ΔP ≈ -D_M* × P × Δy + 0.5 × Convexity × P × (Δy)^2
```

## Data Parameters (NSE Integration)

### Default Risk-Free Rate
- **7% (0.07)** - Aligned with India's RBI policy rate
- Configurable per collector instance
- Used in forward pricing and bond valuation

### Default Dividend Yield
- **2% (0.02)** - Conservative estimate for equity index
- Configurable per collector instance
- Used in cost-of-carry calculations

### NSE Market Parameters
- **Market Hours**: 9:15 AM - 3:30 PM IST (India Standard Time)
- **Day Counter**: Actual365Fixed (365 days/year)
- **Symbols**: Use NSE convention (TCS, ICICIBANK, SBIN, etc. - no .NS suffix)
- **Expiries**: Monthly (third Thursday) and weekly options
- **Tick Size**: ₹0.05 for most options


### Bond Parameters (for Valuation Scenarios)
- Face Value: ₹1000
- Coupon Rates: 4%, 6%, 8%
- Yield Rates: 4%, 5%, 6%, 7%
- Maturities: 1, 3, 5, 10 years
- Payment Frequency: Semi-annual (2x per year)

## Running Examples

### 1. Collect Live NSE Data
```bash
# Quick start - fetch latest option chains for TCS and ICICIBANK
python data_collector.py
```

### 2. Integrate with QuantLib Analytics
```bash
# Run complete analysis pipeline
python integration_example.py
```

### 3. Detailed NSE Data Documentation
```bash
# Read comprehensive guide
cat NSE_DATA_COLLECTOR_GUIDE.md
```

### 4. Run Individual Calculators (C++)
```bash
# Forward & Futures analysis
./build/module_3_integrated_example

# YTM Solver
./build/module_3_ytm_solver_example

# Quick examples
./build/quick_start_examples
```

## Dependencies

### Python Requirements
```
requests>=2.28.0       # NSE API HTTP client
pandas>=1.3.0          # Data manipulation & CSV I/O
numpy>=1.21.0          # Numerical computations
urllib3>=1.26.0        # HTTP connection pooling
scipy>=1.7.0           # Numerical methods and statistics
```

### Install
```bash
pip install -r requirements.txt
```

### Optional (for C++ Examples)
- QuantLib >= 1.20
- CMake >= 3.10
- C++11 or later compiler

## Key Concepts Implemented

### 1. NSE Live Data Integration
Real-time option chain data collection from National Stock Exchange India with:
- Secure API session management
- Connection block bypass via User-Agent rotation
- Retry logic with exponential backoff
- Automatic QuantLib data enrichment

### 2. Cost-of-Carry Model
Foundational framework for pricing forwards and futures using the relationship between:
- Spot price (S) from NSE
- Interest rates (r) - default 7% RBI rate
- Dividend yields (q) - default 2% estimate
- Time to maturity (T) calculated using Actual365Fixed

### 3. Fixed Income Valuation
Complete bond pricing framework covering:
- Zero-coupon bond pricing
- Coupon-bearing bonds with various frequencies
- Portfolio duration analysis
- Interest rate risk quantification

### 4. YTM Solver
Newton-Raphson iterative method for solving the yield that equates:
- Bond price to present value of future cash flows
- Forward price to spot/carry costs

### 5. Interest Rate Risk
Duration measures how sensitive bonds are to yield changes:
- **Macaulay Duration**: Weighted average time to cash flow
- **Modified Duration**: Price sensitivity to yield changes
- **Convexity**: Captures nonlinearity in price-yield relationship


### 7. Scenario Analysis
Performance evaluation under different market conditions:
- Bear market (yields up, prices down)
- Base case (current parameters)
- Bull market (yields down, prices up)

## Troubleshooting

### No Data Returned
**Problem**: NSE returns empty option chain
**Solutions**:
- Verify market is open (9:15 AM - 3:30 PM IST)
- Check symbol spelling (use NSE format: TCS, not TCS.NS)
- Increase timeout: `NSEOptionChainDataCollector(timeout=20)`

### Connection Timeouts
**Problem**: API requests timing out
**Solutions**:
- Increase timeout parameter
- Check network/firewall rules
- Try different time (off-peak hours)
- Check NSE service status

### Date Parsing Errors
**Problem**: Unable to parse expiry date from NSE
**Solutions**:
- NSE date formats are flexible; script handles 25-JAN-2024, 25Jan2024, etc.
- If error persists, file GitHub issue with full error trace

## Extensions & Variations

### For Production Use:
- Implement caching to reduce API calls
- Add rate limiting (max 1 request/symbol/5 sec)
- Store historical data for backtesting
- Extend to multiple expiries and strikes
- Add transaction costs and slippage
- Implement portfolio sensitivity aggregation

### For Research:
- Add Monte Carlo simulation for path-dependent options
- Implement term structure modeling
- Add volatility skew and smile effects
- Extend to exotic derivatives (barriers, lookback, etc.)
- Add portfolio optimization
- Implement stress testing and VaR

### For Production Derivatives Desk:
- Live forward and bond pricing dashboards
- Multi-asset correlation analysis
- Counterparty credit risk (CVA)
- XVA (Valuation adjustments)
- Collateral optimization
- Position reconciliation

## C++ QuantLib Examples
The Module-3 folder includes QuantLib C++ implementations for identical analytics:

**Files**:
- `module_3_forwards_futures.hpp` / `.cpp` - Forward/futures pricing
- `module_3_bonds.hpp` / `.cpp` - Bond valuation and analytics
- `module_3_ytm_solver.hpp` / `.cpp` - YTM solving with scenario analysis
- `quick_start_examples.cpp` - Quick reference examples
- `module_3_integrated_example.cpp` - Complete integration example
- `CMakeLists.txt` - Build configuration

**Build**:
```bash
cd build
cmake ..
make
```

**Run C++ Examples**:
```bash
./module_3_integrated_example
./quick_start_examples
```

## File Manifest

```
Module-3/
├── data_collector.py              ⭐ NSE option chain fetcher (production)
├── integration_example.py          ⭐ QuantLib bridge & analytics
├── NSE_DATA_COLLECTOR_GUIDE.md    ⭐ Comprehensive NSE documentation
├── module_3_forwards_futures.cpp  Forward/futures C++ implementation
├── module_3_forwards_futures.hpp  Forward/futures headers
├── module_3_bonds.cpp             Bond pricing C++ implementation
├── module_3_bonds.hpp             Bond pricing headers
├── module_3_ytm_solver.cpp        YTM solver C++ implementation
├── module_3_ytm_solver.hpp        YTM solver headers
├── quick_start_examples.cpp       C++ quick reference
├── module_3_integrated_example.cpp C++ integrated example
├── CMakeLists.txt                 C++ build configuration
├── README.md                      This file
└── requirements.txt               Python dependencies

data/ (created at runtime)
├── options_tcs.csv                TCS option chain
└── options_icicibank.csv          ICICIBANK option chain

results/ (created at runtime)
├── forward_pricing.csv            Forward price analysis
├── ytm_equivalent.csv             Bond-like yield analysis
└── pnl_scenarios.csv              P&L across spot moves
```


## References

### NSE Documentation
- [NSE Official Website](https://www.nseindia.com)
- [NSE API Documentation](https://www.nseindia.com/api)
- [NSE Market Data](https://www.nseindia.com/market-data)

### QuantLib Resources
- [QuantLib Official](https://www.quantlib.org)
- [QuantLib Documentation](https://quantlib.org/reference/)
- [QuantLib C++ API](https://quantlib.org/reference/annotated.html)

### Financial Theory
- [Duration & Convexity](https://en.wikipedia.org/wiki/Bond_duration)
- [Yield-to-Maturity](https://en.wikipedia.org/wiki/Yield_to_maturity)
- [Duration & Convexity](https://en.wikipedia.org/wiki/Bond_duration)
- [Yield-to-Maturity](https://en.wikipedia.org/wiki/Yield_to_maturity)

## License

This implementation is part of the ONITRO derivatives analytics framework. Use in accordance with project LICENSE file.

## Support & Contributing

For issues, enhancements, or contributions:
1. Check troubleshooting section above
2. Review NSE_DATA_COLLECTOR_GUIDE.md for detailed diagnostics
3. Check NSE API status at https://www.nseindia.com
4. File issues on GitHub with:
   - Exact error message
   - Python/QuantLib versions
   - NSE symbols attempted
   - Network environment details

- Cost-of-Carry Model (Hull, 2021)
- Bond Pricing & Duration (Tuckman, 2011)
- Fixed Income Mathematics (Fabozzi, 2011)
