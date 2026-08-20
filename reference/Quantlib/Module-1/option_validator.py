import os
import sys
import pandas as pd
import QuantLib as ql

def clean_float(val):
    """Parses raw Indian NSE specific formatting strings (commas, hyphens) safely."""
    if pd.isna(val):
        return 0.0
    val_str = str(val).strip().replace(',', '')
    if val_str == '-' or val_str == '' or val_str == 'nan':
        return 0.0
    try:
        return float(val_str)
    except ValueError:
        return 0.0

def calculate_greeks(spot, strike, iv, r, q, days_to_expiry, option_type):
    """Executes structural Black-Scholes evaluation on a standalone row element."""
    
    # 1. Hard stop only if the strike price itself is corrupted
    if strike <= 0:
        return 0.0, 0.0, 0.0, 0.0, 0.0
        
    # 2. SAFETY PATCH FOR IV: If IV is missing or 0, fall back to market average (e.g., 24%)
    # Adjust 0.24 if your pipeline expects percentages (like 24.0) instead of decimals
    if iv <= 0.01 or pd.isna(iv):
        iv = 0.24  
        
    # 3. SAFETY PATCH FOR SPOT: If spot is missing or 0, fall back to benchmark
    if spot <= 0 or pd.isna(spot):
        spot = 2515.0
        
    


    try:
        # Match time series anchor exactly: June 26, 2026
        today = ql.Date(26, 6, 2026)
        ql.Settings.instance().evaluationDate = today

        day_counter = ql.Actual365Fixed()
        calendar = ql.NullCalendar()
        expiry_date = today + int(days_to_expiry)

        spot_handle = ql.QuoteHandle(ql.SimpleQuote(spot))
        rate_handle = ql.YieldTermStructureHandle(ql.FlatForward(today, r, day_counter))
        div_handle = ql.YieldTermStructureHandle(ql.FlatForward(today, q, day_counter))
        vol_handle = ql.BlackVolTermStructureHandle(ql.BlackConstantVol(today, calendar, iv, day_counter))

        process = ql.BlackScholesMertonProcess(spot_handle, div_handle, rate_handle, vol_handle)
        engine = ql.AnalyticEuropeanEngine(process)

        opt_type = ql.Option.Call if option_type.upper() == 'CALL' else ql.Option.Put
        payoff = ql.PlainVanillaPayoff(opt_type, strike)
        exercise = ql.EuropeanExercise(expiry_date)

        option = ql.VanillaOption(payoff, exercise)
        option.setPricingEngine(engine)

        # Output standard standardizations
        return (option.NPV(), option.delta(), option.gamma(), option.vega() / 100, option.theta() / 365)
    except Exception:
        return 0.0, 0.0, 0.0, 0.0, 0.0

def process_nse_file(input_file, output_file):
    if not os.path.exists(input_file):
        print(f"[ERROR] Target file '{input_file}' not found in current directory.")
        sys.exit(1)

    # Load data skipping raw first metadata row ('CALLS,,PUTS')
    raw_df = pd.read_csv(input_file, header=1)
    raw_df.columns = [str(col).strip() for col in raw_df.columns]
    cols = raw_df.columns.tolist()
    
    # Map strict index column alignments matching NSE template structure
    strike_col = cols[11] 
    call_iv_col = cols[4]
    call_ltp_col = cols[5]
    put_ltp_col = cols[17]
    put_iv_col = cols[18]

    # Baseline environment parameters for TCS data context
    spot_price = 2515.0       # Near ATM baseline anchor
    risk_free_rate = 0.07     # 7% interest rate baseline
    dividend_yield = 0.01     # 1% dividend yield baseline
    days_to_expiry = 32       # Temporal difference: June 26, 2026 to July 28, 2026

    processed_records = []
    print("[Module-1] Ingesting and executing Greeks extraction pipeline...")

    for _, row in raw_df.iterrows():
        strike = clean_float(row[strike_col])
        if strike == 0:
            continue

        # Process Calls metrics
        c_iv = clean_float(row[call_iv_col]) / 100.0  
        c_ltp = clean_float(row[call_ltp_col])
        c_price, c_delta, c_gamma, c_vega, c_theta = calculate_greeks(
            spot_price, strike, c_iv, risk_free_rate, dividend_yield, days_to_expiry, 'CALL'
        )

        # Process Puts metrics
        p_iv = clean_float(row[put_iv_col]) / 100.0   
        p_ltp = clean_float(row[put_ltp_col])
        p_price, p_delta, p_gamma, p_vega, p_theta = calculate_greeks(
            spot_price, strike, p_iv, risk_free_rate, dividend_yield, days_to_expiry, 'PUT'
        )

        processed_records.append({
            'Strike': strike,
            'Call_Market_LTP': c_ltp,
            'Call_QL_Price': c_price,
            'Call_Delta': c_delta,
            'Call_Gamma': c_gamma,
            'Call_Vega': c_vega,
            'Call_Theta_PerDay': c_theta,
            'Put_Market_LTP': p_ltp,
            'Put_QL_Price': p_price,
            'Put_Delta': p_delta,
            'Put_Gamma': p_gamma,
            'Put_Vega': p_vega,
            'Put_Theta_PerDay': p_theta
        })

    output_df = pd.DataFrame(processed_records)
    output_df.to_csv(output_file, index=False)
    print(f"[SUCCESS] Validation matrix generated at: '{output_file}'")

if __name__ == "__main__":
    input_path = "./option-chain-ED-TCS-28-Jul-2026.csv"
    output_path = "./tcs_greeks_output.csv"
    process_nse_file(input_path, output_path)