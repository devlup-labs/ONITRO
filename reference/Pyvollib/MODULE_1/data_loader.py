# ============================================================
# ONITRO - Module 1 Data Loader
# Loads and cleans NSE Option Chain CSV files
# ============================================================

import pandas as pd


def load_option_chain_csv(filepath: str) -> pd.DataFrame:
    """
    Loads an NSE Option Chain CSV file and returns a clean DataFrame with:

    STRIKE, 
    C_OI, C_VOLUME, C_IV, C_LTP,       ← Call side
    P_OI, P_VOLUME, P_IV, P_LTP         ← Put side
    """

    # NSE option chain CSV structure:
    # Row 0: "CALLS,,PUTS"             ← skip
    # Row 1: actual column names       ← use as header
    # Row 2+: data rows

    df = pd.read_csv(
        filepath,
        skiprows=1,   # skip "CALLS,,PUTS" row
        names=[
            'C_OI', 'C_CHNG_OI', 'C_VOLUME', 'C_IV', 'C_LTP', 'C_CHNG',
            'C_BID_QTY', 'C_BID', 'C_ASK', 'C_ASK_QTY',
            'STRIKE',
            'P_BID_QTY', 'P_BID', 'P_ASK', 'P_ASK_QTY',
            'P_CHNG', 'P_LTP', 'P_IV', 'P_VOLUME', 'P_CHNG_OI', 'P_OI',
            'EXTRA'
        ]
    )

    # Drop the column header row (first row after skip)
    df = df.iloc[1:].reset_index(drop=True)

    # Drop extra column
    df.drop(columns=["EXTRA"], inplace=True, errors="ignore")

    # Clean STRIKE — remove commas, convert to float
    df["STRIKE"] = pd.to_numeric(
        df["STRIKE"].apply(lambda x: str(x).replace(",", "").strip()),
        errors="coerce"
    )

    # Clean numeric columns — remove commas, convert - to NaN
    numeric_cols = [
        "C_OI", "C_CHNG_OI", "C_VOLUME", "C_IV", "C_LTP",
        "C_BID", "C_ASK",
        "P_OI", "P_CHNG_OI", "P_VOLUME", "P_IV", "P_LTP",
        "P_BID", "P_ASK"
    ]
    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(
                df[col].apply(lambda x: str(x).replace(",", "").strip()
                              if str(x) not in ["-", "nan", ""] else None),
                errors="coerce"
            )

    # Drop rows where STRIKE is NaN
    df.dropna(subset=["STRIKE"], inplace=True)
    df.reset_index(drop=True, inplace=True)

    return df
