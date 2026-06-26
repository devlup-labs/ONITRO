# ONITRO - Module 1 Pipeline
# Orchestrates: load → volatility → expiry → pricing → save

import os
from config import CSV_FILES, STRIKES, OUTPUT_DIR
from data_loader import load_nse_csv
from volatility import add_historical_volatility
from expiry import assign_expiry_dates
from pricing import compute_options


def run_pipeline():
   
    for stock_name, csv_path in CSV_FILES.items():
        print(f"\n{'='*50}")
        print(f"Processing: {stock_name}")
        print(f"{'='*50}")

        # Step 1 — Load
        df = load_nse_csv(csv_path)
        print(f"  Loaded       : {len(df)} rows")

        # Step 2 — Historical Volatility
        df = add_historical_volatility(df)
        print(f"  After HV     : {len(df)} rows")

        # Step 3 — Assign expiry dates (last Thursday of month)
        df = assign_expiry_dates(df)
        print(f"  Expiry dates assigned")

        # Step 4 — Option Pricing + Greeks
        strikes = STRIKES[stock_name]
        results_df = compute_options(df, strikes=strikes)
        print(f"  Option rows  : {len(results_df)}")

        # Step 5 — Save output CSV
        output_path = os.path.join(OUTPUT_DIR, f"onitro_{stock_name.lower()}_options.csv")
        results_df.to_csv(output_path, index=False)
        print(f"  Saved to     : {output_path}")

        # Preview
        print(f"\n  Sample output:")
        print(results_df.head(8).to_string(index=False))
