"""
Production-ready NSE option chain data collector for QuantLib derivatives analysis.

Fetches live option chain data from NSE (National Stock Exchange of India) API
and bridges it with QuantLib analytical functions for:
- Yield-to-Maturity (YTM) calculations
- Forward pricing
- Futures pricing
- Option pricing

The collector:
1. Establishes secure NSE API sessions with proper headers and session handshakes
2. Fetches live option chain JSON for specified stocks
3. Parses and transforms data into QuantLib-compatible format
4. Calculates precise Time-to-Maturity using Actual365Fixed day counter
5. Outputs structured Pandas DataFrames for downstream analysis
"""

import json
import os
import re
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple

import pandas as pd
import requests
from requests.adapters import HTTPAdapter
from urllib3.util.retry import Retry


class NSESessionManager:
    """Manages secure NSE API sessions with proper authentication and retry logic."""

    # Multiple User-Agent strings to rotate and avoid blocking
    USER_AGENTS = [
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:91.0) Gecko/20100101 Firefox/91.0",
    ]

    NSE_BASE_URL = "https://www.nseindia.com"
    NSE_OPTIONS_API = f"{NSE_BASE_URL}/api/optionchain"
    NSE_LIVE_API = f"{NSE_BASE_URL}/api/quote-derivative"

    def __init__(self, timeout: int = 10, max_retries: int = 3):
        """
        Initialize NSE session manager.

        Args:
            timeout: Request timeout in seconds.
            max_retries: Maximum retry attempts for failed requests.
        """
        self.timeout = timeout
        self.max_retries = max_retries
        self._user_agent_index = 0
        self.session = self._create_session()

    def _create_session(self) -> requests.Session:
        """Create a robust requests session with retry strategy."""
        session = requests.Session()

        # Configure retry strategy
        retry_strategy = Retry(
            total=self.max_retries,
            backoff_factor=0.5,
            status_forcelist=[429, 500, 502, 503, 504],
            allowed_methods=["GET"],
        )

        adapter = HTTPAdapter(max_retries=retry_strategy)
        session.mount("http://", adapter)
        session.mount("https://", adapter)

        # Set initial headers
        self._update_headers(session)

        # Warm the session (fetch base page) to obtain cookies and handshake
        try:
            session.get(self.NSE_BASE_URL, timeout=self.timeout)
        except Exception:
            # Non-fatal; some environments may block this call
            pass
        return session

    def _update_headers(self, session: requests.Session) -> None:
        """Update session headers with rotating User-Agent."""
        user_agent = self.USER_AGENTS[self._user_agent_index % len(self.USER_AGENTS)]
        self._user_agent_index += 1

        session.headers.update(
            {
                "User-Agent": user_agent,
                "Accept": "application/json, text/plain, */*",
                "Accept-Encoding": "gzip, deflate",
                "Accept-Language": "en-US,en;q=0.9",
                "Origin": self.NSE_BASE_URL,
                "X-Requested-With": "XMLHttpRequest",
                "Cache-Control": "no-cache",
                "Referer": self.NSE_BASE_URL,
                "Pragma": "no-cache",
                "Sec-Fetch-Dest": "empty",
                "Sec-Fetch-Mode": "cors",
                "Sec-Fetch-Site": "same-origin",
            }
        )

    def fetch_contract_info(self, symbol: str) -> Optional[Dict]:
        """
        Fetch NSE option-contract metadata such as expiry dates and strike prices.

        Args:
            symbol: NSE stock symbol.

        Returns:
            Contract metadata JSON or None if unavailable.
        """
        try:
            print(f"[NSE] Fetching contract info for {symbol}...")
            url = f"{self.NSE_BASE_URL}/api/option-chain-contract-info?symbol={symbol}"
            response = self.session.get(url, timeout=self.timeout)
            response.raise_for_status()

            data = response.json()
            if data and data.get("expiryDates"):
                return data

            print(f"[NSE] No contract metadata returned for {symbol}")
            return None

        except Exception as exc:
            print(f"[ERROR] Failed to fetch contract info for {symbol}: {exc}")
            return None

    def fetch_option_chain(self, symbol: str) -> Optional[Dict]:
        """
        Fetch live option chain data from NSE.

        Args:
            symbol: NSE stock symbol (e.g., 'TCS', 'ICICIBANK').

        Returns:
            Parsed JSON response or None if request fails.
        """
        try:
            print(f"[NSE] Initiating session for {symbol}...")
            # First, initiate a session by fetching the main page to establish cookies.
            self.session.get(self.NSE_BASE_URL, timeout=self.timeout)

            contract_info = self.fetch_contract_info(symbol)
            if not contract_info:
                print(f"[ERROR] Unable to retrieve contract metadata for {symbol}")
                return None

            expiry_dates = contract_info.get("expiryDates", [])
            if not expiry_dates:
                print(f"[ERROR] No expiry dates returned for {symbol}")
                return None

            combined_data = None
            all_strike_prices = set()
            total_records = 0

            for expiry in expiry_dates:
                url = (
                    f"{self.NSE_BASE_URL}/api/option-chain-v3?type=Equity"
                    f"&symbol={symbol}&expiry={expiry}"
                )
                try:
                    response = self.session.get(url, timeout=self.timeout)
                    response.raise_for_status()
                    payload = response.json()
                    records = payload.get("records", {}).get("data", [])
                    if records:
                        total_records += len(records)
                        if combined_data is None:
                            combined_data = payload
                        else:
                            combined_data["records"]["data"].extend(records)

                        strike_prices = payload.get("records", {}).get("strikePrices", [])
                        all_strike_prices.update(strike_prices)

                except Exception as exc:
                    print(f"[NSE] option-chain-v3 attempt failed for {url}: {exc}")
                    continue

            if combined_data is None or total_records == 0:
                print(f"[ERROR] No option-chain data returned for {symbol}")
                return None

            combined_data["records"]["expiryDates"] = expiry_dates
            combined_data["records"]["strikePrices"] = sorted(
                all_strike_prices, key=lambda x: float(x)
            )

            print(
                f"[NSE] Successfully fetched {total_records} option records for {symbol} across {len(expiry_dates)} expiries"
            )
            return combined_data

        except requests.exceptions.Timeout:
            print(f"[ERROR] Timeout fetching option chain for {symbol}")
            return None
        except requests.exceptions.ConnectionError:
            print(f"[ERROR] Connection error fetching option chain for {symbol}")
            return None
        except json.JSONDecodeError:
            print(f"[ERROR] Invalid JSON response for {symbol}")
            return None
        except Exception as exc:
            print(f"[ERROR] Failed to fetch option chain for {symbol}: {exc}")
            return None

    def fetch_spot_price(self, symbol: str) -> Optional[float]:
        """
        Fetch current spot price for a symbol.

        Args:
            symbol: NSE stock symbol.

        Returns:
            Current spot price or None if fetch fails.
        """
        try:
            print(f"[NSE] Fetching spot price for {symbol}...")
            candidate_urls = [
                f"{self.NSE_LIVE_API}?symbol={symbol}",
                f"{self.NSE_BASE_URL}/api/quote-equity?symbol={symbol}",
                f"{self.NSE_BASE_URL}/api/quote?symbol={symbol}",
            ]

            response = None
            data = None
            last_exc = None
            for url in candidate_urls:
                try:
                    response = self.session.get(url, timeout=self.timeout)
                    response.raise_for_status()
                    data = response.json()
                    break
                except Exception as exc:
                    last_exc = exc
                    print(f"[NSE] spot attempt failed for {url}: {exc}")

            if data is None:
                raise last_exc

            # The live API may place the spot price in several keys depending on the endpoint
            if data:
                # Try common paths
                spot = None
                if isinstance(data, dict):
                    # priceInfo -> lastPrice
                    if "priceInfo" in data and isinstance(data["priceInfo"], dict):
                        spot = data["priceInfo"].get("lastPrice")
                    # records.underlyingValue
                    if spot is None:
                        spot = data.get("underlyingValue") or data.get("records", {}).get("underlyingValue")
                if spot is not None:
                    return float(spot)

            raise ValueError("Spot price not found in response")

            return None
        except Exception as exc:
            print(f"[ERROR] Failed to fetch spot price for {symbol}: {exc}")
            return None


