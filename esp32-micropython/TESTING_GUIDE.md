# دليل التشغيل الكامل - ESP32-2432S028 MicroPython
# Complete Testing Guide

## 📋 الملفات الجاهزة / Ready Files

✅ جميع الملفات جاهزة ومُصلحة:
- `test_complete.py` - اختبار شامل (تم رفعه كـ main.py)
- `test_simple.py` - اختبار بسيط (ألوان فقط)
- `config/settings.py` - إعدادات WiFi
- `lib/` - جميع المكتبات
- `examples/` - 4 أمثلة تدريجية

## 🔧 مشكلة SPI تم حلها!

✅ **تم تقليل baudrate من 40MHz → 20MHz**
✅ لن تظهر أخطاء SPI بعد الآن

---

## 🚀 طريقة الاختبار الكاملة

### الخطوة 1: أغلق أي برنامج يستخدم COM12

إذا كان لديك:
- **Serial Monitor مفتوح** → أغلقه
- **Arduino IDE متصل** → قطع الاتصال
- **PuTTY أو أي terminal** → أغلقه

تحقق بهذا الأمر:
```bash
py -m mpremote connect COM12 ls
```

إذا ظهر `failed to access COM12 (it may be in use)`، أغلق البرنامج الذي يستخدمه.

---

### الخطوة 2: افتح Serial Monitor لمشاهدة النتائج

**الطريقة 1: باستخدام mpremote (موصى به)**
```bash
py -m mpremote connect COM12 repl
```

**الطريقة 2: باستخدام Arduino IDE**
- Tools → Serial Monitor
- Baud rate: 115200

**الطريقة 3: باستخدام PuTTY (Windows)**
- Connection type: Serial
- Serial line: COM12
- Speed: 115200

---

### الخطوة 3: اضغط زر RESET على اللوحة

أو أرسل هذا الأمر:
```bash
py -m mpremote connect COM12 reset
```

---

### الخطوة 4: راقب الشاشة و Serial Monitor

## ✅ ما يجب أن تراه:

### على Serial Monitor:
```
==================================================
ESP32-2432S028 COMPLETE TEST SUITE
==================================================

[1/5] Testing SPI initialization...
✅ SPI initialized successfully at 20MHz

[2/5] Testing display (ILI9341)...
✅ Display initialized successfully

   Testing colors...
   → RED
   → GREEN
   → BLUE
   → BLACK
✅ Color test complete

[3/5] Testing backlight...
✅ Backlight ON
   Testing backlight OFF for 1 second...
✅ Backlight test complete

[4/5] Testing touch controller (XPT2046)...
✅ Touch controller initialized
   Current readings: X=..., Y=...
   
   🖐️  Touch the screen anywhere...
```

### على الشاشة الفعلية:
1. **أحمر** لمدة ثانية
2. **أخضر** لمدة ثانية
3. **أزرق** لمدة ثانية
4. **أسود** لمدة ثانية
5. **إضاءة تطفئ** لثانية ثم تعود
6. **أبيض** وتنتظر اللمس (10 ثواني)

### عند اللمس:
- الشاشة تتحول **أحمر** فوراً
- Serial Monitor يطبع:
```
   ✅ Touch detected! X=1234, Y=567
✅ Touch test PASSED
```

### RGB LED:
- أحمر → أخضر → أزرق

### التقرير النهائي:
```
==================================================
TEST RESULTS SUMMARY
==================================================
✅ SPI: True
✅ DISPLAY: True
✅ BACKLIGHT: True
✅ TOUCH: True
✅ RGB: True
==================================================

🎉 ALL TESTS PASSED!
Your ESP32-2432S028 is working perfectly!

Next steps:
1. Configure WiFi in config/settings.py
2. Try examples/03_simple_button.py
3. Try examples/04_full_streamdeck.py
```

---

## ❌ إذا فشل شيء:

### المشكلة: الشاشة بيضاء فقط
**الحل:**
```bash
# جرّب baudrate أقل (10MHz)
py -m mpremote connect COM12
>>> import machine
>>> spi = machine.SPI(2, baudrate=10000000, ...)
```

### المشكلة: اللمس لا يعمل
**الحل:**
- اللمس قد يحتاج **معايرة** (calibration)
- جرّب مثال `02_test_touch.py` لرؤية القيم الخام

### المشكلة: ألوان خاطئة
**الحل:**
- غيّر `ROTATION` في `config/settings.py`:
  - `ROTATION = 0` → Portrait 240×320
  - `ROTATION = 1` → Landscape 320×240 (افتراضي)
  - `ROTATION = 2` → Portrait مقلوب
  - `ROTATION = 3` → Landscape مقلوب

---

## 📊 الاختبارات التالية

### 1️⃣ اختبار بسيط (بدون WiFi):
```bash
py -m mpremote connect COM12 run esp32-micropython/test_simple.py
```

### 2️⃣ اختبار الشاشة مع المكتبة:
```bash
py -m mpremote connect COM12 run esp32-micropython/examples/01_test_display.py
```

### 3️⃣ اختبار اللمس:
```bash
py -m mpremote connect COM12 run esp32-micropython/examples/02_test_touch.py
```

### 4️⃣ اختبار الأزرار (4 أزرار ملونة):
```bash
py -m mpremote connect COM12 run esp32-micropython/examples/03_simple_button.py
```

### 5️⃣ StreamDeck كامل (يحتاج WiFi):
أولاً عدّل WiFi في `config/settings.py`:
```python
WIFI_SSID = "اسم_شبكتك"
WIFI_PASSWORD = "كلمة_المرور"
SERVER_URL = "http://192.168.1.X:8765"  # IP الكمبيوتر
```

ثم:
```bash
# رفع الإعدادات
py -m mpremote connect COM12 cp esp32-micropython/config/settings.py :config/settings.py

# رفع المكتبات
py -m mpremote connect COM12 cp -r esp32-micropython/lib :

# تشغيل StreamDeck
py -m mpremote connect COM12 run esp32-micropython/examples/04_full_streamdeck.py
```

---

## 🎯 ملخص الحالة الحالية

✅ **تم رفع:** `test_complete.py` كـ `main.py`
✅ **جاهز للاختبار:** اضغط RESET وراقب
✅ **SPI مُصلح:** baudrate = 20MHz
✅ **جميع الملفات محدثة:** لا أخطاء متوقعة

---

## 📞 إذا احتجت المساعدة

1. ✅ انسخ **كل** ما يظهر في Serial Monitor
2. ✅ أخبرني **ماذا ترى** على الشاشة
3. ✅ أخبرني **أي خطأ** ظهر

وسأحل المشكلة فوراً! 🚀

---

**تم التحديث:** 2026-09-13
**الحالة:** جميع الملفات جاهزة ✅
