# ============================================================
# ONITRO - Module 1 Pipeline
# Orchestrates: load → pricing → save
# ============================================================

import os
import time
from config import CSV_FILES, OUTPUT_DIR

from data_loader import load_option_chain_csv
from pricing import compute_options

# Spot prices as shown on NSE option chain page
SPOT_PRICES = {
    "TCS":   2094.00,   # TCS spot as on 30-Jun-2026
    "ICICI": 1443.60,   # ICICI spot as on 30-Jun-2026
}


def run_pipeline():
    for stock_name, csv_path in CSV_FILES.items():
        print(f"\n{'='*50}")
        print(f"Processing: {stock_name}")
        print(f"{'='*50}")

        # Step 1 — Load option chain
        t0 = time.perf_counter()
        df = load_option_chain_csv(csv_path)
        t1 = time.perf_counter()
        print(f"  Loaded          : {len(df)} strikes")
        print(f"  Load time       : {t1-t0:.4f}s")

        # Step 2 — Compute BS prices + Greeks
        spot = SPOT_PRICES[stock_name]
        t2 = time.perf_counter()
        results_df = compute_options(df, spot_price=spot)
        t3 = time.perf_counter()
        print(f"  Option rows     : {len(results_df)}")
        print(f"  Pricing time    : {t3-t2:.4f}s")
        print(f"  Rows/sec        : {len(results_df)/(t3-t2):.0f}")

        # Step 3 — Save output CSV
        output_path = os.path.join(OUTPUT_DIR, f"onitro_{stock_name.lower()}_options.csv")
        results_df.to_csv(output_path, index=False)
        print(f"  Saved to        : {output_path}")

        # Preview
        print(f"\n  Sample output:")
        print(results_df.head(6).to_string(index=False))
