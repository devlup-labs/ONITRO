"""CLI for live NSE data collection and offline benchmark processing.

Usage:
    python main.py --mode live
    python main.py --mode live --collect-history
    python main.py --mode benchmark --input data/tcs_history.csv
    python main.py --mode benchmark --input data/icicibank_history.csv --output outputs/icicibank_output.csv
"""

import argparse
import math
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# Ensure local module imports resolve even when the script is run from another directory.
ROOT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT_DIR))

import pandas as pd

from data_collector import NSEOptionChainDataCollector
from csv_loader import load_historical_csv

DEFAULT_OUTPUT_DIR = Path("outputs")
DEFAULT_HISTORY_DIR = Path("data")
DEFAULT_HISTORY_TARGET_ROWS = 15000
DEFAULT_HISTORY_SNAPSHOT_INTERVAL_SECONDS = 30


class BenchmarkCalculator:
    """Benchmark calculator for forward/futures and bond/YTM metrics."""

    @staticmethod
    def forward_price(spot_price: float, time_to_maturity: float, risk_free_rate: float, dividend_yield: float) -> float:
        return spot_price * math.exp((risk_free_rate - dividend_yield) * time_to_maturity)

    @staticmethod
    def futures_price(spot_price: float, time_to_maturity: float, risk_free_rate: float, dividend_yield: float) -> float:
        return BenchmarkCalculator.forward_price(spot_price, time_to_maturity, risk_free_rate, dividend_yield)

    @staticmethod
    def forward_discount_premium(spot_price: float, forward_price: float) -> float:
        if spot_price == 0.0:
            return 0.0
        return ((forward_price - spot_price) / spot_price) * 100.0

    @staticmethod
    def cost_of_carry(spot_price: float, forward_price: float, time_to_maturity: float) -> float:
        if time_to_maturity <= 0.0 or spot_price <= 0.0 or forward_price <= 0.0:
            return 0.0
        return math.log(forward_price / spot_price) / time_to_maturity

    @staticmethod
    def zero_coupon_bond_price(face_value: float, yield_rate: float, years_to_maturity: float) -> float:
        if years_to_maturity <= 0.0:
            return face_value
        return face_value / ((1.0 + yield_rate) ** years_to_maturity)

    @staticmethod
    def coupon_bond_price(
        face_value: float,
        coupon_rate: float,
        yield_rate: float,
        years_to_maturity: float,
        payment_frequency: int = 2,
    ) -> float:
        if years_to_maturity <= 0.0 or payment_frequency <= 0:
            return face_value

        periods = int(round(years_to_maturity * payment_frequency))
        if periods <= 0:
            return face_value

        rate_per_period = yield_rate / payment_frequency
        coupon_payment = face_value * coupon_rate / payment_frequency

        if abs(rate_per_period) < 1e-12:
            return coupon_payment * periods + face_value

        discount_factor = (1.0 - (1.0 + rate_per_period) ** (-periods)) / rate_per_period
        return coupon_payment * discount_factor + face_value * (1.0 + rate_per_period) ** (-periods)

    @staticmethod
    def ytm_coupon_bond(
        face_value: float,
        coupon_rate: float,
        bond_price: float,
        years_to_maturity: float,
        payment_frequency: int = 2,
        initial_guess: float = 0.05,
        tolerance: float = 1e-8,
        max_iterations: int = 100,
    ) -> Optional[float]:
        if bond_price <= 0.0 or years_to_maturity <= 0.0:
            return None

        periods = int(round(years_to_maturity * payment_frequency))
        if periods <= 0:
            return None

        coupon_payment = face_value * coupon_rate / payment_frequency
        ytm = initial_guess

        for _ in range(max_iterations):
            rate_per_period = ytm / payment_frequency
            if rate_per_period <= -0.999999:
                rate_per_period = 1e-6

            if abs(rate_per_period) < 1e-12:
                pv_coupons = coupon_payment * periods
                pv_face = face_value
            else:
                pv_coupons = coupon_payment * (1.0 - (1.0 + rate_per_period) ** (-periods)) / rate_per_period
                pv_face = face_value * (1.0 + rate_per_period) ** (-periods)

            price_estimate = pv_coupons + pv_face
            error = price_estimate - bond_price
            if abs(error) < tolerance:
                return ytm

            if abs(rate_per_period) < 1e-12:
                derivative = -coupon_payment * periods * (periods + 1) / 2.0
            else:
                derivative = (
                    -coupon_payment * (periods * (1.0 + rate_per_period) ** (-periods - 1) - (1.0 - (1.0 + rate_per_period) ** (-periods)) / rate_per_period) / payment_frequency
                    -face_value * periods * (1.0 + rate_per_period) ** (-periods - 1) / payment_frequency
                )

            if derivative == 0.0:
                break

            ytm -= error / derivative

        return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Live NSE collector and offline benchmark runner.")
    parser.add_argument(
        "--mode",
        choices=["live", "benchmark"],
        required=True,
        help="Execution mode: live data collection or offline benchmark.",
    )
    parser.add_argument(
        "--input",
        type=str,
        help="Historical CSV input path for benchmark mode.",
    )
    parser.add_argument(
        "--output",
        type=str,
        help="Output CSV path for benchmark mode. Default: outputs/<symbol>_output.csv",
    )
    parser.add_argument(
        "--benchmark-report",
        type=str,
        default=str(DEFAULT_OUTPUT_DIR / "benchmark_report.txt"),
        help="Benchmark report file path.",
    )
    parser.add_argument(
        "--collect-history",
        action="store_true",
        help="Collect history snapshots instead of only live data when running live mode.",
    )
    parser.add_argument(
        "--target-rows",
        type=int,
        default=DEFAULT_HISTORY_TARGET_ROWS,
        help="Target number of valid rows for historical data collection.",
    )
    parser.add_argument(
        "--snapshot-interval",
        type=int,
        default=DEFAULT_HISTORY_SNAPSHOT_INTERVAL_SECONDS,
        help="Seconds between live history snapshots.",
    )
    parser.add_argument(
        "--stable-symbol",
        type=str,
        default="TCS",
        help="Stable symbol used by live mode.",
    )
    parser.add_argument(
        "--volatile-symbol",
        type=str,
        default="ICICIBANK",
        help="Volatile symbol used by live mode.",
    )
    return parser.parse_args()