class NSEOptionChainParser:
    """Parses NSE option chain JSON data into structured format."""

    @staticmethod
    def parse_date(date_string: str) -> datetime:
        """
        Parse NSE date strings (e.g., '25-JAN-2024' or '25Jan2024').

        Args:
            date_string: Date string from NSE API.

        Returns:
            datetime object.
        """
        date_formats = ["%d-%b-%Y", "%d%b%Y", "%d-%m-%Y", "%d/%m/%Y"]

        for fmt in date_formats:
            try:
                return datetime.strptime(date_string.upper(), fmt)
            except ValueError:
                continue

        raise ValueError(f"Unable to parse date: {date_string}")

    @staticmethod
    def parse_option_chain(data: Dict, symbol: str, spot_price: float) -> pd.DataFrame:
        """
        Parse NSE option chain JSON into structured DataFrame.

        Args:
            data: JSON data from NSE API.
            symbol: Stock symbol.
            spot_price: Current spot price of underlying.

        Returns:
            Structured DataFrame with normalized column names.
        """
        if not data or "records" not in data:
            return pd.DataFrame()

        records = data.get("records", {}).get("data", [])
        if not records:
            return pd.DataFrame()

        rows = []
        for record in records:
            # Extract call option data
            ce = record.get("CE", {})
            if ce and ce.get("strikePrice"):
                rows.append(
                    {
                        "underlying_symbol": symbol,
                        "spot_price": spot_price,
                        "strike_price": float(ce.get("strikePrice", 0)),
                        "expiry_date_str": ce.get("expiryDate", record.get("expiryDates", "")),
                        "option_type": "CALL",
                        "ltp": float(ce.get("lastPrice", 0)),
                        "bid": float(ce.get("buyPrice1", ce.get("bidprice", 0)) or 0),
                        "ask": float(ce.get("sellPrice1", ce.get("askPrice", 0)) or 0),
                        "volume": int(ce.get("totalTradedVolume", 0) or 0),
                        "open_interest": int(ce.get("openInterest", 0) or 0),
                        "iv": float(ce.get("impliedVolatility", 0) or 0) / 100.0,
                        "delta": float(ce.get("delta", 0) or 0),
                        "gamma": float(ce.get("gamma", 0) or 0),
                        "theta": float(ce.get("theta", 0) or 0),
                        "vega": float(ce.get("vega", 0) or 0),
                        "collected_at": datetime.now().isoformat(timespec="seconds"),
                    }
                )

            # Extract put option data
            pe = record.get("PE", {})
            if pe and pe.get("strikePrice"):
                rows.append(
                    {
                        "underlying_symbol": symbol,
                        "spot_price": spot_price,
                        "strike_price": float(pe.get("strikePrice", 0)),
                        "expiry_date_str": pe.get("expiryDate", record.get("expiryDates", "")),
                        "option_type": "PUT",
                        "ltp": float(pe.get("lastPrice", 0)),
                        "bid": float(pe.get("buyPrice1", pe.get("bidprice", 0)) or 0),
                        "ask": float(pe.get("sellPrice1", pe.get("askPrice", 0)) or 0),
                        "volume": int(pe.get("totalTradedVolume", 0) or 0),
                        "open_interest": int(pe.get("openInterest", 0) or 0),
                        "iv": float(pe.get("impliedVolatility", 0) or 0) / 100.0,
                        "delta": float(pe.get("delta", 0) or 0),
                        "gamma": float(pe.get("gamma", 0) or 0),
                        "theta": float(pe.get("theta", 0) or 0),
                        "vega": float(pe.get("vega", 0) or 0),
                        "collected_at": datetime.now().isoformat(timespec="seconds"),
                    }
                )

        df = pd.DataFrame(rows)

        if not df.empty:
            print(f"[Parser] Parsed {len(df)} option records for {symbol}")
            print(
                f"[Parser] Calls: {(df['option_type'] == 'CALL').sum()}, Puts: {(df['option_type'] == 'PUT').sum()}"
            )

        return df

    @staticmethod
    def enrich_with_quantlib_data(
        df: pd.DataFrame, risk_free_rate: float = 0.07, dividend_yield: float = 0.02
    ) -> pd.DataFrame:
        """
        Enrich DataFrame with QuantLib-required fields.

        Converts NSE dates to QuantLib.Date objects and calculates Time-to-Maturity
        using Actual365Fixed day counter.

        Args:
            df: DataFrame with parsed option chain data.
            risk_free_rate: Risk-free interest rate (default 7% for India).
            dividend_yield: Expected dividend yield (default 2%).

        Returns:
            Enhanced DataFrame with QuantLib fields.
        """
        if df.empty:
            return df

        # Parse expiry dates
        try:
            df["expiry_date"] = df["expiry_date_str"].apply(
                NSEOptionChainParser.parse_date
            )
        except Exception as exc:
            print(f"[Parser] Error parsing expiry dates: {exc}")
            return df

        # Calculate evaluation date and time-to-maturity
        evaluation_date = datetime.now()
        df["evaluation_date"] = evaluation_date

        # Time-to-maturity in years (Actual365Fixed: 365 days/year)
        df["time_to_maturity"] = df["expiry_date"].apply(
            lambda x: max(
                (x - evaluation_date).days / 365.0, 0.0001
            )  # Avoid zero division
        )

        # Add risk parameters for QuantLib calculators
        df["risk_free_rate"] = risk_free_rate
        df["dividend_yield"] = dividend_yield

        # Mid-price for valuation
        df["mid_price"] = (df["bid"] + df["ask"]) / 2.0

        print(
            f"[Parser] Enriched DataFrame with QuantLib fields. "
            f"Time-to-Maturity range: {df['time_to_maturity'].min():.4f} to {df['time_to_maturity'].max():.4f} years"
        )

        return df


