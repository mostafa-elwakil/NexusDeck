"""
Example 4: Full StreamDeck
Complete StreamDeck with server synchronization
Requires server running at the configured URL
"""

from machine import Pin, SPI
from config.settings import *
from config.pins import *
from lib.ili9341 import ILI9341
from lib.xpt2046 import XPT2046
from lib.streamdeck import StreamDeck
from lib.utils import connect_wifi

def main():
    print("=" * 40)
    print("Full NexusDeck Example")
    print("=" * 40)

    # Initialize SPI
    print("\nInitializing display...")
    spi = SPI(
        2,
        baudrate=20000000,  # 20MHz (ESP32 safe limit)
        polarity=0,
        phase=0,
        sck=Pin(TFT_SCLK),
        mosi=Pin(TFT_MOSI),
        miso=Pin(TFT_MISO)
    )

    # Initialize display
    display = ILI9341(
        spi=spi,
        cs=Pin(TFT_CS),
        dc=Pin(TFT_DC),
        width=320,
        height=240,
        rotation=ROTATION
    )

    # Turn on backlight
    backlight = Pin(TFT_BL, Pin.OUT)
    backlight.value(1)

    # Clear screen
    display.fill(0x0000)
    print("Display ready!")

    # Initialize touch
    print("\nInitializing touch...")
    touch = XPT2046(
        spi=spi,
        cs=Pin(TOUCH_CS),
        irq=Pin(TOUCH_IRQ),
        width=320,
        height=240
    )
    print("Touch ready!")

    # Connect to WiFi
    print(f"\nConnecting to WiFi ({WIFI_SSID})...")
    wlan = connect_wifi(WIFI_SSID, WIFI_PASSWORD, timeout=CONNECT_TIMEOUT)

    if not wlan:
        print("ERROR: WiFi connection failed!")
        display.fill(0xF800)  # Red screen
        display.text("WiFi Failed!", 80, 110, 0xFFFF)
        return

    # Initialize StreamDeck
    print("\nInitializing StreamDeck...")
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
    print("=" * 40)
    print("\nPress Ctrl+C to exit")

    # Run main loop
    try:
        streamdeck.run()
    except KeyboardInterrupt:
        print("\nShutting down...")
        display.fill(0x0000)
        backlight.value(0)
        print("Done!")

if __name__ == "__main__":
    main()
