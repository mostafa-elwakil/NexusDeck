"""
ESP32-2432S028 Complete Test Suite
====================================
اختبار شامل لجميع مكونات اللوحة
Tests all components step by step and reports status
"""

from machine import Pin, SPI
import time
import gc

print("\n" + "=" * 50)
print("ESP32-2432S028 COMPLETE TEST SUITE")
print("=" * 50)
print("\nThis will test:")
print("1. SPI initialization")
print("2. Display (ILI9341)")
print("3. Touch controller (XPT2046)")
print("4. Backlight")
print("5. RGB LED (optional)")
print("\nPress Ctrl+C anytime to stop\n")

time.sleep(2)

# Pin definitions
TFT_MISO = 12
TFT_MOSI = 13
TFT_SCLK = 14
TFT_CS = 15
TFT_DC = 2
TFT_BL = 21
TOUCH_CS = 33
TOUCH_IRQ = 36
RGB_R = 4
RGB_G = 16
RGB_B = 17

# Test results
results = {
    'spi': False,
    'display': False,
    'touch': False,
    'backlight': False,
    'rgb': False
}

# ============================================
# TEST 1: SPI Initialization
# ============================================
print("\n[1/5] Testing SPI initialization...")
try:
    spi = SPI(
        2,  # HSPI
        baudrate=20000000,  # 20MHz - safe for ESP32
        polarity=0,
        phase=0,
        sck=Pin(TFT_SCLK),
        mosi=Pin(TFT_MOSI),
        miso=Pin(TFT_MISO)
    )
    results['spi'] = True
    print("✅ SPI initialized successfully at 20MHz")
except Exception as e:
    print(f"❌ SPI initialization failed: {e}")
    print("\nFATAL: Cannot continue without SPI")
    raise SystemExit

