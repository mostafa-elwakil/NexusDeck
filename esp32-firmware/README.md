# ESP32-2432S028 StreamDeck Firmware

**ربط ESP32-2432S028 (CYD) بمشروع StreamDeck Simulator**

دليل شامل لتحويل جهاز CYD الخاص بك إلى Stream Deck حقيقي مع شاشة لمس تفاعلية.

---

## 📋 المحتويات / Contents

1. [المواصفات التقنية](#المواصفات-التقنية)
2. [المتطلبات](#المتطلبات)
3. [التثبيت](#التثبيت)
4. [الإعداد](#الإعداد)
5. [الاستخدام](#الاستخدام)
6. [استكشاف الأخطاء](#استكشاف-الأخطاء)

---

## 🔧 المواصفات التقنية / Technical Specs

### ESP32-2432S028 (Cheap Yellow Display)

- **المعالج**: ESP32-WROOM-32
- **الشاشة**: ILI9341 2.8" TFT LCD (240x320 pixels)
- **اللمس**: XPT2046 Resistive Touch
- **الذاكرة**: 4MB Flash
- **WiFi**: 802.11 b/g/n
- **الأزرار**: 12 زر (شبكة 4×3)

### Pin Configuration

```
TFT Display (ILI9341):
- MISO: GPIO 12
- MOSI: GPIO 13
- SCLK: GPIO 14
- CS:   GPIO 15
- DC:   GPIO 2
- RST:  -1 (not connected)

Touch (XPT2046):
- CS:   GPIO 33
- IRQ:  GPIO 36
```

---

## 📦 المتطلبات / Requirements

### Hardware
- ESP32-2432S028 (CYD) board
- USB-C cable for programming
- WiFi network

### Software
- **PlatformIO** (recommended) or **Arduino IDE**
- Python 3.8+ (for companion server)
- Modern web browser

### Libraries
- TFT_eSPI (v2.5.43+)
- ArduinoJson (v6.21.3+)
- XPT2046_Touchscreen
- WiFi (built-in)
- HTTPClient (built-in)

---

## 🚀 التثبيت / Installation

### الطريقة 1: PlatformIO (موصى بها)

```bash
# 1. انتقل لمجلد الفيرموير
cd esp32-firmware

# 2. بناء المشروع
pio run

# 3. رفع الفيرموير للـ ESP32
pio run --target upload

# 4. مراقبة Serial Monitor
pio device monitor
```

### الطريقة 2: Arduino IDE

1. **تثبيت ESP32 Board Support:**
   - File → Preferences → Additional Board URLs
   - Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Tools → Board → Boards Manager → Install "ESP32"

2. **تثبيت المكتبات:**
   - Sketch → Include Library → Manage Libraries
   - ثبت: TFT_eSPI, ArduinoJson, XPT2046_Touchscreen

3. **ضبط TFT_eSPI:**
   - افتح مجلد المكتبة: `Arduino/libraries/TFT_eSPI/`
   - عدل ملف `User_Setup.h` أو استخدم `User_Setup_Select.h`
   - فعّل التعريفات الموجودة في `platformio.ini`

4. **فتح وتحميل الكود:**
   - File → Open → `esp32-firmware/src/main.cpp`
   - Tools → Board → "ESP32 Dev Module"
   - Tools → Port → اختر منفذ ESP32
   - اضغط Upload

---

## ⚙️ الإعداد / Configuration

### 1. إعداد WiFi

عدّل الإعدادات في `main.cpp`:

```cpp
// WiFi Configuration
const char* WIFI_SSID = "YOUR_WIFI_SSID";      // اسم شبكة WiFi
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"; // كلمة المرور

// Server Configuration
const char* SERVER_URL = "http://192.168.1.100:8765"; // IP جهاز الكمبيوتر
```

### 2. معرفة IP الكمبيوتر

**على Windows:**
```cmd
ipconfig
# ابحث عن "IPv4 Address" تحت "Wireless LAN adapter WiFi"
```

**على Linux/Mac:**
```bash
ifconfig
# أو
ip addr show
```

### 3. تشغيل السيرفر المساعد

```bash
cd streamdeck-simulator/server
pip install -r requirements.txt
python server.py
```

يجب أن ترى:
```
========================================
StreamDeck Companion Server
========================================
Server: http://0.0.0.0:8765
Server is running...
```

---

## 🎮 الاستخدام / Usage

### بدء التشغيل

1. **شغّل السيرفر المساعد** على الكمبيوتر
2. **وصّل ESP32** بالطاقة (USB أو خارجي)
3. **انتظر الاتصال بالـ WiFi**
   - سترى شاشة "Connecting to WiFi..."
   - بعد الاتصال: "WiFi Connected!"
   - سيظهر IP Address على الشاشة

4. **افتح واجهة الويب**
   ```
   cd streamdeck-simulator
   # افتح index.html في المتصفح
   ```

### إنشاء ملف تعريف / Creating a Profile

1. **في واجهة الويب:**
   - اضغط زر "Studio" أو `Ctrl+E`
   - عدّل الأزرار حسب رغبتك
   - اضغط "Save" أو `Ctrl+S`

2. **المزامنة التلقائية:**
   - ESP32 يسحب التعديلات كل 5 ثواني
   - الأزرار ستتحدث تلقائياً على الشاشة

### استخدام الأزرار

- **المس زر** على الشاشة لتنفيذ إجراءه
- **حالات الأزرار:**
  - 🔵 **خامل (Idle)**: اللون الأساسي
  - 🟡 **مضغوط (Pressed)**: لون أغمق
  - 🔄 **قيد التنفيذ (Running)**: وميض
  - 🟢 **نجح (Success)**: أخضر
  - 🔴 **فشل (Error)**: أحمر

### مؤشر الحالة / Status Indicator

دائرة صغيرة في أعلى يمين الشاشة:

- 🟢 **أخضر**: متصل بالسيرفر
- 🟡 **أصفر**: WiFi متصل، السيرفر غير متاح
- 🔴 **أحمر**: WiFi غير متصل

---

## 🔍 استكشاف الأخطاء / Troubleshooting

### المشكلة: ESP32 لا يتصل بالـ WiFi

**الحلول:**
```cpp
// 1. تأكد من صحة SSID والباسورد
const char* WIFI_SSID = "YourNetworkName";
const char* WIFI_PASSWORD = "YourPassword";

// 2. جرب إعادة تشغيل ESP32

// 3. تأكد من أن WiFi على نطاق 2.4GHz (ESP32 لا يدعم 5GHz)
```

### المشكلة: الشاشة بيضاء أو فارغة

**الحلول:**
1. تأكد من توصيلات الشاشة صحيحة
2. راجع pin definitions في الكود
3. جرب rotation مختلف:
   ```cpp
   tft.setRotation(0); // جرب 0, 1, 2, 3
   ```

### المشكلة: اللمس لا يعمل

**الحلول:**
```cpp
// 1. معايرة إحداثيات اللمس
uint16_t x = map(p.x, 200, 3700, 0, SCREEN_WIDTH);
uint16_t y = map(p.y, 240, 3800, 0, SCREEN_HEIGHT);

// 2. اضبط القيم حسب شاشتك
// للحصول على القيم الصحيحة:
// - شغل Serial Monitor
// - المس زوايا الشاشة
// - اقرأ القيم الخام (raw values)
```

### المشكلة: ESP32 متصل لكن لا يتزامن

**الحلول:**
1. تأكد من تشغيل السيرفر:
   ```bash
   python server.py
   ```

2. تأكد من IP صحيح:
   ```cpp
   const char* SERVER_URL = "http://192.168.1.X:8765";
   ```

3. تأكد من Firewall لا يحجب Port 8765:
   ```cmd
   # Windows
   netsh advfirewall firewall add rule name="StreamDeck" dir=in action=allow protocol=TCP localport=8765
   ```

### المشكلة: الأزرار لا تنفذ الإجراءات

**الحلول:**
1. تأكد من تكوين action صحيح في الويب
2. راجع Serial Monitor للأخطاء:
   ```bash
   pio device monitor
   ```
3. تأكد من server endpoint متاح:
   ```
   http://YOUR_PC_IP:8765/api/execute-action
   ```

---

## 📊 مثال على Profile للـ ESP32

```json
{
  "name": "ESP32 Profile",
  "size": "cyd",
  "buttons": [
    {
      "label": "VS Code",
      "icon": "💻",
      "color": "#007acc",
      "action": {
        "type": "open_app",
        "data": "{\"app\":\"code\"}"
      }
    },
    {
      "label": "",
      "icon": "🕐",
      "color": "#1a1a3e",
      "widget": {
        "type": "clock"
      }
    }
  ]
}
```

---

## 🔌 Pin-out Reference

```
ESP32-2432S028 Full Pinout:

Display Controller (ILI9341):
┌─────────────────────────────┐
│ Pin  │ GPIO │ Function      │
├──────┼──────┼───────────────┤
│ SCK  │  14  │ SPI Clock     │
│ MOSI │  13  │ SPI Data Out  │
│ MISO │  12  │ SPI Data In   │
│ CS   │  15  │ Chip Select   │
│ DC   │   2  │ Data/Command  │
│ RST  │  -1  │ Not Connected │
│ BL   │  21  │ Backlight     │
└──────┴──────┴───────────────┘

Touch Controller (XPT2046):
┌─────────────────────────────┐
│ Pin  │ GPIO │ Function      │
├──────┼──────┼───────────────┤
│ CS   │  33  │ Chip Select   │
│ IRQ  │  36  │ Interrupt     │
└──────┴──────┴───────────────┘

Additional:
- LED: GPIO 4
- Boot Button: GPIO 0
- RGB LED: GPIO 17 (R), 4 (G), 16 (B)
```

---

## 🎨 تخصيص الشبكة / Grid Customization

لتغيير حجم الشبكة، عدّل في `main.cpp`:

```cpp
// للحصول على 3x2 (6 أزرار)
#define GRID_COLS 3
#define GRID_ROWS 2

// أو 5x3 (15 زر) - قد تكون الأزرار صغيرة
#define GRID_COLS 5
#define GRID_ROWS 3
```

---

## 📱 Serial Monitor Commands

عند الاتصال بـ Serial Monitor (115200 baud):

```
=================================
StreamDeck ESP32-2432S028 Firmware
=================================

Initializing display...
Display initialized!
Initializing touch screen...
Touch screen initialized!
Connecting to WiFi: YourNetwork
....
WiFi connected!
IP Address: 192.168.1.150
Setup complete!

Button pressed: 0
Action executed successfully
Profile synced successfully
```

---

## 🔄 تحديث الفيرموير / Firmware Update

### عبر USB:
```bash
pio run --target upload
```

### عبر OTA (Over-The-Air) - قريباً:
```bash
# سيتم إضافة دعم OTA في إصدار قادم
```

---

## 🌟 مميزات إضافية قادمة / Upcoming Features

- [ ] دعم NTP للساعة الحقيقية
- [ ] دعم MQTT للتحكم عن بعد
- [ ] حفظ البروفايلات محلياً على ESP32
- [ ] وضع Standalone (بدون كمبيوتر)
- [ ] دعم OTA Updates
- [ ] اختيار سطوع الشاشة
- [ ] Sleep mode لتوفير الطاقة
- [ ] دعم أزرار فيزيائية خارجية

---

## 📞 الدعم / Support

للمشاكل التقنية:
1. راجع قسم [استكشاف الأخطاء](#استكشاف-الأخطاء)
2. افتح Issue على GitHub
3. تحقق من Serial Monitor للأخطاء

---

## 📄 الملفات / Files Structure

```
esp32-firmware/
├── platformio.ini          # تكوين PlatformIO
├── src/
│   └── main.cpp           # الكود الرئيسي
├── include/               # Header files (فارغ حالياً)
├── lib/                   # مكتبات خارجية (فارغ حالياً)
└── README.md             # هذا الملف
```

---

## ⚡ نصائح الأداء / Performance Tips

1. **خفض معدل المزامنة** إذا كان الأداء بطيء:
   ```cpp
   const int SYNC_INTERVAL = 10000; // 10 ثواني بدلاً من 5
   ```

2. **تعطيل الويدجت الحية** المعقدة على ESP32:
   ```cpp
   // الويدجت مثل CPU/RAM تحتاج السيرفر
   // استخدم فقط الويدجت البسيطة: clock, uptime
   ```

3. **استخدام ألوان RGB565** للأداء الأفضل

---

## 🙏 شكر وتقدير / Credits

- **Bodmer** - مكتبة TFT_eSPI الرائعة
- **ESP32 Community** - الدعم والمساعدة
- **CYD Community** - مواصفات الجهاز

---

**استمتع بـ ESP32 StreamDeck! 🎮**

**Enjoy your ESP32 StreamDeck! 🎮**
