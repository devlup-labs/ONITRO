# ============================================================
# ONITRO - Module 1 Configuration
# Edit this file to change paths, strikes, and parameters
# ============================================================

# Input CSV file paths
CSV_FILES = {
    "TCS":   r"C:\Users\Admin\OneDrive\Summer2026_Project\ONITRO\ONITRO\reference\Pyvollib\MODULE_1\25-06-2025-TO-25-06-2026-TCS-EQ-N.csv",
    "ICICI": r"C:\Users\Admin\OneDrive\Summer2026_Project\ONITRO\ONITRO\reference\Pyvollib\MODULE_1\25-06-2025-TO-25-06-2026-ICICIBANK-EQ-N.csv",
}

# Strike prices per stock
STRIKES = {
    "TCS":   [2900, 2950, 3000, 3050, 3100],  # TCS around ₹3032
    "ICICI": [1350, 1400, 1450, 1500, 1550],  # ICICI around ₹1443
}
# Output directory — where CSVs will be saved
OUTPUT_DIR = r"C:\Users\Admin\OneDrive\Summer2026_Project\ONITRO\ONITRO\reference\Pyvollib\MODULE_1"

# Black-Scholes parameters
RISK_FREE_RATE = 0.065   # ~6.5% India risk-free rate
VOL_WINDOW     = 30      # rolling window for historical volatility
