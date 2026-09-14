# StreamDeck Simulator 🎮

**محاكي Stream Deck الافتراضي - Virtual Elgato Stream Deck Simulator**

محاكي واقعي ثلاثي الأبعاد لجهاز Elgato Stream Deck يعمل في المتصفح بالكامل، مع أزرار تفاعلية، ويدجت حية، ومحرر مرئي للتخصيص.

A realistic 3D virtual Stream Deck simulator that runs entirely in the browser, featuring interactive buttons, live widgets, and a visual editor for customization.

---

## ✨ المميزات / Features

### 🎯 المميزات الأساسية / Core Features

- **أزرار LCD واقعية ثلاثية الأبعاد** / Realistic 3D LCD buttons with glassmorphism effects
- **تأثيرات ميكانيكية تفاعلية** / Physical press animations with haptic feedback
- **أصوات نقر اختيارية** / Optional click sounds using Web Audio API
- **دعم أحجام متعددة** / Multiple deck sizes:
  - Stream Deck Mini (3×2 = 6 buttons)
  - CYD OpsDeck (4×3 = 12 buttons)
  - Stream Deck Classic (5×3 = 15 buttons)
  - Stream Deck XL (8×4 = 32 buttons)

### 🚀 أنواع الإجراءات / Action Types

1. **إجراءات النظام** / System Actions:
   - فتح روابط (Open URLs)
   - تشغيل برامج (Launch Applications)
   - تنفيذ أوامر PowerShell/CMD (Execute Shell Commands)
   - نسخ نصوص (Copy Text to Clipboard)

2. **إجراءات الشبكة** / Network Actions:
   - فحص صحة الروابط (HTTP Health Checks)
   - اختبار ping للسيرفرات (Ping Monitoring)
   - أوامر Docker (Docker Commands)

3. **إجراءات متقدمة** / Advanced Actions:
   - ماكرو متعدد الخطوات (Multi-step Macros)
   - التنقل بين الصفحات (Page Navigation)
   - سكربتات مخصصة (Custom Scripts)

### 📊 الويدجت الحية / Live Widgets

- **⏰ ساعة حية** / Live Clock (HH:MM:SS)
- **📅 تاريخ حي** / Live Date Display
- **📊 مراقب CPU و RAM** / CPU & RAM Monitor with gauges
- **⏱️ ساعة إيقاف** / Stopwatch (Start/Stop/Reset)
- **⏲️ مؤقت عد تنازلي** / Countdown Timer with alerts
- **📡 مراقب Ping** / Network Ping Monitor
- **⏳ وقت التشغيل** / Session Uptime Counter

### 🎨 محرر الاستوديو / Studio Editor

- **تعديل مرئي للأزرار** / Visual button editor with live preview
- **اختيار ألوان متقدم** / Advanced color picker with presets
- **محرر إجراءات سهل** / Easy-to-use action configuration
- **إدارة البروفايلات** / Profile management (Create/Load/Save/Delete)
- **استيراد وتصدير** / Import/Export profiles as JSON
- **معاينة فورية** / Live preview while editing

---

## 📦 التثبيت / Installation

### المتطلبات / Requirements

- **متصفح حديث** / Modern web browser (Chrome, Firefox, Edge, Safari)
- **Python 3.8+** (للسيرفر المساعد - اختياري) / (for Companion Server - optional)

### خطوات التشغيل / Setup Steps

#### 1️⃣ تشغيل سريع (بدون سيرفر) / Quick Start (Browser Only)

```bash
# فقط افتح الملف في المتصفح
# Simply open the file in your browser
cd streamdeck-simulator
# افتح index.html في المتصفح / Open index.html in browser
```

#### 2️⃣ تشغيل كامل (مع السيرفر) / Full Setup (With Server)

**على Windows:**

```cmd
# تشغيل بنقرة واحدة
run_simulator.bat
```

**يدوياً / Manually:**

```bash
# 1. انتقل لمجلد السيرفر
cd streamdeck-simulator/server

# 2. أنشئ بيئة افتراضية (اختياري)
python -m venv venv

# Windows
venv\Scripts\activate

# Linux/Mac
source venv/bin/activate

# 3. ثبت المكتبات
pip install -r requirements.txt

# 4. شغل السيرفر
python server.py

# 5. افتح index.html في المتصفح
# في نافذة أخرى، افتح streamdeck-simulator/index.html
```

---

## 🎮 طريقة الاستخدام / Usage Guide

### وضع المشغل / Player Mode

الوضع الافتراضي لاستخدام الأزرار وتنفيذ الإجراءات.

Default mode for using buttons and executing actions.

