# ONITRO - Module 1 Volatility
# Computes annualized historical volatility (HV)

import numpy as np
import pandas as pd
from config import VOL_WINDOW


def add_historical_volatility(df: pd.DataFrame, window: int = VOL_WINDOW) -> pd.DataFrame:
    
    df = df.copy()

    df["Returns"] = df["Close"].pct_change()
    df["HV"] = df["Returns"].rolling(window).std() * np.sqrt(252)

    df.dropna(subset=["HV"], inplace=True)
    df.reset_index(drop=True, inplace=True)

    return df
