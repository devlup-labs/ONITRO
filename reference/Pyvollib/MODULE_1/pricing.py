# ONITRO - Module 1 Pricing
# Computes Black-Scholes option prices and Greeks using vollib
# T_Years is now taken per-row from assign_expiry_dates()

import pandas as pd
from vollib.black_scholes import black_scholes as bs
from vollib.black_scholes.greeks.analytical import (
    delta, gamma, theta, vega, rho
)
from config import RISK_FREE_RATE


def compute_options(
    df: pd.DataFrame,
    strikes: list,
    r: float = RISK_FREE_RATE,
) -> pd.DataFrame:
    
    results = []

    for _, row in df.iterrows():
        S           = row["Close"]
        sigma       = row["HV"]
        date        = row["Date"]
        T           = row["T_Years"]       # ← now per-row, not constant
        expiry_date = row["Expiry_Date"]

        # Skip rows where T is 0 or negative (expiry day itself)
        if T <= 0:
            continue

        for K in strikes:
            for flag, option_type in [("c", "Call"), ("p", "Put")]:
                try:
                    price = bs(flag, S, K, T, r, sigma)
                    d     = delta(flag, S, K, T, r, sigma)
                    g     = gamma(flag, S, K, T, r, sigma)
                    th    = theta(flag, S, K, T, r, sigma)
                    v     = vega(flag, S, K, T, r, sigma)
                    rh    = rho(flag, S, K, T, r, sigma)
                except Exception:
                    price = d = g = th = v = rh = None

                results.append({
                    "Date":         date,
                    "Expiry_Date":  expiry_date,
                    "Stock_Price":  round(S, 4),
                    "Strike":       K,
                    "Option_Type":  option_type,
                    "T_Years":      T,
                    "HV_Sigma":     round(sigma, 6),
                    "Option_Price": round(price, 4) if price is not None else None,
                    "Delta":        round(d,     6) if d     is not None else None,
                    "Gamma":        round(g,     6) if g     is not None else None,
                    "Theta":        round(th,    6) if th    is not None else None,
                    "Vega":         round(v,     6) if v     is not None else None,
                    "Rho":          round(rh,    6) if rh    is not None else None,
                })

    return pd.DataFrame(results)