- **انقر على زر** لتنفيذ إجراءه / Click a button to execute its action
- **الأزرار الحية** تتحدث تلقائياً / Live widgets update automatically
- **حالات الأزرار**: خامل (Idle)، مضغوط (Pressed)، قيد التنفيذ (Running)، نجاح (Success)، خطأ (Error)

### وضع الاستوديو / Studio Mode

وضع التعديل والتخصيص.

Editing and customization mode.

**تفعيل/إلغاء**: اضغط زر "Studio" أو `Ctrl+E`

**Activate**: Click "Studio" button or press `Ctrl+E`

#### تعديل زر / Editing a Button:

1. **انقر على الزر** الذي تريد تعديله / Click the button you want to edit
2. **لوحة Inspector** ستفتح على اليمين / Inspector panel opens on the right
3. **عدّل**: / Edit:
   - Label (النص)
   - Icon (الأيقونة أو إيموجي)
   - Background Color (اللون)
   - Action Type (نوع الإجراء)
   - Action Configuration (إعدادات الإجراء)
4. **اضغط "Apply Changes"** / Click "Apply Changes"
5. **اختبر الإجراء** بزر "Test Action" / Test with "Test Action" button

#### إدارة البروفايلات / Managing Profiles:

- **حفظ التعديلات**: `Ctrl+S` أو زر "Save" / Save changes: `Ctrl+S` or "Save" button
- **تحميل بروفايل**: من القائمة الجانبية / Load profile: from sidebar
- **إنشاء جديد**: زر "+ New Profile" / Create new: "+ New Profile" button
- **حذف بروفايل**: زر 🗑️ بجانب البروفايل / Delete: 🗑️ button next to profile
- **تصدير**: زر "Export" لحفظ كملف JSON / Export: "Export" button to save as JSON
- **استيراد**: زر "Import" لتحميل ملف JSON / Import: "Import" button to load JSON file

---

## ⚙️ واجهة برمجة السيرفر / Server API

السيرفر المساعد يوفر API endpoints لتنفيذ الإجراءات المتقدمة.

The Companion Server provides API endpoints for advanced actions.

### Endpoints:

#### `GET /api/health`
فحص حالة السيرفر / Check server health

```json
{
  "status": "ok",
  "server": "StreamDeck Companion Server",
  "version": "1.0.0"
}
```

#### `GET /api/system-stats`
الحصول على موارد النظام / Get system resource usage

```json
{
  "success": true,
  "cpu": 45.2,
  "ram": 62.5,
  "disk": 78.9
}
```

#### `POST /api/open-app`
فتح تطبيق / Open an application

```json
{
  "app": "code",
  "args": []
}
```

#### `POST /api/run-command`
تنفيذ أمر / Execute a command

```json
{
  "command": "git status",
  "shell": "powershell"
}
```

#### `POST /api/ping`
فحص اتصال بمضيف / Ping a host

```json
{
  "host": "8.8.8.8"
}
```

---

## 🎨 تخصيص البروفايلات / Profile Customization

### مثال على بروفايل JSON / Example Profile JSON:

```json
{
  "name": "My Custom Profile",
  "size": "cyd",
  "rows": 3,
  "cols": 4,
  "buttons": [
    {
      "label": "VS Code",
      "icon": "💻",
      "color": "#007acc",
      "action": {
        "type": "open_app",
        "app": "code"
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

### البروفايلات الجاهزة / Pre-built Profiles:

- **DevOps Dashboard**: أدوات المطورين / Developer tools
- **Media Control**: التحكم بالوسائط / Media controls
- **Productivity**: أدوات الإنتاجية / Productivity apps

---

## ⌨️ اختصارات لوحة المفاتيح / Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+E` | تبديل وضع الاستوديو / Toggle Studio Mode |
| `Ctrl+S` | حفظ البروفايل الحالي / Save Current Profile |
| `ESC` | إغلاق الـ Inspector / Close Inspector Panel |

---

## 🔧 استكشاف الأخطاء / Troubleshooting

### المشكلة: السيرفر لا يعمل / Server not working

**الحل / Solution:**
```bash
# تأكد من تثبيت المكتبات
pip install -r requirements.txt

# تأكد من عدم استخدام البورت 8765
netstat -ano | findstr :8765
```

### المشكلة: الأزرار لا تستجيب / Buttons not responding

**الحل / Solution:**
- تأكد من فتح Console في المتصفح للتحقق من الأخطاء / Check browser console for errors
- تأكد من تحميل جميع ملفات JS بشكل صحيح / Ensure all JS files loaded correctly
- جرب إعادة تحميل الصفحة / Try refreshing the page

