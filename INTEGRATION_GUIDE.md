# ESP32-2432S028 StreamDeck Integration Guide

## 🎉 تم الإكمال بنجاح / Successfully Completed!

تم ربط ESP32-2432S028 (CYD) بنجاح مع مشروع StreamDeck Simulator!

---

## 📁 الملفات المُنشأة / Created Files

### ESP32 Firmware
```
esp32-firmware/
├── platformio.ini          ✅ تكوين المشروع
├── src/
│   └── main.cpp           ✅ الكود الرئيسي (500+ سطر)
├── include/               ✅ للـ headers
├── lib/                   ✅ للمكتبات
└── README.md             ✅ دليل شامل
```

### Server Updates
```
streamdeck-simulator/server/
└── server.py              ✅ تم إضافة 3 endpoints جديدة:
                              - /api/get-profile
                              - /api/set-profile  
                              - /api/execute-action
```

---

## 🚀 دليل التشغيل السريع / Quick Start Guide

### الخطوة 1️⃣: تعديل إعدادات WiFi

افتح `esp32-firmware/src/main.cpp` وعدّل:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";          // 👈 اسم الشبكة
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // 👈 كلمة المرور
const char* SERVER_URL = "http://192.168.1.X:8765"; // 👈 IP الكمبيوتر
```

### الخطوة 2️⃣: معرفة IP الكمبيوتر

**Windows:**
```cmd
ipconfig
```

**Linux/Mac:**
```bash
ifconfig
# أو
ip addr show
```

### الخطوة 3️⃣: رفع الفيرموير للـ ESP32

**باستخدام PlatformIO:**
```bash
cd esp32-firmware
pio run --target upload
```

**باستخدام Arduino IDE:**
1. ثبت مكتبات: TFT_eSPI, ArduinoJson, XPT2046_Touchscreen
2. ضبط Board: ESP32 Dev Module
3. افتح `src/main.cpp`
4. اضغط Upload

### الخطوة 4️⃣: تشغيل السيرفر

```bash
cd streamdeck-simulator/server
python server.py
```

### الخطوة 5️⃣: فتح واجهة الويب

```bash
cd streamdeck-simulator
# افتح index.html في المتصفح
```

### الخطوة 6️⃣: إنشاء بروفايل

1. في الواجهة، اضغط **"Studio"** أو `Ctrl+E`
2. عدّل الأزرار كما تريد
3. اضغط **"Save"** أو `Ctrl+S`
4. ESP32 سيتزامن تلقائياً خلال 5 ثواني!

---

## 🎮 المميزات المتوفرة / Available Features

### ✅ على ESP32:
- ✅ شاشة TFT 240x320 ملونة
- ✅ 12 زر تفاعلي (شبكة 4×3)
- ✅ لمس مع ردود فعل بصرية
- ✅ مزامنة تلقائية مع الويب كل 5 ثواني
- ✅ مؤشر حالة الاتصال (أخضر/أصفر/أحمر)
- ✅ دعم جميع أنواع الأزرار

### ✅ أنواع الإجراءات المدعومة:
- ✅ فتح روابط (Open URLs)
- ✅ تشغيل برامج (Launch Apps)
- ✅ تنفيذ أوامر (Run Commands)
- ✅ نسخ نصوص (Copy Text)
- ✅ فحص HTTP (HTTP Health Check)
- ✅ Ping السيرفرات

### ✅ الويدجت الحية:
- ✅ ساعة حية (Clock)
- ✅ تاريخ (Date)
- ✅ وقت التشغيل (Uptime)
- ⚠️ CPU/RAM (يحتاج سيرفر)

---

## 🔌 مخطط التوصيل / Connection Diagram

```
┌──────────────────────────────────────────────┐
│                                              │
│           ESP32-2432S028 (CYD)              │
│                                              │
│  ┌────────────────────────────────────┐    │
│  │                                    │    │
│  │     ILI9341 TFT Display           │    │
│  │        240 x 320 pixels            │    │
│  │                                    │    │
│  │   ┌──┐ ┌──┐ ┌──┐ ┌──┐           │    │
│  │   │ 1│ │ 2│ │ 3│ │ 4│           │    │
│  │   └──┘ └──┘ └──┘ └──┘           │    │
│  │   ┌──┐ ┌──┐ ┌──┐ ┌──┐           │    │
│  │   │ 5│ │ 6│ │ 7│ │ 8│  (4x3)    │    │
│  │   └──┘ └──┘ └──┘ └──┘           │    │
│  │   ┌──┐ ┌──┐ ┌──┐ ┌──┐           │    │
│  │   │ 9│ │10│ │11│ │12│           │    │
│  │   └──┘ └──┘ └──┘ └──┘           │    │
│  │                                    │    │
│  └────────────────────────────────────┘    │
│                                              │
│  WiFi: 802.11 b/g/n                         │
│  Touch: XPT2046                             │
│                                              │
└──────────────────────────────────────────────┘
         │
         │ WiFi Connection
         ↓
┌──────────────────────────────────────────────┐
│                                              │
│         Your Computer                        │
│                                              │
│  ┌────────────────────────────────────┐    │
│  │  Python Server (Port 8765)        │    │
│  │  streamdeck-simulator/server/     │    │
│  └────────────────────────────────────┘    │
│                                              │
│  ┌────────────────────────────────────┐    │
│  │  Web Interface                     │    │
│  │  streamdeck-simulator/index.html  │    │
│  └────────────────────────────────────┘    │
│                                              │
└──────────────────────────────────────────────┘
```

---

## 🎯 سير العمل / Workflow

```
1. الويب (Web Interface)
   └─ يقوم المستخدم بإنشاء/تعديل البروفايل
   
