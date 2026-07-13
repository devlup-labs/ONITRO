import tempfile
import unittest
from pathlib import Path

import pandas as pd

from benchmarks import process_benchmark_dataset


class BenchmarkReportingTests(unittest.TestCase):
    def test_benchmark_report_includes_feature_wise_timings(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            input_path = tmp_path / "sample_history.csv"
            output_path = tmp_path / "sample_output.csv"

            pd.DataFrame(
                [
                    {
                        "Symbol": "TCS",
                        "Date": "01-Jan-2025",
                        "Expiry": "27-Feb-2025",
                        "Option type": "CE",
                        "Strike Price": 3400.0,
                        "Open": 200.0,
                        "High": 210.0,
                        "Low": 195.0,
                        "Close": 205.0,
                        "LTP": 203.0,
                        "Settle Price": 205.0,
                        "No. of contracts": 100,
                        "Turnover * in  ₹ Lakhs": 50.0,
                        "Premium Turnover ** in   ₹ Lakhs": 10.0,
                        "Open Int": 1200,
                        "Change in OI": 100,
                        "Underlying Value": 4112.45,
                        "Implied Volatility (%)": 22.44,
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
