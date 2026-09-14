# ESP32-2432S028 (CYD) Pin Definitions
# تعريفات الـ GPIO للوحة

# TFT Display (ILI9341) - SPI
TFT_MISO = 12
TFT_MOSI = 13
TFT_SCLK = 14
TFT_CS = 15
TFT_DC = 2
TFT_RST = -1  # Not connected, use software reset
TFT_BL = 21   # Backlight

# Touch Controller (XPT2046) - Same SPI bus
TOUCH_CS = 33
TOUCH_IRQ = 36

# RGB LED
RGB_R = 4
RGB_G = 16
RGB_B = 17

# LDR (Light Sensor)
LDR = 34

# SD Card (shared SPI)
SD_CS = 5

# Speaker/Buzzer
SPEAKER = 26

# User Button
USER_BTN = 0  # Boot button
