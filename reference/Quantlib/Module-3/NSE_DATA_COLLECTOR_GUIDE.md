# NSE Option Chain Data Collector - Production Guide

## Overview

The `data_collector.py` script provides a production-ready bridge between live NSE (National Stock Exchange of India) option chain data and QuantLib analytical functions. It replaces the previous yfinance-based approach with direct NSE API integration.

## Architecture

### Three-Layer Design

```
┌─────────────────────────────────────────┐
│  NSEOptionChainDataCollector (Main)     │  Orchestrates collection & saves CSV
├─────────────────────────────────────────┤
│  NSESessionManager                      │  Manages API sessions & retries
│  NSEOptionChainParser                   │  Parses JSON & enriches QuantLib data
├─────────────────────────────────────────┤
│  NSE API                                │  https://www.nseindia.com/api/*
└─────────────────────────────────────────┘
```

## Quick Start

### Installation

```bash
pip install -r requirements.txt
```

**Required packages:**
- `requests>=2.28.0` - NSE API communication
- `pandas>=1.3.0` - DataFrame operations
- `numpy>=1.21.0` - Numerical computations
- `urllib3>=1.26.0` - HTTP connection pooling

### Basic Usage

```python
from data_collector import NSEOptionChainDataCollector

# Initialize collector
collector = NSEOptionChainDataCollector(
    stable_symbol="TCS",           # Tata Consultancy Services
    volatile_symbol="ICICIBANK",   # ICICI Bank
    output_dir="./data",
    risk_free_rate=0.07,           # 7% (India RBI rate)
    dividend_yield=0.02,           # 2% expected dividend
    timeout=10                      # API timeout seconds
)

# Fetch and save data
stable_path, volatile_path = collector.collect_and_save()

# Print comprehensive summary
collector.get_summary_statistics()
```

### Running from Command Line

```bash
cd /home/vaibhav/ONITRO/reference/Quantlib/Module-3
python3 data_collector.py
```

## Output Format

### CSV Structure

Each generated CSV contains the following columns for QuantLib consumption:

#### Core Pricing Data
| Column | Type | Description | QuantLib Use |
|--------|------|-------------|--------------|
| `underlying_symbol` | str | Stock symbol (e.g., TCS) | Reference |
| `spot_price` | float | Current underlying price | Forward/Futures S |
| `strike_price` | float | Option strike price | Option intrinsic value |
| `option_type` | str | CALL or PUT | Greeks calculation |
| `ltp` | float | Last Traded Price (market price) | Option valuation |

#### Time & Rates
| Column | Type | Description | QuantLib Use |
|--------|------|-------------|--------------|
| `expiry_date` | datetime | Expiry date object | Date calculations |
| `time_to_maturity` | float | Years to expiry (Actual365Fixed) | Discount factor: e^(-rT) |
| `risk_free_rate` | float | Risk-free rate (decimal) | Forward: S*e^((r-q)T) |
| `dividend_yield` | float | Dividend yield (decimal) | Forward pricing q term |
| `evaluation_date` | datetime | Data collection date | Valuation reference date |

#### Market Data
| Column | Type | Description | QuantLib Use |
|--------|------|-------------|--------------|
| `bid` | float | Bid price | Option valuation bounds |
| `ask` | float | Ask price | Option valuation bounds |
| `mid_price` | float | (Bid + Ask) / 2 | Fair value estimate |
| `volume` | int | Trading volume | Liquidity indicator |
| `open_interest` | int | Open interest | Liquidity indicator |
| `iv` | float | Implied volatility (decimal) | Black-Scholes sigma |

#### Greeks (Risk Sensitivities)
| Column | Type | Description | QuantLib Use |
|--------|------|-------------|--------------|
| `delta` | float | Price sensitivity to underlying | Hedge ratio |
| `gamma` | float | Delta sensitivity | Convexity risk |
| `theta` | float | Time decay (per day) | Theta P&L |
| `vega` | float | IV sensitivity | Vol risk exposure |

#### Metadata
| Column | Type | Description |
|--------|------|-------------|
| `expiry_date_str` | str | Original NSE date string |
| `collected_at` | str | ISO timestamp of collection |

### Example Row

```csv
underlying_symbol,spot_price,strike_price,expiry_date_str,option_type,ltp,bid,ask,volume,open_interest,iv,delta,gamma,theta,vega,collected_at,time_to_maturity,risk_free_rate,dividend_yield,evaluation_date,mid_price
TCS,3500.50,3400.00,25-JAN-2024,CALL,156.25,154.50,158.00,1250,45000,0.25,-0.45,0.0012,-0.08,250.00,2024-01-15T14:30:00,0.0822,0.07,0.02,2024-01-15,156.25
```

## Integration with QuantLib Calculators

### 1. Forward Pricing

