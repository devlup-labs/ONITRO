"""Historical CSV loader for Module-3 offline benchmark workflows.

This module provides safe CSV ingestion for historical NSE option chain snapshots.
It validates required headers, skips malformed rows, normalizes column types, and
returns a Pandas DataFrame that can be consumed by benchmark calculations.
"""

import csv
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

import pandas as pd

REQUIRED_COLUMNS = [
    "underlying_symbol",
    "spot_price",
    "strike_price",
    "expiry_date_str",
    "option_type",
    "ltp",
    "bid",
    "ask",
    "volume",
    "open_interest",
    "iv",
    "risk_free_rate",
    "dividend_yield",
    "evaluation_date",
    "time_to_maturity",
    "collected_at",
]

DATE_FORMATS = [
    "%Y-%m-%d",
    "%Y-%m-%d %H:%M:%S",
    "%Y-%m-%d %H:%M:%S.%f",
    "%Y-%m-%dT%H:%M:%S",
    "%Y-%m-%dT%H:%M:%S.%f",
    "%d-%b-%Y",
    "%d%b%Y",
    "%d/%m/%Y",
    "%d-%m-%Y",
]


def _parse_float(value: str) -> Optional[float]:
    if value is None:
        return None
    value = value.strip()
    if value == "" or value.lower() == "nan":
        return None
    try:
        return float(value)
    except ValueError:
        return None


def _parse_int(value: str) -> Optional[int]:
    if value is None:
        return None
    value = value.strip()
    if value == "" or value.lower() == "nan":
        return None
    try:
        return int(float(value))
    except ValueError:
        return None


def _parse_datetime(value: str) -> Optional[datetime]:
    if value is None:
        return None
    value = value.strip()
    if value == "" or value.lower() == "nan":
        return None

    for fmt in DATE_FORMATS:
        try:
            return datetime.strptime(value, fmt)
        except ValueError:
            continue
    return None


def _normalize_header(header: Iterable[str]) -> List[str]:
    return [col.strip() for col in header]


def _validate_columns(header: List[str], required: List[str]) -> None:
    missing = [col for col in required if col not in header]
    if missing:
        raise ValueError(
            f"Missing required columns in historical CSV: {', '.join(missing)}"
        )


def load_historical_csv(
    path: str,
    required_columns: Optional[List[str]] = None,
) -> Tuple[pd.DataFrame, int, int]:
    """Load a historical option chain CSV safely.

    Args:
        path: Path to the CSV file.
        required_columns: Optional list of required column names.

    Returns:
        Tuple containing:
        - DataFrame of valid rows
        - number of loaded rows
        - number of skipped malformed rows
    """
    required = required_columns or REQUIRED_COLUMNS
    csv_path = Path(path)

    if not csv_path.exists():
        raise FileNotFoundError(f"Historical CSV not found: {csv_path}")

    valid_rows: List[Dict[str, object]] = []
    skipped_rows = 0

    with csv_path.open(newline="", encoding="utf-8") as fp:
        reader = csv.DictReader(fp)
        header = _normalize_header(reader.fieldnames or [])
        _validate_columns(header, required)

        row_number = 1
        for raw_row in reader:
            row_number += 1
            row = {key.strip(): (value or "").strip() for key, value in raw_row.items()}
            if not row:
                skipped_rows += 1
                print(f"[WARN] Empty row at line {row_number}; skipping.")
                continue

            missing_values = [col for col in required if row.get(col, "") == ""]
            if missing_values:
                skipped_rows += 1
                print(
                    f"[WARN] Row {row_number} missing required values: "
                    f"{', '.join(missing_values)}. Skipping."
                )
                continue

            expiry_date = _parse_datetime(row.get("expiry_date_str"))
            evaluation_date = _parse_datetime(row.get("evaluation_date"))
            collected_at = _parse_datetime(row.get("collected_at"))

            if expiry_date is None:
                skipped_rows += 1
                print(
                    f"[WARN] Row {row_number} has invalid expiry_date_str '{row.get('expiry_date_str')}'. Skipping."
                )
                continue

            if evaluation_date is None:
                skipped_rows += 1
                print(
                    f"[WARN] Row {row_number} has invalid evaluation_date '{row.get('evaluation_date')}'. Skipping."
                )
                continue

            parsed_row = {
                "underlying_symbol": row.get("underlying_symbol", ""),
                "spot_price": _parse_float(row.get("spot_price")),
                "strike_price": _parse_float(row.get("strike_price")),
                "expiry_date_str": row.get("expiry_date_str", ""),
                "expiry_date": expiry_date,
                "option_type": row.get("option_type", ""),
                "ltp": _parse_float(row.get("ltp")),
                "bid": _parse_float(row.get("bid")),
                "ask": _parse_float(row.get("ask")),
                "volume": _parse_int(row.get("volume")),
                "open_interest": _parse_int(row.get("open_interest")),
                "iv": _parse_float(row.get("iv")),
                "delta": _parse_float(row.get("delta")),
                "gamma": _parse_float(row.get("gamma")),
                "theta": _parse_float(row.get("theta")),
                "vega": _parse_float(row.get("vega")),
                "risk_free_rate": _parse_float(row.get("risk_free_rate")),
                "dividend_yield": _parse_float(row.get("dividend_yield")),
                "evaluation_date": evaluation_date,
                "time_to_maturity": _parse_float(row.get("time_to_maturity")),
                "collected_at": collected_at,
                "mid_price": _parse_float(row.get("mid_price")),
            }

            if parsed_row["spot_price"] is None or parsed_row["strike_price"] is None:
                skipped_rows += 1
                print(
                    f"[WARN] Row {row_number} has invalid numeric values for spot_price/strike_price. Skipping."
                )
                continue

            valid_rows.append(parsed_row)

    df = pd.DataFrame(valid_rows)
    return df, len(valid_rows), skipped_rows
