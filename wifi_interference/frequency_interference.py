import subprocess
import re
import math
import logging
from dataclasses import dataclass, field
from sys import flags
from typing import Optional

logger = logging.getLogger(__name__)

CHANNEL_FREQ_MAP: dict[int, int] = {
    36: 5180,  40: 5200,  44: 5220,  48: 5240,   # UNII-1
    52: 5260,  56: 5280,  60: 5300,  64: 5320,   # UNII-2A
   100: 5500, 104: 5520, 108: 5540, 112: 5560,   # UNII-2C
   116: 5580, 120: 5600, 124: 5620, 128: 5640,
   132: 5660, 136: 5680, 140: 5700, 144: 5720,
   149: 5745, 153: 5765, 157: 5785, 161: 5805,   # UNII-3
   165: 5825,
}

FREQ_CHANNEL_MAP: dict[int, int] = {v: k for k, v in CHANNEL_FREQ_MAP.items()}

SORTED_CHANNELS = sorted(CHANNEL_FREQ_MAP.keys())

@dataclass
class Network:
    bssid: str
    ssid: str
    channel: int
    frequency_mhz: int
    signal_dbm: float
    channel_width_mhz: int = 20
    security: str = "open"
    band: str = "5GHz"

@dataclass
class ChannelReport:
    channel: int
    frequency_mhz: int
    network_count: int = 0
    cumulative_power_dbm: float = -120.0
    interference_score: float = 0.0
    networks: list = field(default_factory=list)

    @property
    def interference_label(self) -> str:
        if self.interference_score < 33:
            return "Low"
        if self.interference_score < 66:
            return "Medium"
        return "High"
    
def _freq_to_channel(mhz: int) -> Optional[int]:
    if mhz in FREQ_CHANNEL_MAP:
        return FREQ_CHANNEL_MAP[mhz]
    for freq, ch in FREQ_CHANNEL_MAP.items():
        if abs(freq - mhz) <= 10:
            return ch
    return None

def _parse_channel_width(block: str) -> int:
    vht_m = re.search(r"VHT operation.*?channel width[:\s]+(\d)", block, re.S | re.I)
    if vht_m:
        vw = int(vht_m.group(1))
        return {0: 20, 1: 80, 2: 160, 3: 160}.get(vw, 20)

    explicit = re.search(r"channel width[:\s]+(\d+)", block, re.I)
    if explicit:
        return int(explicit.group(1))
    
    ht_m = re.search(r"secondary channel offset[:\s]+(above|below)", block, re.I)
    if ht_m:
        return 40
    
    return 20

def _parse_security(block: str) -> str:
    if re.search(r"WPA2", block, re.I):
        return "WPA2"
    if re.search(r"WPA", block, re.I):
        return "WPA"
    if re.search(r"WEP", block, re.I):
        return "WEP"
    return "open"

def parse_iw_scan(raw_output: str) -> list[Network]:
    networks: list[Network] = []

    blocks = re.split(r"(?=^BSS )", raw_output, flags=re.MULTILINE)
    for block in blocks:
        if not block.strip():
            continue

        bssid_m = re.match(r"BSS\s+([\da-fA-F:]{17})", block)
        if not bssid_m:
            continue
        bssid = bssid_m.group(1)

        freq_m = re.search(r"freq(?:uency)?[:\s]+([\d.]+)\s*(GHz|MHz)?", block, re.I)
        if not freq_m:
            continue
        freq_val = float(freq_m.group(1))
        unit = (freq_m.group(2) or "MHz").upper()
        freq_mhz = int(freq_val * 1000) if unit == "GHz" or freq_val < 100 else int(freq_val)

        if freq_mhz < 5000:
            continue

        channel = _freq_to_channel(freq_mhz)
        if channel is None:
            logger.debug("unrecognized frequency %d MHz, skipping", freq_mhz)
            continue

        sig_m = re.search(r"signal[:\s]+([-\d.]+)\s*dbm", block, re.I)
        signal_dbm = float(sig_m.group(1)) if sig_m else -90.0

        ssid_m = re.search(r"SSID[:\s]+(.+)", block)
        ssid = ssid_m.group(1).strip() if ssid_m else "(hidden)"
        if not ssid:
            ssid = "(hidden)"

        channel_width = _parse_channel_width(block)
        security = _parse_security(block)

        networks.append(Network(
            bssid=bssid,
            ssid=ssid,
            channel=channel,
            frequency_mhz=freq_mhz,
            signal_dbm=signal_dbm,
            channel_width_mhz=channel_width,
            security=security,
        ))

    logger.info("parse %d 5GHz networks from output", len(networks))
    return networks

def compute_interference(networks: list[Network]) -> list[ChannelReport]:

    # Initialize all channels
    reports: dict[int, ChannelReport] = {
        ch: ChannelReport(channel=ch, frequency_mhz=CHANNEL_FREQ_MAP[ch])
        for ch in SORTED_CHANNELS
    }

    # Assign each network to its own channel
    for net in networks:
        if net.channel not in reports:
            continue

        rep = reports[net.channel]
        rep.network_count += 1

        # Convert dBm → linear, add, convert back
        existing_linear = 10 ** (rep.cumulative_power_dbm / 10)
        new_linear = 10 ** (net.signal_dbm / 10)
        rep.cumulative_power_dbm = 10 * math.log10(existing_linear + new_linear)

        rep.networks.append(net)

    # Compute score
    for rep in reports.values():
        if rep.network_count == 0:
            rep.interference_score = 0
            continue

        dbm_norm = max(0.0, rep.cumulative_power_dbm + 100)

        # Slightly better weighting
        rep.interference_score = rep.network_count * 5 + dbm_norm * 1.5

    # Normalize scores to 0–100
    all_scores = [r.interference_score for r in reports.values()]
    max_score = max(all_scores) if max(all_scores) > 0 else 1

    for rep in reports.values():
        rep.interference_score = round((rep.interference_score / max_score) * 100, 1)

    # Sort best → worst
    result = list(reports.values())
    result.sort(key=lambda r: r.interference_score)

    return result

def run_iw_scan(interface: str) -> str:
    
    cmd = ["sudo", "iw", "dev", interface, "scan"]
    logger.info("Running %s", " ".join(cmd))
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode != 0:
            raise RuntimeError(f"iw scan failed: (exit {result.returncode}) {result.stderr.strip()}")
        return result.stdout
    except FileNotFoundError:
        raise RuntimeError("iw not found. install with sudo apt install iw")
    except subprocess.TimeoutExpired:
        raise RuntimeError("iw scan timed out after 30 seconds")
        

        

                                

                            
