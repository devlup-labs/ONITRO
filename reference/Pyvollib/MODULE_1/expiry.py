# ONITRO - Module 1 Expiry Date Calculator
# NSE options expire on the last Thursday of every month

import pandas as pd
from datetime import date, timedelta


def last_thursday_of_month(year: int, month: int) -> date:

    # Start from last day of month
    if month == 12:
        last_day = date(year + 1, 1, 1) - timedelta(days=1)
    else:
        last_day = date(year, month + 1, 1) - timedelta(days=1)

    # Walk back to Thursday (weekday 3)
    days_back = (last_day.weekday() - 3) % 7
    return last_day - timedelta(days=days_back)


def assign_expiry_dates(df: pd.DataFrame) -> pd.DataFrame:
 
    df = df.copy()

    expiry_dates = []
    t_years = []

    for _, row in df.iterrows():
        d = row["Date"].date()

        # Get this month's expiry
        expiry = last_thursday_of_month(d.year, d.month)

        # If today is on or past expiry, use next month's expiry
        if d >= expiry:
            if d.month == 12:
                expiry = last_thursday_of_month(d.year + 1, 1)
            else:
                expiry = last_thursday_of_month(d.year, d.month + 1)

        expiry_dates.append(pd.Timestamp(expiry))

        # T = trading days remaining / 252
        # Using calendar days / 365 is also fine for BSM
        t = (expiry - d).days / 365.0
        t_years.append(round(t, 6))

    df["Expiry_Date"] = expiry_dates
    df["T_Years"]     = t_years

    return df
