# ============================================================
# ONITRO - Module 1 Configuration
# Edit this file to change paths, strikes, and parameters
# ============================================================

# Input option chain CSV file paths (downloaded from NSE)
CSV_FILES = {
    "TCS":   r"C:\Users\Admin\OneDrive\Summer2026_Project\ONITRO\ONITRO\reference\Pyvollib\MODULE_1\option-chain-ED-TCS-30-Jun-2026.csv",
    "ICICI": r"C:\Users\Admin\OneDrive\Summer2026_Project\ONITRO\ONITRO\reference\Pyvollib\MODULE_1\option-chain-ED-ICICIBANK-30-Jun-2026.csv",
}

# Output directory — where result CSVs will be saved
OUTPUT_DIR = r"C:\Users\Admin\OneDrive\Summer2026_Project\ONITRO\ONITRO\reference\Pyvollib\MODULE_1"

# Snapshot date (date the option chain was downloaded)
SNAPSHOT_DATE = "2026-06-30"

# Expiry date from NSE option chain page
EXPIRY_DATE = "2026-07-31"   # last Thursday of July 2026

# Black-Scholes parameters
RISK_FREE_RATE = 0.065        # ~6.5% India risk-free rate