def _safe_float(value: Any) -> Optional[float]:
    if value is None:
        return None
    try:
        return float(value)
    except (ValueError, TypeError):
        return None


def process_benchmark_dataset(
    input_path: Path,
    output_path: Path,
    report_lines: List[str],
) -> Tuple[int, int, float, float, float]:
    create_directory(output_path.parent)

    load_start = time.perf_counter()
    df, loaded_rows, skipped_rows = load_historical_csv(str(input_path))
    load_end = time.perf_counter()
    load_time = load_end - load_start

    if df.empty:
        report_lines.append(f"Dataset: {input_path.name}")
        report_lines.append(f"Rows Processed: 0")
        report_lines.append(f"Rows Skipped: {skipped_rows}")
        report_lines.append("CSV Load:")
        report_lines.append(f"{load_time * 1000:.1f} ms")
        report_lines.append("Computation:")
        report_lines.append("0.0 ms")
        report_lines.append("Feature-wise timings:")
        report_lines.append("Forward/Futures pricing: 0.0 ms")
        report_lines.append("Bond pricing: 0.0 ms")
        report_lines.append("YTM solver: 0.0 ms")
        report_lines.append("CSV Write:")
        report_lines.append("0.0 ms")
        report_lines.append("Total:")
        report_lines.append(f"{load_time * 1000:.1f} ms")
        report_lines.append("Average per Row:")
        report_lines.append("0.0 microseconds")
        report_lines.append("Throughput:")
        report_lines.append("0 rows/sec")
        report_lines.append("--------------------------------")
        return 0, skipped_rows, load_time, 0.0, 0.0

    calculator = BenchmarkCalculator()
    computed_rows = []
    compute_start = time.perf_counter()
    forward_futures_time = 0.0
    bond_pricing_time = 0.0
    ytm_solver_time = 0.0
    for idx, row in df.iterrows():
        spot_price = _safe_float(row.get("spot_price"))
        time_to_maturity = _safe_float(row.get("time_to_maturity"))
        risk_free_rate = _safe_float(row.get("risk_free_rate"))
        dividend_yield = _safe_float(row.get("dividend_yield"))
        ltp = _safe_float(row.get("ltp"))

        if spot_price is None or time_to_maturity is None or risk_free_rate is None or dividend_yield is None or ltp is None:
            skipped_rows += 1
            continue

        ff_start = time.perf_counter()
        forward_price = calculator.forward_price(spot_price, time_to_maturity, risk_free_rate, dividend_yield)
        futures_price = calculator.futures_price(spot_price, time_to_maturity, risk_free_rate, dividend_yield)
        discount_premium = calculator.forward_discount_premium(spot_price, forward_price)
        carry = calculator.cost_of_carry(spot_price, forward_price, time_to_maturity)
        forward_futures_time += time.perf_counter() - ff_start

        bond_start = time.perf_counter()
        zero_coupon_price = calculator.zero_coupon_bond_price(1000.0, risk_free_rate, 5.0)
        coupon_bond_price = calculator.coupon_bond_price(1000.0, 0.06, risk_free_rate, 5.0, payment_frequency=2)
        implied_market_price = coupon_bond_price - 1.25
        bond_pricing_time += time.perf_counter() - bond_start

        ytm_start = time.perf_counter()
        ytm = calculator.ytm_coupon_bond(1000.0, 0.06, implied_market_price, 5.0, payment_frequency=2)
        ytm_solver_time += time.perf_counter() - ytm_start

        row_dict = row.to_dict()
        row_dict.update(
            {
                "calculated_forward_price": forward_price,
                "calculated_futures_price": futures_price,
                "forward_discount_premium_pct": discount_premium,
                "cost_of_carry": carry,
                "zero_coupon_bond_price": zero_coupon_price,
                "coupon_bond_price": coupon_bond_price,
                "implied_bond_market_price": implied_market_price,
                "calculated_ytm": ytm if ytm is not None else float("nan"),
            }
        )
        computed_rows.append(row_dict)
    compute_end = time.perf_counter()
    compute_time = compute_end - compute_start

    if computed_rows:
        out_df = pd.DataFrame(computed_rows)
    else:
        out_df = pd.DataFrame(columns=list(df.columns) + [
            "calculated_forward_price",
            "calculated_futures_price",
            "forward_discount_premium_pct",
            "cost_of_carry",
            "zero_coupon_bond_price",
            "coupon_bond_price",
            "implied_bond_market_price",
            "calculated_ytm",
        ])

    write_start = time.perf_counter()
    out_df.to_csv(output_path, index=False)
    write_end = time.perf_counter()
    write_time = write_end - write_start

    total_time = load_time + compute_time + write_time
    processed_rows = len(computed_rows)
    average_per_row = (compute_time / processed_rows * 1_000_000) if processed_rows else 0.0
    throughput = (processed_rows / total_time) if total_time > 0 else 0.0

    report_lines.append(f"Dataset: {input_path.name}")
    report_lines.append(f"Rows Processed: {processed_rows}")
    report_lines.append(f"Rows Skipped: {skipped_rows}")
    report_lines.append("CSV Load:")
    report_lines.append(f"{load_time * 1000:.1f} ms")
    report_lines.append("Computation:")
    report_lines.append(f"{compute_time * 1000:.1f} ms")
    report_lines.append("Feature-wise timings:")
    report_lines.append(f"Forward/Futures pricing: {forward_futures_time * 1000:.1f} ms")
    report_lines.append(f"Bond pricing: {bond_pricing_time * 1000:.1f} ms")
    report_lines.append(f"YTM solver: {ytm_solver_time * 1000:.1f} ms")
    report_lines.append("CSV Write:")
    report_lines.append(f"{write_time * 1000:.1f} ms")
    report_lines.append("Total:")
    report_lines.append(f"{total_time * 1000:.1f} ms")
    report_lines.append("Average per Row:")
    report_lines.append(f"{average_per_row:.2f} microseconds")
    report_lines.append("Throughput:")
    report_lines.append(f"{throughput:.0f} rows/sec")
    report_lines.append("--------------------------------")

    return processed_rows, skipped_rows, load_time, compute_time, write_time


