import pandas as pd
from datetime import datetime
import os

def clean_num(x):
    if isinstance(x, str):
        x = x.replace(',', '').replace('-', '0').strip()
    try:
        return float(x)
    except ValueError:
        return 0.0

def process_nse_file(input_file, output_file, spot_price, expiry_date_str):
    print(f"Cleaning {input_file}...")

    df_raw = pd.read_csv(input_file, skiprows=1)
    
    eval_date = datetime(2026, 6, 25)
    expiry_date = datetime.strptime(expiry_date_str, "%d-%b-%Y")
    
    t = max((expiry_date - eval_date).days / 365.0, 0.0001)
    r = 0.07  
    
    rows = []
    
    for _, row in df_raw.iterrows():
        strike = clean_num(row.iloc[11])
        if strike == 0:
            continue
            
        call_ltp = clean_num(row.iloc[5])
        put_ltp = clean_num(row.iloc[17])
        
        if call_ltp > 0:
            rows.append([spot_price, strike, call_ltp, t, r, 'CALL'])
        if put_ltp > 0:
            rows.append([spot_price, strike, put_ltp, t, r, 'PUT'])
            
    df_clean = pd.DataFrame(rows, columns=[
        'spot_price', 'strike_price', 'ltp', 'time_to_maturity', 'risk_free_rate', 'option_type'
    ])
    
    df_clean.to_csv(output_file, index=False)
    print(f"Success. Cleaned {len(df_clean)} contracts into {output_file}")

if __name__ == "__main__":
    os.makedirs('data', exist_ok=True)


    process_nse_file(
        input_file='option-chain-ED-TCS-28-Jul-2026.csv',
        output_file='data/options_tcs.csv',
        spot_price=3900.0,
        expiry_date_str='28-Jul-2026'
    )
   
    process_nse_file(
        input_file='option-chain-ED-ICICIBANK-28-Jul-2026.csv',
        output_file='data/options_icicibank.csv',
        spot_price=1200.0,
        expiry_date_str='28-Jul-2026'
)
