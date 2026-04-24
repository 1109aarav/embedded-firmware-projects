connect the wifi card and run this command in terminal: 
sudo python3 main.py --iface <interface name> --show-networks

What It Does

Scans all nearby 5 GHz networks via sudo iw dev <iface> scan
Parses BSSID, SSID, channel, signal strength (dBm), channel width, and security
Accumulates interference per channel using linear power addition (dBm → mW → dBm)
Computes normalized interference scores (0–100) weighted by network count + signal power
Outputs ranked channel list and recommends the best channel

--iface      WiFi interface name (e.g. wlan0, wlx...)
--file       Path to saved iw scan output (for offline analysis)
--top N      Show top N least-congested channels (default: 5)
--show-networks  Print all detected networks with SSID, channel, RSSI
