"""
Example 3: Simple Button Demo
Display 4 interactive buttons with touch feedback
"""

from machine import Pin, SPI
from config.pins import *
from lib.ili9341 import ILI9341, BLACK
from lib.xpt2046 import XPT2046
from lib.button import Button
import time

def main():
    print("Simple Button Demo...")

    # Initialize SPI
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

    # Create buttons
    buttons = [
        Button(10, 10, 140, 100, text="Button 1", icon="1", bg_color=0xF800),    # Red
        Button(170, 10, 140, 100, text="Button 2", icon="2", bg_color=0x07E0),   # Green
        Button(10, 130, 140, 100, text="Button 3", icon="3", bg_color=0x001F),   # Blue
        Button(170, 130, 140, 100, text="Button 4", icon="4", bg_color=0xFFE0),  # Yellow
    ]

    # Draw initial state
    display.fill(BLACK)
    for btn in buttons:
        btn.draw(display)

    print("Buttons ready! Touch them to interact.")
    print("Press Ctrl+C to exit")

    try:
        while True:
            touch_data = touch.get_touch()

            if touch_data:
                x, y, pressure = touch_data

                # Check which button was touched
                for i, btn in enumerate(buttons):
                    if btn.contains_point(x, y):
                        print(f"Button {i+1} pressed!")

                        # Visual feedback
                        btn.on_press()
                        btn.draw(display)

                        # Wait for release
                        while touch.is_touched():
                            time.sleep(0.05)

                        # Release
                        btn.on_release()
                        btn.draw(display)

                        break

            time.sleep(0.05)

    except KeyboardInterrupt:
        display.fill(BLACK)
        backlight.value(0)
        print("\nDone!")

if __name__ == "__main__":
    main()
