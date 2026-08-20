# ============================================================
# ONITRO - Module 1 Pricing
# Computes BS price + Greeks for each strike in option chain
# Compares with actual market LTP
# ============================================================

import pandas as pd
from vollib.black_scholes import black_scholes as bs
from vollib.black_scholes.greeks.analytical import (
    delta, gamma, theta, vega, rho
)
from config import RISK_FREE_RATE, SNAPSHOT_DATE, EXPIRY_DATE


def compute_options(df: pd.DataFrame, spot_price: float, r: float = RISK_FREE_RATE) -> pd.DataFrame:
    """
    For each strike in the option chain DataFrame:
    - Computes BS Call and Put price + Greeks
    - Compares with actual market LTP
    - Uses market IV where available, else skips

    Returns a DataFrame with BS prices, Greeks, and market LTP side by side.
    """
    results = []

    # Compute T from snapshot date to expiry date
    snap = pd.Timestamp(SNAPSHOT_DATE)
    expiry = pd.Timestamp(EXPIRY_DATE)
    T = (expiry - snap).days / 365.0

    for _, row in df.iterrows():
        K = row["STRIKE"]

        # Skip illiquid strikes with no LTP on either side
        if pd.isna(row.get("C_LTP")) and pd.isna(row.get("P_LTP")):
            continue

        for flag, option_type, ltp_col, iv_col, oi_col, vol_col in [
            ("c", "Call", "C_LTP", "C_IV", "C_OI", "C_VOLUME"),
            ("p", "Put",  "P_LTP", "P_IV", "P_OI", "P_VOLUME"),
        ]:
            market_ltp = row.get(ltp_col)
            market_iv  = row.get(iv_col)
            market_oi  = row.get(oi_col)
            market_vol = row.get(vol_col)

            # Use market IV if available (convert % to decimal)
            if pd.notna(market_iv) and market_iv > 0:
                sigma = market_iv / 100.0
            else:
                continue   # skip if no IV available

            try:
                price = bs(flag, spot_price, K, T, r, sigma)
                d     = delta(flag, spot_price, K, T, r, sigma)
                g     = gamma(flag, spot_price, K, T, r, sigma)
                th    = theta(flag, spot_price, K, T, r, sigma)
                v     = vega(flag, spot_price, K, T, r, sigma)
                rh    = rho(flag, spot_price, K, T, r, sigma)
            except Exception:
                price = d = g = th = v = rh = None

            # Price difference: BS vs Market
            price_diff = round(price - market_ltp, 4) if (
                price is not None and pd.notna(market_ltp)
            ) else None

            results.append({
                "Snapshot_Date":  SNAPSHOT_DATE,
                "Expiry_Date":    EXPIRY_DATE,
                "Spot_Price":     spot_price,
                "Strike":         K,
                "Option_Type":    option_type,
                "T_Years":        round(T, 6),
                "Market_IV":      market_iv,
                "Sigma_Used":     round(sigma, 6),
                "Market_LTP":     market_ltp,
                "BS_Price":       round(price, 4) if price is not None else None,
                "Price_Diff":     price_diff,
                "Delta":          round(d,  6) if d  is not None else None,
                "Gamma":          round(g,  6) if g  is not None else None,
                "Theta":          round(th, 6) if th is not None else None,
                "Vega":           round(v,  6) if v  is not None else None,
                "Rho":            round(rh, 6) if rh is not None else None,
                "Market_OI":      market_oi,
                "Market_Volume":  market_vol,
            })

    return pd.DataFrame(results)
