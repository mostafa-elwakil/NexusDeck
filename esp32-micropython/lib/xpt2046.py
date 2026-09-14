"""
XPT2046 Touch Controller Driver for MicroPython
Resistive touch screen controller
"""

from machine import Pin, SPI
from time import sleep_ms


class XPT2046:
    """Driver for XPT2046 touch controller"""

    # Commands
    GET_X = 0xD0
    GET_Y = 0x90
    GET_Z1 = 0xB0
    GET_Z2 = 0xC0

    def __init__(self, spi, cs, irq=None, rotation=1,
                 x_min=200, x_max=3700, y_min=240, y_max=3800):
        """
        XPT2046 Touch Controller for ESP32-2432S028
        rotation: match display rotation (0=240×320, 1=320×240, etc)
        Calibration values may need adjustment for your specific unit
        """
        self.spi = spi
        self.cs = cs
        self.irq = irq
        self.rotation = rotation

        # Set width/height based on rotation
        if rotation in [0, 2]:
            self.width, self.height = 240, 320  # Portrait
        else:
            self.width, self.height = 320, 240  # Landscape

        # Calibration values (adjust for your display)
        self.x_min = x_min
        self.x_max = x_max
        self.y_min = y_min
        self.y_max = y_max

        # Setup pins
        self.cs.init(Pin.OUT, value=1)
        if self.irq:
            self.irq.init(Pin.IN, Pin.PULL_UP)

    def _read_adc(self, command):
        """Read ADC value from touch controller"""
        self.cs(0)

        # Send command and read 12-bit result
        self.spi.write(bytearray([command]))
        data = self.spi.read(2)

        self.cs(1)

        if data:
            return ((data[0] << 8) | data[1]) >> 3
        return 0

    def get_raw_touch(self):
        """Get raw touch coordinates"""
        if self.irq and self.irq.value():
            return None  # Not touched

        # Read X and Y coordinates
        x = self._read_adc(self.GET_X)
        y = self._read_adc(self.GET_Y)
        z1 = self._read_adc(self.GET_Z1)

        # Check if valid touch (pressure threshold)
        if z1 < 50:
            return None

        return (x, y, z1)

    def get_touch(self):
        """Get calibrated touch coordinates"""
        raw = self.get_raw_touch()

        if not raw:
            return None

        x_raw, y_raw, pressure = raw

        # Map raw values to screen coordinates
        x = self._map(x_raw, self.x_min, self.x_max, 0, self.width)
        y = self._map(y_raw, self.y_min, self.y_max, 0, self.height)

        # Clamp to screen bounds
        x = max(0, min(self.width - 1, x))
        y = max(0, min(self.height - 1, y))

        return (x, y, pressure)

    def is_touched(self):
        """Check if screen is currently touched"""
        if self.irq:
            return not self.irq.value()
        return self.get_raw_touch() is not None

    def calibrate(self, samples=10):
        """Helper for calibration - prints raw values"""
        print("Touch screen to calibrate...")
        print("Touch top-left, top-right, bottom-left, bottom-right corners")

        readings = []
        while len(readings) < samples:
            raw = self.get_raw_touch()
            if raw:
                readings.append(raw[:2])  # x, y only
                print(f"Sample {len(readings)}: X={raw[0]}, Y={raw[1]}")
                sleep_ms(500)

        # Calculate min/max
        x_vals = [r[0] for r in readings]
        y_vals = [r[1] for r in readings]

        print(f"\nCalibration values:")
        print(f"x_min={min(x_vals)}, x_max={max(x_vals)}")
        print(f"y_min={min(y_vals)}, y_max={max(y_vals)}")

    @staticmethod
    def _map(value, in_min, in_max, out_min, out_max):
        """Map value from one range to another"""
        return int((value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)
