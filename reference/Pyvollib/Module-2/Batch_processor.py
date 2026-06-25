import pandas as pd
import numpy as np
import os
from vollib.black_scholes.implied_volatility import implied_volatility
from vollib.black_scholes.greeks.analytical import delta, gamma, theta, vega

def calculate_greeks(row):
    S = float(row['spot_price'])
    K = float(row['strike_price'])
    t = float(row['time_to_maturity'])
    r = float(row['risk_free_rate'])
    price = float(row['ltp'])
    
    flag = 'c' if str(row['option_type']).strip().upper() == 'CALL' else 'p'
    
    if t <= 0.0001 or price <= 0.0:
        return pd.Series([np.nan, np.nan, np.nan, np.nan, np.nan])
        
    try:
        iv = implied_volatility(price, S, K, t, r, flag)
        
        d = delta(flag, S, K, t, r, iv)
        g = gamma(flag, S, K, t, r, iv)
        th = theta(flag, S, K, t, r, iv)
        v = vega(flag, S, K, t, r, iv)
        
        return pd.Series([iv, d, g, th, v])
        
    except Exception:
        return pd.Series([np.nan, np.nan, np.nan, np.nan, np.nan])

def process_benchmark_file(input_path, output_path):
    if not os.path.exists(input_path):
        print(f"Skipping: {input_path} not found.")
        return

    print(f"Processing {input_path}...")
    df = pd.read_csv(input_path)
    initial_rows = len(df)
    
    columns = ['calc_iv', 'calc_delta', 'calc_gamma', 'calc_theta', 'calc_vega']
    df[columns] = df.apply(calculate_greeks, axis=1)
    
    failed_calcs = df['calc_iv'].isna().sum()
    
    df.to_csv(output_path, index=False)
    print(f"Success. Processed {initial_rows - failed_calcs} valid contracts. Dropped {failed_calcs} due to math bounds.\n")

if __name__ == "__main__":
    process_benchmark_file('data/options_tcs.csv', 'data/pyvollib_results_tcs.csv')
    process_benchmark_file('data/options_icicibank.csv', 'data/pyvollib_results_icicibank.csv')