```python
import pandas as pd
from data_collector import NSEOptionChainDataCollector

# Collect data
collector = NSEOptionChainDataCollector()
stable_path, _ = collector.collect_and_save()

# Load data
df = pd.read_csv(stable_path)

# Use with forward calculator
for idx, row in df.head(10).iterrows():
    spot = row['spot_price']
    T = row['time_to_maturity']
    r = row['risk_free_rate']
    q = row['dividend_yield']
    
    forward_price = spot * np.exp((r - q) * T)
    print(f"Forward Price: {forward_price:.2f}")
```

### 2. Options Valuation

```python
# Use IV and Greeks for Black-Scholes valuation
for idx, row in df.iterrows():
    if row['option_type'] == 'CALL':
        sigma = row['iv']  # Implied volatility
        delta = row['delta']
        gamma = row['gamma']
        theta = row['theta']
        vega = row['vega']
        
        # These can feed into option pricing models
        # or for Greeks-based P&L prediction
```

### 3. Duration & Convexity (Bond-like)

```python
# For instruments with time-to-maturity and yield
for idx, row in df.iterrows():
    T = row['time_to_maturity']
    r = row['risk_free_rate']
    
    # Calculate modified duration
    macaulay_duration = T  # Simplified
    modified_duration = macaulay_duration / (1 + r)
```

## Advanced Usage

### Custom Risk Parameters

```python
collector = NSEOptionChainDataCollector(
    stable_symbol="SBIN",
    volatile_symbol="RELIANCE",
    output_dir="/custom/data/path",
    risk_free_rate=0.065,      # 6.5% SOFR-equivalent
    dividend_yield=0.015,      # 1.5% custom dividend estimate
    timeout=15                  # Longer timeout for slow connections
)
```

### Multiple Collections with Different Parameters

```python
# Conservative scenario
conservative = NSEOptionChainDataCollector(
    risk_free_rate=0.055,
    dividend_yield=0.010
)

# Base case
base = NSEOptionChainDataCollector(
    risk_free_rate=0.070,
    dividend_yield=0.020
)

# Aggressive scenario
aggressive = NSEOptionChainDataCollector(
    risk_free_rate=0.085,
    dividend_yield=0.025
)

# Collect all scenarios
for scenario, collector in [("conservative", conservative), 
                            ("base", base), 
                            ("aggressive", aggressive)]:
    _, volatile_path = collector.collect_and_save()
    df = pd.read_csv(volatile_path)
    print(f"\n{scenario.upper()} SCENARIO")
    print(df.describe())
```

### Filtering & Processing

```python
import pandas as pd

# Load collected data
df = pd.read_csv("./data/options_tcs.csv")

# Filter for specific expiry
expiry = "25-JAN-2024"
df_expiry = df[df['expiry_date_str'] == expiry]

# Filter for ATM (At-The-Money) options
spot = df['spot_price'].iloc[0]
df_atm = df[(df['strike_price'] > spot * 0.95) & 
            (df['strike_price'] < spot * 1.05)]

# Filter for liquid contracts
df_liquid = df[df['open_interest'] > 1000]

# Time decay analysis (theta decay per day)
df_theta_daily = df.copy()
df_theta_daily['theta_daily'] = df_theta_daily['theta']  # Already daily
df_theta_daily_sorted = df_theta_daily.sort_values('theta_daily', ascending=False)
```

## Error Handling

### Common Issues

#### 1. Connection Timeout
```
[ERROR] Timeout fetching option chain for TCS
```
**Solution:** Increase timeout parameter
```python
collector = NSEOptionChainDataCollector(timeout=20)
```

#### 2. No Data Returned
```
[NSE] No data returned for TCS
```
**Solution:** 
- Check if market is open (9:15 AM - 3:30 PM IST)
- Verify symbol spelling (use NSE symbols without .NS suffix)
- Check NSE website directly: https://www.nseindia.com

#### 3. API Rate Limiting
**Solution:** Add delay between collections
```python
import time

for symbol in ['TCS', 'ICICIBANK', 'SBIN']:
    collector = NSEOptionChainDataCollector(
        stable_symbol=symbol,
        volatile_symbol="RELIANCE"
    )
    collector.collect_and_save()
    time.sleep(5)  # 5-second delay between collections
```

## NSE API Endpoints

### Supported Endpoints

1. **Option Chain Data**
   - URL: `https://www.nseindia.com/api/optionchain`
   - Parameters: `mode=STRIKEPRICE&symbol=SYMBOL`
   - Returns: JSON with CALL/PUT Greeks, volumes, open interest

2. **Spot Price (Derivative Quote)**
   - URL: `https://www.nseindia.com/api/quote-derivative`
   - Parameters: `symbol=SYMBOL`
   - Returns: JSON with current spot price and Greeks

### NSE Symbol Convention

