import tempfile
import unittest
from pathlib import Path

import pandas as pd

from main import process_benchmark_dataset


class BenchmarkReportingTests(unittest.TestCase):
    def test_benchmark_report_includes_feature_wise_timings(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            input_path = tmp_path / "sample_history.csv"
            output_path = tmp_path / "sample_output.csv"

            pd.DataFrame(
                [
                    {
                        "underlying_symbol": "TCS",
                        "spot_price": 2500.0,
                        "strike_price": 2600.0,
                        "expiry_date_str": "2026-07-01",
                        "option_type": "CALL",
                        "ltp": 100.0,
                        "bid": 99.0,
                        "ask": 101.0,
                        "volume": 500,
                        "open_interest": 100,
                        "iv": 0.2,
                        "delta": 0.5,
                        "gamma": 0.01,
                        "theta": -0.1,
                        "vega": 0.2,
                        "risk_free_rate": 0.06,
                        "dividend_yield": 0.02,
                        "evaluation_date": "2026-07-01 13:52:41.420657",
                        "time_to_maturity": 0.25,
                        "collected_at": "2026-07-01 13:52:41.420657",
                        "mid_price": 100.0,
                    }
                ]
            ).to_csv(input_path, index=False)

            report_lines = []
            process_benchmark_dataset(input_path, output_path, report_lines)
            report_text = "\n".join(report_lines)

            self.assertIn("Feature-wise timings:", report_text)
            self.assertIn("Forward/Futures pricing:", report_text)
            self.assertIn("Bond pricing:", report_text)
            self.assertIn("YTM solver:", report_text)


if __name__ == "__main__":
    unittest.main()
