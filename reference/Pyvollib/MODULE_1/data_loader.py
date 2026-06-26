# ONITRO - Module 1 Data Loader
# Loads and cleans NSE equity CSV files

import pandas as pd


def load_nse_csv(filepath: str) -> pd.DataFrame:
   
    df = pd.read_csv(filepath)

    # Normalize column names — strip whitespace and uppercase
    df.columns = df.columns.str.strip().str.upper()

    # Drop LAST PRICE to avoid duplicate Close column
    if "LAST PRICE" in df.columns:
        df.drop(columns=["LAST PRICE"], inplace=True)

    # Rename NSE column names to standard names
    rename_map = {
        "DATE":                  "Date",
        "OPEN PRICE":            "Open",
        "HIGH PRICE":            "High",
        "LOW PRICE":             "Low",
        "CLOSE PRICE":           "Close",
        "TOTAL TRADED QUANTITY": "Volume",
        "TTL TRD QNTY":          "Volume",
        "TOTTRDQTY":             "Volume",
    }
    df.rename(columns=rename_map, inplace=True)

    # Parse and sort by date
    df["Date"] = pd.to_datetime(df["Date"], dayfirst=True)
    df.sort_values("Date", inplace=True)
    df.reset_index(drop=True, inplace=True)

    # Keep only needed columns that exist
    keep = [c for c in ["Date", "Open", "High", "Low", "Close", "Volume"] if c in df.columns]
    df = df[keep].copy()

    # Clean numeric columns — remove commas
    for col in ["Open", "High", "Low", "Close", "Volume"]:
        if col in df.columns:
            df[col] = pd.to_numeric(
                df[col].apply(lambda x: str(x).replace(",", "").strip()),
                errors="coerce"
            )

    df.dropna(subset=["Close"], inplace=True)
    df.reset_index(drop=True, inplace=True)

    return df
