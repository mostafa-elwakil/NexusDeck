"""
Utility functions for ESP32 StreamDeck
"""

import network
import time
import ujson as json
import urequests as requests


def connect_wifi(ssid, password, timeout=10):
    """Connect to WiFi network"""
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)

    if wlan.isconnected():
        print("Already connected!")
        print_network_info(wlan)
        return wlan

    print(f"Connecting to {ssid}...")
    wlan.connect(ssid, password)

    start = time.time()
    while not wlan.isconnected():
        if time.time() - start > timeout:
            print("Connection timeout!")
            return None
        time.sleep(0.5)
        print(".", end="")

    print("\nConnected!")
    print_network_info(wlan)
    return wlan


def print_network_info(wlan):
    """Print network connection info"""
    config = wlan.ifconfig()
    print(f"IP: {config[0]}")
    print(f"Gateway: {config[2]}")
    print(f"DNS: {config[3]}")


def http_get(url, timeout=5):
    """Simple HTTP GET request"""
    try:
        response = requests.get(url, timeout=timeout)
        data = response.json() if response.headers.get('content-type') == 'application/json' else response.text
        response.close()
        return data
    except Exception as e:
        print(f"HTTP GET Error: {e}")
        return None


def http_post(url, data, timeout=5):
    """Simple HTTP POST request"""
    try:
        response = requests.post(
            url,
            json=data,
            headers={'Content-Type': 'application/json'},
            timeout=timeout
        )
        result = response.json() if response.headers.get('content-type') == 'application/json' else response.text
        response.close()
        return result
    except Exception as e:
        print(f"HTTP POST Error: {e}")
        return None


def map_value(x, in_min, in_max, out_min, out_max):
    """Map value from one range to another"""
    return int((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)


def rgb_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def format_uptime(seconds):
    """Format uptime in human-readable format"""
    hours = seconds // 3600
    minutes = (seconds % 3600) // 60
    secs = seconds % 60

    if hours > 0:
        return f"{hours}h {minutes}m"
    elif minutes > 0:
        return f"{minutes}m {secs}s"
    else:
        return f"{secs}s"


def load_json_file(filename):
    """Load JSON from file"""
    try:
        with open(filename, 'r') as f:
            return json.load(f)
    except Exception as e:
        print(f"Error loading {filename}: {e}")
        return None


def save_json_file(filename, data):
    """Save JSON to file"""
    try:
        with open(filename, 'w') as f:
            json.dump(data, f)
        return True
    except Exception as e:
        print(f"Error saving {filename}: {e}")
        return False


def free_memory():
    """Get free memory in KB"""
    import gc
    gc.collect()
    import micropython
    return micropython.mem_info()