def create_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def run_live_mode(args: argparse.Namespace) -> None:
    collector = NSEOptionChainDataCollector(
        stable_symbol=args.stable_symbol,
        volatile_symbol=args.volatile_symbol,
        output_dir=str(DEFAULT_HISTORY_DIR),
    )

    if args.collect_history:
        print("Starting historical snapshot collection...")
        collector.collect_history_until_target(
            target_rows=args.target_rows,
            snapshot_interval_seconds=args.snapshot_interval,
        )
        print("Historical snapshot collection complete.")
        return

    stable_path, volatile_path = collector.collect_and_save()
    print(f"Live data collection complete:\n  Stable: {stable_path}\n  Volatile: {volatile_path}")


def run_benchmark_mode(args: argparse.Namespace) -> None:
    if not args.input:
        raise ValueError("Benchmark mode requires --input path to historical data.")

    input_path = Path(args.input)
    if not input_path.exists():
        raise FileNotFoundError(f"Input file does not exist: {input_path}")

    output_path = Path(args.output) if args.output else DEFAULT_OUTPUT_DIR / f"{input_path.stem.replace('_history','')}_output.csv"
    report_path = Path(args.benchmark_report)

    create_directory(output_path.parent)
    create_directory(report_path.parent)

    report_lines: List[str] = ["--------------------------------"]
    print(f"Benchmarking historical dataset: {input_path}")

    processed_rows, skipped_rows, load_time, compute_time, write_time = process_benchmark_dataset(
        input_path=input_path,
        output_path=output_path,
        report_lines=report_lines,
    )

    total_time = load_time + compute_time + write_time
    print(f"Benchmark complete. Output written to {output_path}")

    report_lines.append(f"Benchmark report saved to: {report_path}")
    report_text = "\n".join(report_lines)
    report_path.write_text(report_text, encoding="utf-8")
    print(report_text)


def main() -> None:
    args = parse_args()

    if args.mode == "live":
        run_live_mode(args)
    elif args.mode == "benchmark":
        run_benchmark_mode(args)
    else:
        raise ValueError(f"Unsupported mode: {args.mode}")


if __name__ == "__main__":
    main()
