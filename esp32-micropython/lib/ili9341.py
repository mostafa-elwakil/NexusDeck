"""
ILI9341 TFT Display Driver for MicroPython
Optimized for ESP32-2432S028
"""

from machine import Pin, SPI
from time import sleep_ms
import framebuf

# ILI9341 Commands
ILI9341_NOP = 0x00
ILI9341_SWRESET = 0x01
ILI9341_SLPOUT = 0x11
ILI9341_DISPON = 0x29
ILI9341_CASET = 0x2A
ILI9341_PASET = 0x2B
ILI9341_RAMWR = 0x2C
ILI9341_MADCTL = 0x36
ILI9341_PIXFMT = 0x3A
ILI9341_INVON = 0x21

# Colors (RGB565)
BLACK = 0x0000
WHITE = 0xFFFF
RED = 0xF800
GREEN = 0x07E0
BLUE = 0x001F
CYAN = 0x07FF
MAGENTA = 0xF81F
YELLOW = 0xFFE0
ORANGE = 0xFD20


class ILI9341:
    """Driver for ILI9341 TFT display"""

    def __init__(self, spi, cs, dc, rst=None, rotation=1):
        """
        ESP32-2432S028 has 240x320 ILI9341 display
        rotation=0: 240x320 (Portrait)
        rotation=1: 320x240 (Landscape) - Default for StreamDeck
        rotation=2: 240x320 (Portrait, inverted)
        rotation=3: 320x240 (Landscape, inverted)
        """
        self.spi = spi
        self.cs = cs
        self.dc = dc
        self.rst = rst
        self.rotation = rotation

        # Width/height will be set by set_rotation()
        self.width = 0
        self.height = 0

        # Setup pins
        self.cs.init(Pin.OUT, value=1)
        self.dc.init(Pin.OUT, value=0)
        if self.rst:
            self.rst.init(Pin.OUT, value=1)

        # Initialize display
        self.init_display()

    def write_cmd(self, cmd):
        """Write command to display"""
        self.cs(0)
        self.dc(0)
        self.spi.write(bytearray([cmd]))
        self.cs(1)

    def write_data(self, data):
        """Write data to display"""
        self.cs(0)
        self.dc(1)
        if isinstance(data, int):
            data = bytearray([data])
        self.spi.write(data)
        self.cs(1)

    def init_display(self):
        """Initialize the display"""
        # Hardware reset
        if self.rst:
            self.rst(0)
            sleep_ms(10)
            self.rst(1)
            sleep_ms(120)

        # Software reset
        self.write_cmd(ILI9341_SWRESET)
        sleep_ms(150)

        # Sleep out
        self.write_cmd(ILI9341_SLPOUT)
        sleep_ms(120)

        # Pixel format: 16-bit color
        self.write_cmd(ILI9341_PIXFMT)
        self.write_data(0x55)

        # Set rotation
        self.set_rotation(self.rotation)

        # Invert display
        self.write_cmd(ILI9341_INVON)

        # Display on
        self.write_cmd(ILI9341_DISPON)
        sleep_ms(100)

    def set_rotation(self, rotation):
        """
        Set display rotation for ESP32-2432S028 (ILI9341 240x320)
        rotation=0: 240x320 Portrait (USB at bottom)
        rotation=1: 320x240 Landscape (USB at right) - StreamDeck default
        rotation=2: 240x320 Portrait inverted (USB at top)
        rotation=3: 320x240 Landscape inverted (USB at left)
        """
        self.rotation = rotation % 4
        self.write_cmd(ILI9341_MADCTL)

        if self.rotation == 0:
            # Portrait: 240w × 320h
            self.write_data(0x48)
            self.width, self.height = 240, 320
        elif self.rotation == 1:
            # Landscape: 320w × 240h (default for StreamDeck)
            self.write_data(0x28)
            self.width, self.height = 320, 240
        elif self.rotation == 2:
            # Portrait inverted: 240w × 320h
            self.write_data(0x88)
            self.width, self.height = 240, 320
        elif self.rotation == 3:
            # Landscape inverted: 320w × 240h
            self.write_data(0xE8)
            self.width, self.height = 320, 240

    def set_window(self, x0, y0, x1, y1):
        """Set drawing window"""
        # Column address
        self.write_cmd(ILI9341_CASET)
        self.write_data(bytearray([x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF]))

        # Page address
        self.write_cmd(ILI9341_PASET)
        self.write_data(bytearray([y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF]))

        # Write to RAM
        self.write_cmd(ILI9341_RAMWR)

    def fill(self, color):
        """Fill entire screen with color"""
        self.fill_rect(0, 0, self.width, self.height, color)

    def fill_rect(self, x, y, w, h, color):
        """Fill rectangle with color"""
        self.set_window(x, y, x + w - 1, y + h - 1)

        # Convert color to bytes
        hi = color >> 8
        lo = color & 0xFF
        chunk = bytearray([hi, lo] * 32)  # 32 pixels at a time

        # Write color data
        self.cs(0)
        self.dc(1)

        pixels = w * h
        while pixels > 32:
            self.spi.write(chunk)
            pixels -= 32

        if pixels > 0:
            self.spi.write(chunk[:pixels * 2])

        self.cs(1)

    def pixel(self, x, y, color):
        """Draw single pixel"""
        if 0 <= x < self.width and 0 <= y < self.height:
            self.set_window(x, y, x, y)
            self.write_data(bytearray([color >> 8, color & 0xFF]))

    def hline(self, x, y, w, color):
        """Draw horizontal line"""
        self.fill_rect(x, y, w, 1, color)

    def vline(self, x, y, h, color):
        """Draw vertical line"""
        self.fill_rect(x, y, 1, h, color)

    def rect(self, x, y, w, h, color):
        """Draw rectangle outline"""
        self.hline(x, y, w, color)
        self.hline(x, y + h - 1, w, color)
        self.vline(x, y, h, color)
        self.vline(x + w - 1, y, h, color)

    def text(self, text, x, y, color, scale=1):
        """Draw text (basic 8x8 font)"""
        # Simple implementation - you can add better fonts later
        for i, char in enumerate(text):
            self.char(char, x + i * 8 * scale, y, color, scale)

    def char(self, char, x, y, color, scale=1):
        """Draw single character"""
        # Basic 8x8 font - simplified implementation
        # For production, use a proper font module
        pass

    def rgb565(self, r, g, b):
        """Convert RGB888 to RGB565"""
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