2. السيرفر (Python Server)
   └─ يحفظ البروفايل في الذاكرة
   └─ يوفر API للـ ESP32
   
3. ESP32 (Hardware)
   └─ يسحب البروفايل كل 5 ثواني
   └─ يعرض الأزرار على الشاشة
   └─ عند الضغط على زر:
       └─ يرسل طلب للسيرفر
       └─ السيرفر ينفذ الإجراء
       └─ ESP32 يعرض النتيجة
```

---

## 📊 API Endpoints للـ ESP32

### 1. Health Check
```
GET /api/health
Response: { "status": "ok", "server": "StreamDeck Companion Server" }
```

### 2. Get Profile
```
GET /api/get-profile
Response: {
  "name": "Profile Name",
  "size": "cyd",
  "buttons": [...]
}
```

### 3. Set Profile (من الويب)
```
POST /api/set-profile
Body: { "name": "...", "buttons": [...] }
```

### 4. Execute Action (من ESP32)
```
POST /api/execute-action
Body: {
  "actionType": "open_url",
  "actionData": "{\"url\":\"https://google.com\"}"
}
```

---

## 🐛 استكشاف الأخطاء الشائعة / Common Issues

### ❌ المشكلة: ESP32 لا يتصل بالـ WiFi

**✅ الحل:**
1. تحقق من SSID والباسورد صحيح
2. ESP32 يدعم فقط 2.4GHz (لا يدعم 5GHz)
3. جرب إعادة تشغيل ESP32

### ❌ المشكلة: الشاشة بيضاء

**✅ الحل:**
1. تحقق من توصيلات الشاشة
2. راجع pin definitions في `platformio.ini`
3. جرب rotation مختلف في الكود

### ❌ المشكلة: اللمس لا يعمل

**✅ الحل:**
1. عايِر إحداثيات اللمس في الكود:
```cpp
uint16_t x = map(p.x, 200, 3700, 0, SCREEN_WIDTH);
uint16_t y = map(p.y, 240, 3800, 0, SCREEN_HEIGHT);
```
2. استخدم Serial Monitor لقراءة القيم الخام

### ❌ المشكلة: الأزرار لا تتزامن

**✅ الحل:**
1. تأكد من تشغيل السيرفر: `python server.py`
2. تأكد من IP صحيح في الكود
3. تأكد من Firewall لا يحجب Port 8765

---

## 📸 صور توضيحية / Screenshots

### شاشة ESP32:
```
┌─────────────────────────┐
│ 🟢                      │  ← مؤشر الحالة
│                         │
│  ┌────┐ ┌────┐ ┌────┐  │
│  │ 💻 │ │ ⚡ │ │ 🐳 │  │
│  │Code│ │Term│ │Dock│  │
│  └────┘ └────┘ └────┘  │
│                         │
│  ┌────┐ ┌────┐ ┌────┐  │
│  │ 🌐 │ │ 📡 │ │ 🕐 │  │
│  │HTTP│ │Ping│ │12:30│ │
│  └────┘ └────┘ └────┘  │
│                         │
│  ┌────┐ ┌────┐ ┌────┐  │
│  │ 📋 │ │ 📝 │ │ 🗑️ │  │
│  │ PS │ │ Git│ │Clear│ │
│  └────┘ └────┘ └────┘  │
│                         │
└─────────────────────────┘
```

---

## 🎓 نصائح متقدمة / Advanced Tips

### 1. تحسين الأداء
```cpp
// خفض معدل المزامنة لتوفير الطاقة
const int SYNC_INTERVAL = 10000; // 10 ثواني

// تعطيل الويدجت المعقدة
// استخدم فقط: clock, uptime, date
```

### 2. تخصيص الألوان
```cpp
// استخدم تنسيق RGB565
uint16_t myColor = tft.color565(255, 0, 0); // Red
```

### 3. إضافة أزرار أكثر
```cpp
// للحصول على 5×3 (15 زر)
#define GRID_COLS 5
#define GRID_ROWS 3
// ⚠️ الأزرار ستكون أصغر
```

---

## 📚 موارد إضافية / Additional Resources

- [TFT_eSPI Documentation](https://github.com/Bodmer/TFT_eSPI)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [PlatformIO Docs](https://docs.platformio.org/)
- [CYD Hardware Info](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)

---

## ✅ Checklist للتحقق

- [ ] WiFi SSID والباسورد صحيح
- [ ] IP الكمبيوتر محدث في الكود
- [ ] السيرفر يعمل على Port 8765
- [ ] Firewall لا يحجب الاتصال
- [ ] ESP32 متصل بالطاقة
- [ ] المكتبات مثبتة بشكل صحيح
- [ ] Serial Monitor يظهر "WiFi connected!"
- [ ] واجهة الويب مفتوحة
- [ ] بروفايل تم حفظه

---

## 🎉 تم الإكمال!

الآن لديك:
✅ ESP32-2432S028 يعمل كـ Stream Deck حقيقي
✅ شاشة لمس تفاعلية 240×320
✅ 12 زر قابل للتخصيص
✅ مزامنة لاسلكية مع الويب
✅ تنفيذ إجراءات على الكمبيوتر

**استمتع بـ StreamDeck الخاص بك! 🚀**

---

## 📞 الدعم / Support

إذا واجهت أي مشكلة:
1. راجع قسم [استكشاف الأخطاء](#-استكشاف-الأخطاء-الشائعة--common-issues)
2. افتح Serial Monitor للتحقق من الأخطاء
3. تأكد من جميع الخطوات في Checklist

---

**Created by DarkAI Team 🎮**
**Date: 2026-09-12**
