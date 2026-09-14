"""
Example 1: Test Display
Simple test to verify ILI9341 display is working
"""

from machine import Pin, SPI
from config.pins import *
from lib.ili9341 import ILI9341, RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, BLACK
import time

def main():
    print("Testing ILI9341 Display...")

    # Initialize SPI
    spi = SPI(
        2,  # HSPI
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
        rotation=1
    )

    # Turn on backlight
    backlight = Pin(TFT_BL, Pin.OUT)
    backlight.value(1)

    print("Display initialized!")

    # Test colors
    colors = [RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, BLACK]
    color_names = ["Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White", "Black"]

    for i, (color, name) in enumerate(zip(colors, color_names)):
        print(f"Showing {name}...")
        display.fill(color)
        time.sleep(1)

    # Test rectangles
    print("Testing rectangles...")
    display.fill(BLACK)

    display.fill_rect(10, 10, 60, 60, RED)
    display.fill_rect(80, 10, 60, 60, GREEN)
    display.fill_rect(150, 10, 60, 60, BLUE)

    display.fill_rect(10, 80, 60, 60, YELLOW)
    display.fill_rect(80, 80, 60, 60, CYAN)
    display.fill_rect(150, 80, 60, 60, MAGENTA)

    display.fill_rect(10, 150, 60, 60, WHITE)
    display.rect(80, 150, 60, 60, WHITE)

    print("Display test complete!")
    print("Press Ctrl+C to exit")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        display.fill(BLACK)
        backlight.value(0)
        print("\nDone!")

if __name__ == "__main__":
    main()