class NSEOptionChainDataCollector:
    """Production-ready NSE option chain data collector for QuantLib analysis."""

    def __init__(
        self,
        stable_symbol: str = "TCS",
        volatile_symbol: str = "ICICIBANK",
        output_dir: str = "./data",
        risk_free_rate: float = 0.07,
        dividend_yield: float = 0.02,
        timeout: int = 10,
    ):
        """
        Initialize NSE option chain collector.

        Args:
            stable_symbol: Low-volatility stock symbol (e.g., 'TCS').
            volatile_symbol: High-volatility stock symbol (e.g., 'ICICIBANK').
            output_dir: Directory to save processed CSV files.
            risk_free_rate: Risk-free interest rate for India (default 7%).
            dividend_yield: Expected dividend yield (default 2%).
            timeout: NSE API timeout in seconds.
        """
        self.stable_symbol = stable_symbol
        self.volatile_symbol = volatile_symbol
        self.output_dir = output_dir
        self.risk_free_rate = risk_free_rate
        self.dividend_yield = dividend_yield

        self.session_manager = NSESessionManager(timeout=timeout)
        self.parser = NSEOptionChainParser()

        self.stable_options = pd.DataFrame()
        self.volatile_options = pd.DataFrame()
        self.stable_spot_price = None
        self.volatile_spot_price = None

        os.makedirs(output_dir, exist_ok=True)

    def fetch_and_parse_symbol(self, symbol: str) -> Tuple[Optional[float], pd.DataFrame]:
        """
        Fetch and parse option chain data for a single symbol.

        Args:
            symbol: NSE stock symbol.

        Returns:
            Tuple of (spot_price, parsed_dataframe).
        """
        print(f"\n{'=' * 70}")
        print(f"FETCHING DATA FOR {symbol}")
        print(f"{'=' * 70}")

        # Fetch option chain first (it often contains the current underlying value)
        raw_data = self.session_manager.fetch_option_chain(symbol)
        if not raw_data:
            print(f"[ERROR] Unable to fetch option chain for {symbol}")
            # Try to get spot price as a fallback
            spot_price = self.session_manager.fetch_spot_price(symbol)
            if not spot_price:
                print(f"[ERROR] Unable to fetch spot price for {symbol}")
                return None, pd.DataFrame()
            return spot_price, pd.DataFrame()

        # Prefer spot price from option-chain response when available
        spot_price = raw_data.get("records", {}).get("underlyingValue")
        if spot_price is None:
            spot_price = self.session_manager.fetch_spot_price(symbol)
            if spot_price is None:
                print(f"[ERROR] Unable to determine spot price for {symbol}")
                return None, pd.DataFrame()

        # Parse option chain
        df = self.parser.parse_option_chain(raw_data, symbol, spot_price)

        # Enrich with QuantLib data
        if not df.empty:
            df = self.parser.enrich_with_quantlib_data(
                df,
                risk_free_rate=self.risk_free_rate,
                dividend_yield=self.dividend_yield,
            )

        return spot_price, df

    def save_to_csv(self, data: pd.DataFrame, symbol: str) -> Optional[str]:
        """
        Save option chain data to CSV file.

        Args:
            data: DataFrame to save.
            symbol: Stock symbol (used in filename).

        Returns:
            File path if successful, None otherwise.
        """
        if data.empty:
            print(f"[INFO] Skipping CSV save for {symbol}; no data available.")
            return None

        filepath = os.path.join(self.output_dir, f"options_{symbol.lower()}.csv")
        data.to_csv(filepath, index=False)
        print(f"[SUCCESS] Data saved to {filepath}")
        return filepath

    def collect_and_save(self) -> Tuple[Optional[str], Optional[str]]:
        """
        Collect option chains for both symbols and save to CSV files.

        Returns:
            Tuple of (stable_symbol_path, volatile_symbol_path).
        """
        # Collect stable symbol
        self.stable_spot_price, self.stable_options = self.fetch_and_parse_symbol(
            self.stable_symbol
        )
        stable_path = self.save_to_csv(self.stable_options, self.stable_symbol)

        # Collect volatile symbol
        self.volatile_spot_price, self.volatile_options = (
            self.fetch_and_parse_symbol(self.volatile_symbol)
        )
        volatile_path = self.save_to_csv(self.volatile_options, self.volatile_symbol)

        return stable_path, volatile_path

    def print_options_summary(self, label: str, symbol: str, data: pd.DataFrame) -> None:
        """Print a comprehensive summary for one option-chain dataset."""
        print(f"\n{label}")
        print("=" * 70)

        if data.empty:
            print("No option data collected.")
            return

        print(f"Symbol: {symbol}")
        print(f"Spot Price: ${data['spot_price'].iloc[0]:.2f}")
        print(f"Total Contracts: {len(data)}")
        print(f"Expiry Date(s): {', '.join(sorted(data['expiry_date_str'].unique()))}")
        print(f"CALL Contracts: {(data['option_type'] == 'CALL').sum()}")
        print(f"PUT Contracts: {(data['option_type'] == 'PUT').sum()}")

        # Strike price statistics
        print(
            f"Strike Price Range: ${data['strike_price'].min():.2f} - ${data['strike_price'].max():.2f}"
        )

        # LTP statistics by option type
        for opt_type in ["CALL", "PUT"]:
            subset = data[data["option_type"] == opt_type]
            if not subset.empty:
                ltp_mean = subset["ltp"].mean()
                ltp_max = subset["ltp"].max()
                print(
                    f"  {opt_type} LTP: Avg ${ltp_mean:.2f}, Max ${ltp_max:.2f}"
                )

        # Time-to-maturity statistics
        print(
            f"Time-to-Maturity Range: {data['time_to_maturity'].min():.4f} - {data['time_to_maturity'].max():.4f} years"
        )
        print(
            f"IV Range: {data['iv'].min():.2%} - {data['iv'].max():.2%}"
        )

        # Display sample data
        display_columns = [
            col
            for col in [
                "underlying_symbol",
                "strike_price",
                "option_type",
                "ltp",
                "bid",
                "ask",
                "iv",
                "time_to_maturity",
                "volume",
                "open_interest",
            ]
            if col in data.columns
        ]

        print("\nSample Records:")
        print(data[display_columns].head(10).to_string(index=False))

    def get_summary_statistics(self) -> None:
        """Print comprehensive summary for both symbols."""
        print(f"\n{'=' * 70}")
        print("NSE OPTION CHAIN COLLECTION SUMMARY")
        print(f"{'=' * 70}")
        print(f"Timestamp: {datetime.now().isoformat()}")
        print(f"Risk-Free Rate: {self.risk_free_rate:.2%}")
        print(f"Dividend Yield: {self.dividend_yield:.2%}")

        self.print_options_summary(
            "STABLE UNDERLYING (LOW VOLATILITY)",
            self.stable_symbol,
            self.stable_options,
        )
        self.print_options_summary(
            "VOLATILE UNDERLYING (HIGH VOLATILITY)",
            self.volatile_symbol,
            self.volatile_options,
        )


