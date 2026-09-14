"""
Example 2: Test Touch
Test XPT2046 touch controller and print coordinates
"""

from machine import Pin, SPI
from config.pins import *
from lib.ili9341 import ILI9341, WHITE, BLACK, RED
from lib.xpt2046 import XPT2046
import time

def main():
    print("Testing XPT2046 Touch Controller...")

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

    # Initialize touch
    touch = XPT2046(
        spi=spi,
        cs=Pin(TOUCH_CS),
        irq=Pin(TOUCH_IRQ),
        width=320,
        height=240
    )

    # Turn on backlight
    backlight = Pin(TFT_BL, Pin.OUT)
    backlight.value(1)

    # Clear screen
    display.fill(BLACK)
    display.text("Touch Test", 10, 10, WHITE)
    display.text("Touch anywhere...", 10, 30, WHITE)

    print("Touch controller ready!")
    print("Touch the screen to see coordinates...")
    print("Press Ctrl+C to exit")

    try:
        while True:
            touch_data = touch.get_touch()

            if touch_data:
                x, y, pressure = touch_data

                # Print to serial
                print(f"Touch: X={x:3d}, Y={y:3d}, P={pressure:4d}")

                # Draw on screen
                display.fill_rect(0, 60, 320, 180, BLACK)  # Clear old data
                display.text(f"X: {x}", 10, 80, WHITE)
                display.text(f"Y: {y}", 10, 100, WHITE)
                display.text(f"Pressure: {pressure}", 10, 120, WHITE)

                # Draw a red dot at touch position
                display.fill_rect(x-2, y-2, 5, 5, RED)

                # Wait for release
                while touch.is_touched():
                    time.sleep(0.05)

            time.sleep(0.05)

    except KeyboardInterrupt:
        display.fill(BLACK)
        backlight.value(0)
        print("\nDone!")

if __name__ == "__main__":
    main()