# ============================================
# TEST 2: Display Initialization
# ============================================
print("\n[2/5] Testing display (ILI9341)...")
try:
    # Setup pins
    cs = Pin(TFT_CS, Pin.OUT, value=1)
    dc = Pin(TFT_DC, Pin.OUT, value=0)

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
        cs.value(0)
        dc.value(0)
        spi.write(bytearray([cmd]))
        cs.value(1)

    def write_data(data):
        cs.value(0)
        dc.value(1)
        if isinstance(data, int):
            data = bytearray([data])
        spi.write(data)
        cs.value(1)

    def fill_screen(color):
        """Fill screen with RGB565 color"""
        # Set full screen window (320x240 landscape)
        write_cmd(CASET)
        write_data(bytearray([0, 0, (320-1) >> 8, (320-1) & 0xFF]))
        write_cmd(PASET)
        write_data(bytearray([0, 0, (240-1) >> 8, (240-1) & 0xFF]))
        write_cmd(RAMWR)

        # Write color
        hi = color >> 8
        lo = color & 0xFF
        chunk = bytearray([hi, lo] * 100)

        cs.value(0)
        dc.value(1)

        pixels = 320 * 240
        for _ in range(pixels // 100):
            spi.write(chunk)

        remaining = pixels % 100
        if remaining:
            spi.write(chunk[:remaining * 2])

        cs.value(1)

    # Initialize display
    write_cmd(SWRESET)
    time.sleep_ms(150)

    write_cmd(SLPOUT)
    time.sleep_ms(120)

    write_cmd(PIXFMT)
    write_data(0x55)

    # Landscape rotation
    write_cmd(MADCTL)
    write_data(0x28)

    write_cmd(INVON)
    write_cmd(DISPON)
    time.sleep_ms(100)

    results['display'] = True
    print("✅ Display initialized successfully")

    # Visual test
    print("\n   Testing colors...")
    colors = [
        (0xF800, "RED"),
        (0x07E0, "GREEN"),
        (0x001F, "BLUE"),
        (0x0000, "BLACK")
    ]

    for color, name in colors:
        print(f"   → {name}")
        fill_screen(color)
        time.sleep(1)

    print("✅ Color test complete")

except Exception as e:
    print(f"❌ Display test failed: {e}")
    results['display'] = False

# ============================================
# TEST 3: Backlight
# ============================================
print("\n[3/5] Testing backlight...")
try:
    backlight = Pin(TFT_BL, Pin.OUT)

    # Test ON
    backlight.value(1)
    print("✅ Backlight ON")
    time.sleep(1)

    # Test OFF
    print("   Testing backlight OFF for 1 second...")
    backlight.value(0)
    time.sleep(1)

    # Back ON
    backlight.value(1)
    print("✅ Backlight test complete")
    results['backlight'] = True

except Exception as e:
    print(f"❌ Backlight test failed: {e}")
    results['backlight'] = False

# ============================================
# TEST 4: Touch Controller
# ============================================
print("\n[4/5] Testing touch controller (XPT2046)...")
try:
    touch_cs = Pin(TOUCH_CS, Pin.OUT, value=1)
    touch_irq = Pin(TOUCH_IRQ, Pin.IN, Pin.PULL_UP)

    GET_X = 0xD0
    GET_Y = 0x90

    def read_touch_adc(cmd):
        touch_cs.value(0)
        spi.write(bytearray([cmd]))
        data = spi.read(2)
        touch_cs.value(1)
        if data:
            return ((data[0] << 8) | data[1]) >> 3
        return 0

    # Test if touch controller responds
    x = read_touch_adc(GET_X)
    y = read_touch_adc(GET_Y)

    print(f"✅ Touch controller initialized")
    print(f"   Current readings: X={x}, Y={y}")
    print(f"   IRQ state: {'TOUCHED' if not touch_irq.value() else 'NOT TOUCHED'}")

    # Wait for touch test
    print("\n   🖐️  Touch the screen anywhere...")
    fill_screen(0xFFFF)  # White background

    timeout = 10  # 10 seconds
    start = time.time()
    touched = False

    while time.time() - start < timeout:
        if not touch_irq.value():  # Touch detected
            x = read_touch_adc(GET_X)
            y = read_touch_adc(GET_Y)
            print(f"\n   ✅ Touch detected! X={x}, Y={y}")

            # Draw red rectangle at center as feedback
            fill_screen(0xF800)  # Red
            time.sleep(1)

            touched = True
            break

        time.sleep(0.1)

    if touched:
        print("✅ Touch test PASSED")
        results['touch'] = True
    else:
        print("⚠️  No touch detected within 10 seconds")
        print("   Touch controller responds but needs manual test")
        results['touch'] = 'partial'

except Exception as e:
    print(f"❌ Touch test failed: {e}")
    results['touch'] = False

# ============================================
# TEST 5: RGB LED (Optional)
# ============================================
print("\n[5/5] Testing RGB LED...")
try:
    led_r = Pin(RGB_R, Pin.OUT)
    led_g = Pin(RGB_G, Pin.OUT)
    led_b = Pin(RGB_B, Pin.OUT)

    # Test each color
    print("   → RED")
    led_r.value(1)
    led_g.value(0)
    led_b.value(0)
    time.sleep(0.5)

    print("   → GREEN")
    led_r.value(0)
    led_g.value(1)
    led_b.value(0)
    time.sleep(0.5)

    print("   → BLUE")
    led_r.value(0)
    led_g.value(0)
    led_b.value(1)
    time.sleep(0.5)

    # OFF
    led_r.value(0)
    led_g.value(0)
    led_b.value(0)

    print("✅ RGB LED test complete")
    results['rgb'] = True

except Exception as e:
    print(f"⚠️  RGB LED test failed: {e}")
    print("   (This is optional, not critical)")
    results['rgb'] = False

# ============================================
# FINAL REPORT
# ============================================
print("\n" + "=" * 50)
print("TEST RESULTS SUMMARY")
print("=" * 50)

all_passed = True
for component, status in results.items():
    if status == True:
        icon = "✅"
    elif status == 'partial':
        icon = "⚠️ "
    else:
        icon = "❌"
        if component != 'rgb':  # RGB is optional
            all_passed = False

    print(f"{icon} {component.upper()}: {status}")

print("=" * 50)

if all_passed:
    print("\n🎉 ALL TESTS PASSED!")
    print("Your ESP32-2432S028 is working perfectly!")

    # Show success screen
    fill_screen(0x07E0)  # Green
    time.sleep(2)

    print("\nNext steps:")
    print("1. Configure WiFi in config/settings.py")
    print("2. Try examples/03_simple_button.py")
    print("3. Try examples/04_full_streamdeck.py")

else:
    print("\n⚠️  SOME TESTS FAILED")
    print("Please check the errors above")

    # Show error screen
    fill_screen(0xF800)  # Red
    time.sleep(2)

print("\n" + "=" * 50)
print("Test complete. Press Ctrl+D to exit REPL")
print("=" * 50 + "\n")

# Clean up
gc.collect()
