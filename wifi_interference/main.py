import argparse
import sys
from pathlib import Path

from frequency_interference import (
    run_iw_scan,
    parse_iw_scan,
    compute_interference,
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="5 GHz WiFi Interference Scanner"
    )

    src = parser.add_mutually_exclusive_group(required=True)
    src.add_argument("--iface", help="Wireless interface (e.g. wlan0)")
    src.add_argument("--file", help="Path to saved iw scan output")

    parser.add_argument("--top", type=int, default=5, help="Top N channels")
    parser.add_argument("--show-networks", action="store_true",
                        help="Show detected networks")

    args = parser.parse_args()

    # ── Get raw scan data ─────────────────────────────
    if args.file:
        raw_output = Path(args.file).read_text()
    else:
        raw_output = run_iw_scan(args.iface)

    # ── Parse networks ────────────────────────────────
    networks = parse_iw_scan(raw_output)

    print(f"\nFound {len(networks)} networks (5 GHz)\n")

    if args.show_networks:
        print("📡 Networks:\n")
        for n in sorted(networks, key=lambda x: x.signal_dbm, reverse=True):
            print(f"{n.ssid:25} | Ch {n.channel:3} | {n.signal_dbm:6} dBm")

    # ── Compute interference ──────────────────────────
    reports = compute_interference(networks)

    print("\n📊 Channel Interference:\n")

    for r in reports:
        print(f"Channel {r.channel:3} | "
              f"Score: {r.interference_score:5} | "
              f"{r.interference_label:6} | "
              f"Networks: {r.network_count}")

    # ── Best channel ──────────────────────────────────
    best = reports[0]
    print("\n✅ Best Channel:")
    print(f"→ Channel {best.channel} "
          f"(Score: {best.interference_score}, {best.interference_label})")

    return 0


if __name__ == "__main__":
    sys.exit(main())
