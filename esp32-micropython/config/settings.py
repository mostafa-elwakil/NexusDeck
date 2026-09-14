# ESP32-2432S028 StreamDeck - Configuration
# عدّل هذه الإعدادات حسب شبكتك

# WiFi Settings
WIFI_SSID = "YOUR_WIFI_SSID"          # اسم شبكة الـ WiFi
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"  # كلمة مرور الـ WiFi

# Server Settings
SERVER_URL = "http://192.168.1.100:8765"  # عنوان الكمبيوتر والسيرفر
SYNC_INTERVAL = 5000  # مدة المزامنة (ميللي ثانية)

# StreamDeck Layout
GRID_COLS = 4  # عدد الأعمدة
GRID_ROWS = 3  # عدد الصفوف
TOTAL_BUTTONS = GRID_COLS * GRID_ROWS

# Connection Settings
CONNECT_TIMEOUT = 10  # ثواني
MAX_RETRIES = 3
RETRY_DELAY = 2  # ثواني

# Display Settings (ESP32-2432S028: ILI9341 240×320)
BRIGHTNESS = 255  # 0-255 (backlight)
ROTATION = 1      # 0=Portrait 240×320, 1=Landscape 320×240 (StreamDeck default)
                  # 2=Portrait inverted, 3=Landscape inverted