if __name__ == "__main__":
    # Production example: Fetch live NSE option chain data
    print("\n" + "=" * 70)
    print("NSE OPTION CHAIN DATA COLLECTOR - PRODUCTION EXAMPLE")
    print("=" * 70)

    collector = NSEOptionChainDataCollector(
        stable_symbol="TCS",  # Tata Consultancy Services (Low volatility)
        volatile_symbol="ICICIBANK",  # ICICI Bank (High volatility)
        output_dir="./data",
        risk_free_rate=0.07,  # 7% risk-free rate (India)
        dividend_yield=0.02,  # 2% expected dividend yield
        timeout=10,  # API timeout
    )

    # Collect and save option chain data
    stable_path, volatile_path = collector.collect_and_save()

    # Print comprehensive summary
    collector.get_summary_statistics()

    print("\n" + "=" * 70)
    print("DATA COLLECTION COMPLETE")
    print("=" * 70)
    print(f"Stable Symbol Output: {stable_path}")
    print(f"Volatile Symbol Output: {volatile_path}")
    print("\nThe generated CSV files contain all QuantLib-required fields:")
    print("  - spot_price: Current underlying price")
    print("  - strike_price: Option strike price")
    print("  - ltp: Last Traded Price (option price)")
    print("  - time_to_maturity: T (years, using Actual365Fixed)")
    print("  - risk_free_rate: Risk-free interest rate")
    print("  - dividend_yield: Expected dividend yield")
    print("  - iv: Implied volatility (as decimal)")
    print("  - Greeks: delta, gamma, theta, vega")
    print("\nThese can be directly consumed by QuantLib calculators for:")
    print("  - Forward/Futures pricing")
    print("  - Option valuation")
    print("  - Greeks and risk analysis")
    print("=" * 70)
