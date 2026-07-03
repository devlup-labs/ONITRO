import unittest
from datetime import datetime

from csv_loader import _parse_datetime


class CsvLoaderTests(unittest.TestCase):
    def test_parse_datetime_with_space_and_microseconds(self):
        parsed = _parse_datetime("2026-07-01 13:52:41.420657")
        self.assertEqual(parsed, datetime(2026, 7, 1, 13, 52, 41, 420657))


if __name__ == "__main__":
    unittest.main()
