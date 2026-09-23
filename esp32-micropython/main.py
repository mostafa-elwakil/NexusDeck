"""
ESP32-2432S028 NexusDeck - Main Entry Point
MicroPython Version
"""

import time
from machine import Pin, SPI
from config.settings import *
from config.pins import *
from lib.ili9341 import ILI9341
from lib.xpt2046 import XPT2046
from lib.streamdeck import StreamDeck
from lib.utils import connect_wifi

def main():
    """Main initialization and loop"""

    print("=" * 40)
    print("ESP32-2432S028 NexusDeck")
    print("MicroPython Version")
    print("=" * 40)

    # Initialize SPI for display
    print("\n1. Initializing display...")
    spi_display = SPI(
        2,  # HSPI
        baudrate=20000000,  # 20MHz (ESP32 limit: 26.666MHz)
        polarity=0,
        phase=0,
        sck=Pin(TFT_SCLK),
        mosi=Pin(TFT_MOSI),
        miso=Pin(TFT_MISO)
    )

    # Initialize display (ESP32-2432S028: ILI9341 240×320)
    # rotation=1 makes it 320×240 Landscape for StreamDeck layout
    display = ILI9341(
        spi=spi_display,
        cs=Pin(TFT_CS),
        dc=Pin(TFT_DC),
        rst=None,  # Software reset
        rotation=ROTATION
    )

    # Turn on backlight
    backlight = Pin(TFT_BL, Pin.OUT)
    backlight.value(1)

    # Clear screen
    display.fill(0x0000)
    print("Display ready!")

    # Initialize SPI for touch (shared bus, different CS)
    print("\n2. Initializing touch...")
    touch = XPT2046(
        spi=spi_display,
        cs=Pin(TOUCH_CS),
        irq=Pin(TOUCH_IRQ),
        rotation=ROTATION  # Must match display rotation
    )
    print("Touch ready!")

    # Connect to WiFi
    print("\n3. Connecting to WiFi...")
    wlan = connect_wifi(WIFI_SSID, WIFI_PASSWORD, timeout=CONNECT_TIMEOUT)

    if not wlan:
        print("WiFi connection failed!")
        display.fill(0xF800)  # Red screen
        return

    # Initialize StreamDeck
    print("\n4. Initializing StreamDeck...")
    streamdeck = StreamDeck(
        display=display,
        touch=touch,
        server_url=SERVER_URL,
        grid_cols=GRID_COLS,
        grid_rows=GRID_ROWS
    )

    print("\n" + "=" * 40)
    print("NexusDeck Ready!")
    print(f"Server: {SERVER_URL}")
    print(f"Grid: {GRID_COLS}x{GRID_ROWS}")
    print("=" * 40 + "\n")

    # Run main loop
    try:
        streamdeck.run()
    except KeyboardInterrupt:
        print("\nShutting down...")
        display.fill(0x0000)
        backlight.value(0)

# Auto-run on boot
if __name__ == "__main__":
    main()
