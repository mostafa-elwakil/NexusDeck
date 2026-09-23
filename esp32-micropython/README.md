# ESP32-2432S028 NexusDeck - MicroPython Version

> نسخة MicroPython من مشروع NexusDeck للوحة ESP32-2432S028 (CYD)

---

## 📋 المحتويات / Contents

- [المتطلبات](#-المتطلبات--requirements)
- [التثبيت](#-التثبيت--installation)
- [البنية](#-البنية--structure)
- [الاستخدام](#-الاستخدام--usage)
- [الأمثلة](#-الأمثلة--examples)

---

## 🔧 المتطلبات / Requirements

### الأجهزة / Hardware
- ESP32-2432S028 (Cheap Yellow Display)
- كابل USB-C للبرمجة والطاقة

### البرمجيات / Software
- Python 3.7+ (للأدوات)
- [esptool](https://github.com/espressif/esptool) لرفع الـ firmware
- [ampy](https://github.com/scientifichackers/ampy) أو [mpremote](https://github.com/micropython/micropython/tree/master/tools/mpremote) لنقل الملفات

```bash
pip install esptool adafruit-ampy mpremote
```

---

## 📥 التثبيت / Installation

### الخطوة 1: تحميل MicroPython Firmware

قم بتحميل أحدث firmware من:
- [MicroPython ESP32 Downloads](https://micropython.org/download/esp32/)
- اختر: **ESP32 with SPIRAM** (Generic)

### الخطوة 2: مسح الـ Flash

```bash
esptool.py --chip esp32 --port COM3 erase_flash
```

> غيّر `COM3` إلى المنفذ الصحيح:
> - **Windows**: `COM3`, `COM4`, إلخ
> - **Linux/Mac**: `/dev/ttyUSB0`, `/dev/ttyACM0`

### الخطوة 3: رفع MicroPython

```bash
esptool.py --chip esp32 --port COM3 write_flash -z 0x1000 esp32-*.bin
```

### الخطوة 4: نقل ملفات المشروع

#### باستخدام mpremote (موصى به):
```bash
cd esp32-micropython
mpremote connect COM3 cp -r lib :
mpremote connect COM3 cp config/settings.py :
mpremote connect COM3 cp main.py :
```

#### باستخدام ampy:
```bash
cd esp32-micropython
ampy --port COM3 put lib
ampy --port COM3 put config/settings.py
ampy --port COM3 put main.py
```

---

## 📁 البنية / Structure

```
esp32-micropython/
├── main.py                 # نقطة البداية الرئيسية
├── config/
│   ├── settings.py        # إعدادات WiFi والسيرفر
│   └── pins.py            # تعريفات الـ GPIO
├── lib/
│   ├── ili9341.py         # Driver للشاشة ILI9341
│   ├── xpt2046.py         # Driver للمس XPT2046
│   ├── button.py          # كلاس الأزرار
│   ├── streamdeck.py      # المنطق الرئيسي
│   └── utils.py           # دوال مساعدة
├── examples/
│   ├── 01_test_display.py
│   ├── 02_test_touch.py
│   ├── 03_simple_button.py
│   └── 04_full_streamdeck.py
└── README.md
```

---

## 🚀 الاستخدام / Usage

### 1. تكوين الإعدادات

عدّل `config/settings.py`:

```python
# WiFi
WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_PASSWORD"

# Server
SERVER_URL = "http://192.168.1.X:8765"
SYNC_INTERVAL = 5000  # milliseconds
```

### 2. تشغيل المشروع

بعد رفع الملفات، اضغط زر **Reset** على اللوحة، أو:

```bash
mpremote connect COM3 reset
```

### 3. مراقبة الـ Output

```bash
# mpremote
mpremote connect COM3 repl

# ampy
# استخدم أي serial monitor مثل PuTTY أو screen
screen /dev/ttyUSB0 115200
```

---

## 🎯 الأمثلة / Examples

### مثال 1: اختبار الشاشة

```bash
mpremote connect COM3 run examples/01_test_display.py
```

يعرض مستطيلات ملونة للتأكد من عمل الشاشة.

### مثال 2: اختبار اللمس

```bash
mpremote connect COM3 run examples/02_test_touch.py
```

يطبع إحداثيات اللمس على Serial Monitor.

### مثال 3: زر بسيط

```bash
mpremote connect COM3 run examples/03_simple_button.py
```

يعرض 4 أزرار تفاعلية.

### مثال 4: StreamDeck كامل

```bash
mpremote connect COM3 run examples/04_full_streamdeck.py
```

StreamDeck متكامل مع مزامنة السيرفر.

---

## 🔍 استكشاف الأخطاء / Troubleshooting

### المشكلة: الشاشة بيضاء

**الحل:**
1. تحقق من تثبيت MicroPython بشكل صحيح
2. راجع pin definitions في `config/pins.py`
3. تأكد من وجود `ili9341.py` في مجلد `lib/`

### المشكلة: اللمس لا يعمل

**الحل:**
1. عايِر إحداثيات اللمس في `lib/xpt2046.py`
2. جرب مثال `02_test_touch.py` لقراءة القيم الخام

### المشكلة: لا يتصل بالـ WiFi

**الحل:**
1. تحقق من SSID والباسورد
2. ESP32 يدعم فقط 2.4GHz
3. راجع Serial Monitor للأخطاء

---

## 📊 مقارنة: MicroPython vs C++/Arduino

| الميزة | MicroPython ✅ | C++/Arduino |
|-------|----------------|-------------|
| سهولة البرمجة | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |
| السرعة | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| استهلاك الذاكرة | أعلى | أقل |
| التطوير السريع | نعم | لا |
| Debugging | أسهل (REPL) | أصعب |

**متى تستخدم MicroPython؟**
- ✅ التجربة السريعة (Prototyping)
- ✅ التعلم والتجربة
- ✅ المشاريع البسيطة
- ✅ التطوير التفاعلي

**متى تستخدم C++/Arduino؟**
- ✅ المشاريع الإنتاجية
- ✅ الأداء العالي
- ✅ استهلاك أقل للذاكرة
- ✅ المكتبات الجاهزة الكثيرة

---

## 🎓 موارد إضافية / Resources

- [MicroPython Docs](https://docs.micropython.org/)
- [ESP32 MicroPython Guide](https://docs.micropython.org/en/latest/esp32/quickref.html)
- [ILI9341 Datasheet](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)
- [XPT2046 Datasheet](https://www.buydisplay.com/download/ic/XPT2046.pdf)

---

## 🤝 المساهمة / Contributing

لديك تحسينات؟ افتح Issue أو Pull Request!

---

**Created by DarkAI Team 🎮**
**Date: 2026-09-12**