Use **NSE symbols without `.NS` suffix**:
- ✓ TCS (correct)
- ✗ TCS.NS (incorrect - yfinance convention)

**Common NSE Symbols:**
- `TCS` - Tata Consultancy Services
- `ICICIBANK` - ICICI Bank
- `SBIN` - State Bank of India
- `INFY` - Infosys
- `RELIANCE` - Reliance Industries
- `HDFC` - HDFC Bank

## Production Considerations

### 1. API Rate Limiting
NSE API has implicit rate limits. Recommended practice:
- Maximum 1 request per symbol per 5 seconds
- Maximum 10 symbols per minute

### 2. Market Hours
- Trading hours: 9:15 AM - 3:30 PM IST
- Holiday calendar: Check NSE website for declared holidays

### 3. Data Freshness
- Implement caching to avoid repeated API calls
- Refresh data during market hours only
- Store historical data for backtesting

### 4. Time Zone Handling
```python
from datetime import datetime
import pytz

# All times in IST (IST = UTC+5:30)
ist = pytz.timezone('Asia/Kolkata')
now_ist = datetime.now(ist)
```

### 5. Backup & Fallback
```python
import os
import shutil
from datetime import datetime

# Backup previous collection
if os.path.exists('./data/options_tcs.csv'):
    backup_name = f"options_tcs_backup_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
    shutil.copy('./data/options_tcs.csv', f'./backups/{backup_name}')

# Collect new data
collector = NSEOptionChainDataCollector()
collector.collect_and_save()
```

## Performance Optimization

### Batch Collections

```python
import concurrent.futures

symbols = [('TCS', 'ICICIBANK'), ('SBIN', 'HDFC'), ('INFY', 'RELIANCE')]

def collect_symbol_pair(stable, volatile):
    collector = NSEOptionChainDataCollector(
        stable_symbol=stable,
        volatile_symbol=volatile
    )
    return collector.collect_and_save()

# Parallel collection (use cautiously to avoid rate limits)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
    futures = [executor.submit(collect_symbol_pair, s, v) for s, v in symbols]
    results = [f.result() for f in concurrent.futures.as_completed(futures)]
```

## Output Examples

### Summary Statistics Output

```
======================================================================
NSE OPTION CHAIN COLLECTION SUMMARY
======================================================================
Timestamp: 2024-01-15T14:30:45
Risk-Free Rate: 7.00%
Dividend Yield: 2.00%

STABLE UNDERLYING (LOW VOLATILITY)
======================================================================

Symbol: TCS
Spot Price: $3500.50
Total Contracts: 85
Expiry Date(s): 25-JAN-2024
CALL Contracts: 43
PUT Contracts: 42
Strike Price Range: $3000.00 - $4000.00
  CALL LTP: Avg $150.25, Max $456.75
  PUT LTP: Avg $145.30, Max $401.25
Time-to-Maturity Range: 0.0822 - 0.0822 years
IV Range: 18.50% - 35.20%

Sample Records:
 underlying_symbol  strike_price option_type    ltp   bid   ask    iv  time_to_maturity  volume  open_interest
              TCS        3400.00        CALL 156.25 154.5 158.0 0.250         0.0822 1250   45000
              TCS        3400.00         PUT  43.50  42.0  45.0 0.245         0.0822  850   38000
              TCS        3500.00        CALL  95.75  94.0  97.5 0.260         0.0822 2100   62000
```

## Troubleshooting

### Debug Mode

To enable verbose logging:

```python
import logging

logging.basicConfig(level=logging.DEBUG)

collector = NSEOptionChainDataCollector()
collector.collect_and_save()
```

### Check Network Connectivity

```bash
# Verify NSE API accessibility
curl -v https://www.nseindia.com/api/optionchain?mode=STRIKEPRICE&symbol=TCS

# Check DNS resolution
nslookup www.nseindia.com
```

### Validate CSV Output

```python
import pandas as pd

df = pd.read_csv('./data/options_tcs.csv')

# Check data integrity
print(f"Shape: {df.shape}")
print(f"Columns: {df.columns.tolist()}")
print(f"Data types:\n{df.dtypes}")
print(f"Missing values:\n{df.isnull().sum()}")
print(f"\nFirst row:\n{df.iloc[0]}")
```

## References

- [NSE Official Website](https://www.nseindia.com)
- [QuantLib Documentation](https://www.quantlib.org)
- [Option Greeks Guide](https://en.wikipedia.org/wiki/Greeks_(finance))
- [Black-Scholes Model](https://en.wikipedia.org/wiki/Black%E2%80%93Scholes_model)

## Support

For issues or enhancements:
1. Check NSE API status: https://www.nseindia.com
2. Verify market hours (9:15 AM - 3:30 PM IST)
3. Review error logs and messages
4. Check network connectivity and firewall rules
