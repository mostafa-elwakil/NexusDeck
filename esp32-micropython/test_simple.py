"""
اختبار بسيط للشاشة - بدون WiFi (SPI Fixed)
Simple display test - No WiFi required
Fixed SPI baudrate to 20MHz (within ESP32 limits)
"""

from machine import Pin, SPI
import time

# Pin definitions (ESP32-2432S028)
TFT_MISO = 12
TFT_MOSI = 13
TFT_SCLK = 14
TFT_CS = 15
TFT_DC = 2
TFT_BL = 21

print("=" * 40)
print("ESP32-2432S028 Simple Test")
print("Testing display without WiFi")
print("=" * 40)

# Initialize SPI with LOWER baudrate (20MHz is safe for ESP32)
print("\n1. Initializing SPI...")
spi = SPI(
    2,  # HSPI
    baudrate=20000000,  # 20MHz (was 40MHz - TOO HIGH!)
    polarity=0,
    phase=0,
    sck=Pin(TFT_SCLK),
    mosi=Pin(TFT_MOSI),
    miso=Pin(TFT_MISO)
)
print("SPI ready!")

# Initialize pins
print("\n2. Initializing pins...")
cs = Pin(TFT_CS, Pin.OUT, value=1)
dc = Pin(TFT_DC, Pin.OUT, value=0)
backlight = Pin(TFT_BL, Pin.OUT)

# Turn on backlight
backlight.value(1)
print("Backlight ON!")

# ILI9341 Commands
SWRESET = 0x01
SLPOUT = 0x11
DISPON = 0x29
CASET = 0x2A
PASET = 0x2B
RAMWR = 0x2C
MADCTL = 0x36
PIXFMT = 0x3A
INVON = 0x21

def write_cmd(cmd):
    """Send command to display"""
    cs.value(0)
    dc.value(0)
    spi.write(bytearray([cmd]))
    cs.value(1)

def write_data(data):
    """Send data to display"""
    cs.value(0)
    dc.value(1)
    if isinstance(data, int):
        data = bytearray([data])
    spi.write(data)
    cs.value(1)

print("\n3. Initializing display...")

# Software reset
write_cmd(SWRESET)
time.sleep_ms(150)

# Sleep out
write_cmd(SLPOUT)
time.sleep_ms(120)

# Pixel format: 16-bit
write_cmd(PIXFMT)
write_data(0x55)

# Rotation: Landscape (320x240)
write_cmd(MADCTL)
write_data(0x28)

# Invert display
write_cmd(INVON)

# Display ON
write_cmd(DISPON)
time.sleep_ms(100)

print("Display initialized!")

# Fill screen with RED
print("\n4. Drawing RED screen...")

# Set window to full screen (320x240)
write_cmd(CASET)  # Column
write_data(bytearray([0, 0, (320-1) >> 8, (320-1) & 0xFF]))

write_cmd(PASET)  # Row
write_data(bytearray([0, 0, (240-1) >> 8, (240-1) & 0xFF]))

write_cmd(RAMWR)  # Write to RAM

# RED color (RGB565: 0xF800)
red_chunk = bytearray([0xF8, 0x00] * 100)  # 100 red pixels

cs.value(0)
dc.value(1)

pixels = 320 * 240
for i in range(pixels // 100):
    spi.write(red_chunk)
    if i % 100 == 0:
        print(".", end="")

# Remaining pixels
remaining = pixels % 100
if remaining:
    spi.write(red_chunk[:remaining * 2])

cs.value(1)

print("\nRED screen drawn!")
print("\n" + "=" * 40)
print("SUCCESS! Screen should be RED now")
print("=" * 40)

# Wait 2 seconds, then GREEN
time.sleep(2)

print("\nDrawing GREEN screen...")

write_cmd(CASET)
write_data(bytearray([0, 0, (320-1) >> 8, (320-1) & 0xFF]))
write_cmd(PASET)
write_data(bytearray([0, 0, (240-1) >> 8, (240-1) & 0xFF]))
write_cmd(RAMWR)

# GREEN color (RGB565: 0x07E0)
green_chunk = bytearray([0x07, 0xE0] * 100)

cs.value(0)
dc.value(1)

for i in range(pixels // 100):
    spi.write(green_chunk)

if remaining:
    spi.write(green_chunk[:remaining * 2])

cs.value(1)

print("GREEN screen drawn!")

# Wait 2 seconds, then BLUE
time.sleep(2)

print("\nDrawing BLUE screen...")

write_cmd(CASET)
write_data(bytearray([0, 0, (320-1) >> 8, (320-1) & 0xFF]))
write_cmd(PASET)
write_data(bytearray([0, 0, (240-1) >> 8, (240-1) & 0xFF]))
write_cmd(RAMWR)

# BLUE color (RGB565: 0x001F)
blue_chunk = bytearray([0x00, 0x1F] * 100)

cs.value(0)
dc.value(1)

for i in range(pixels // 100):
    spi.write(blue_chunk)

if remaining:
    spi.write(blue_chunk[:remaining * 2])

cs.value(1)

print("BLUE screen drawn!")
print("\nTest complete! Colors: RED → GREEN → BLUE")
print("\nIf you see colors, display is working!")
print("If still white, try rotation=0 (Portrait mode)")