### المشكلة: الويدجت الحية لا تتحدث / Live widgets not updating

**الحل / Solution:**
- إذا كان السيرفر مطفأ، بعض الويدجت ستعمل بوضع محاكاة / Some widgets work in demo mode without server
- للحصول على بيانات حقيقية، شغّل السيرفر / For real data, start the companion server
- تحقق من اتصال السيرفر في Console / Check server connection in console

---

## 📱 الاستخدام على الهاتف / Mobile Usage

يمكن استخدام المحاكي على الهواتف والتابلت كـ Stream Deck محمول!

The simulator works on phones and tablets as a portable Stream Deck!

1. **افتح `index.html` في متصفح الهاتف** / Open `index.html` in mobile browser
2. **أضف للشاشة الرئيسية** / Add to Home Screen (Chrome: ⋮ → Add to Home screen)
3. **استخدم باللمس** / Use with touch controls
4. **بعض الإجراءات قد لا تعمل بدون السيرفر** / Some actions require companion server

---

## 🏗️ البنية التقنية / Technical Architecture

### Frontend Stack:
- **Vanilla JavaScript** (ES6+)
- **CSS3** with Glassmorphism effects
- **Web Audio API** for haptic feedback
- **LocalStorage** for persistence

### Backend (Optional):
- **Python 3.8+**
- **Flask** web framework
- **psutil** for system monitoring
- **CORS** enabled for browser access

### File Structure:
```
streamdeck-simulator/
├── index.html              # Main interface
├── css/
│   ├── deck.css           # Deck styling
│   └── studio.css         # Studio editor styling
├── js/
│   ├── deck-core.js       # Core deck engine
│   ├── actions.js         # Actions execution
│   ├── live-keys.js       # Live widgets
│   ├── profiles.js        # Profile management
│   └── studio-ui.js       # Studio editor
├── server/
│   ├── server.py          # Python companion server
│   └── requirements.txt   # Python dependencies
├── presets/
│   ├── devops_profile.json
│   ├── media_profile.json
│   └── productivity_profile.json
├── run_simulator.bat      # Windows launcher
└── README.md              # This file
```

---

## 🌟 أمثلة على الاستخدام / Use Cases

### للمطورين / For Developers:
- **اختصارات Git**: تنفيذ أوامر git بنقرة واحدة / One-click git commands
- **Docker Control**: إدارة الحاويات / Container management
- **مراقبة الخوادم**: فحص صحة APIs / Health checks for APIs
- **فتح أدوات التطوير**: VS Code, Terminal, Browser / Open dev tools quickly

### للمبدعين / For Creators:
- **التحكم بالوسائط**: Spotify, YouTube, OBS / Media control
- **اختصارات التسجيل**: بدء/إيقاف التسجيل / Recording shortcuts
- **أدوات التحرير**: فتح برامج التحرير / Open editing software

### للإنتاجية / For Productivity:
- **فتح التطبيقات بسرعة**: Gmail, Calendar, Slack / Quick app launcher
- **أتمتة المهام**: تنفيذ سكربتات مخصصة / Task automation
- **المؤقتات والتنبيهات**: Pomodoro timer / Timers and alerts

---

## 🤝 المساهمة / Contributing

المشروع مفتوح المصدر، المساهمات مرحب بها!

This project is open source, contributions are welcome!

1. Fork المشروع / Fork the repository
2. أنشئ فرع للميزة / Create feature branch: `git checkout -b feature/amazing-feature`
3. Commit التعديلات / Commit changes: `git commit -m 'Add amazing feature'`
4. Push للفرع / Push to branch: `git push origin feature/amazing-feature`
5. افتح Pull Request

---

## 📄 الترخيص / License

هذا المشروع مرخص تحت MIT License - يمكنك استخدامه وتعديله بحرية.

This project is licensed under the MIT License - feel free to use and modify.

---

## 👨‍💻 المطور / Developer

تم تطويره بواسطة **DarkAI Team**

Developed by **DarkAI Team**

---

## 🙏 شكر وتقدير / Acknowledgments

- **Elgato** لإلهام التصميم / For design inspiration
- **مجتمع المطورين** للأدوات والمكتبات / Developer community for tools

---

## 📞 الدعم / Support

للأسئلة أو المشاكل:

For questions or issues:

- افتح Issue على GitHub / Open a GitHub issue
- تحقق من قسم Troubleshooting أعلاه / Check the Troubleshooting section

---

**استمتع باستخدام StreamDeck Simulator! 🎮**

**Enjoy using StreamDeck Simulator! 🎮**
