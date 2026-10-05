/**
 * ESP32-2432S028 NexusDeck Firmware
 * Hardware: Cheap Yellow Display (CYD)
 * Display: ILI9341 240x320 TFT with XPT2046 Touch
 *
 * Features:
 * - 4x3 Grid (12 buttons) optimized for CYD
 * - WiFi connectivity to companion server
 * - Touch screen button detection
 * - Profile synchronization
 * - Live widgets support
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <Preferences.h>
#include <WiFiManager.h>
#include <qrcode.h>
#include <DHT.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include "config.h"
#undef SERVER_URL
#define SERVER_URL serverIP.c_str()

// ===== Configuration & Settings =====
Preferences preferences;
String serverIP = "";
String bgColorHex = "#1a1a2e";
bool bgColorCustom = false;
String apiToken = "";
// MP3 player state (used by loop() and loadSettings())
bool musicPageActive = false;
unsigned long musicOpenedAt = 0;
String mp3Tracks[12];
uint8_t mp3Count = 0;
int8_t mp3Index = -1;
bool musicPlaying = false;
float mp3Gain = 0.5;
AudioGeneratorMP3* mp3Decoder = nullptr;
AudioFileSourceHTTPStream* mp3Source = nullptr;
AudioFileSourceBuffer* mp3Buff = nullptr;
AudioOutputI2S* mp3Out = nullptr;

bool offlineMode = false;
float dhtTempC = NAN;
float dhtHumidity = NAN;
bool dhtOk = false;
unsigned long lastDhtRead = 0;
bool screenAwake = true;
unsigned long lastMotionAt = 0;

// ===== Onboard hardware (adjust to your wiring) =====
// CYD SD slot: SCK=18 MISO=19 MOSI=23 CS=5 (VSPI defaults).
#define SD_CS_PIN 5
// Speaker JST on most CYD revisions = GPIO26 (verify with a multimeter).
// MP3 output uses the internal DAC (pins 25+26), beeps use LEDC PWM.
#define SPEAKER_PIN 26
#define SPEAKER_CHANNEL 7
// DHT22 temperature/humidity on a free header pin.
#define DHT_PIN 22
#define DHT_TYPE DHT22
// PIR motion sensor (HIGH on motion) for auto wake/sleep.
#define PIR_PIN 27
#define PIR_IDLE_SEC 90
#define ENABLE_CLICK true
#define ENABLE_SD 0 // 1 = SD on (breaks touch on shared bus!)
DHT dht(DHT_PIN, DHT_TYPE);

void loadSettings() {
    preferences.begin("deck", false);
    serverIP = preferences.getString("srv_ip", "");
    bgColorHex = preferences.getString("bg_color", "#1a1a2e");
    bgColorHex.trim();
    if (bgColorHex.length() == 0) {
        bgColorHex = "#1a1a2e";
    }
    bgColorCustom = preferences.getBool("bg_custom", false);
    apiToken = preferences.getString("api_token", "");
    apiToken.trim();
    mp3Gain = preferences.getFloat("mp3_gain", 0.5f);
    if (!(mp3Gain >= 0.1f && mp3Gain <= 1.0f)) mp3Gain = 0.5f;
    preferences.end();
}

void saveIP(String ip) {
    preferences.begin("deck", false);
    preferences.putString("srv_ip", ip);
    preferences.end();
    serverIP = ip;
}

void saveApiToken(String token) {
    token.trim();
    preferences.begin("deck", false);
    preferences.putString("api_token", token);
    preferences.end();
    apiToken = token;
}

// Begin an authenticated server request (LAN pairing token + client id).
void espHttpBegin(HTTPClient& client, const String& url) {
    client.begin(url);
    client.addHeader("X-NexusDeck-Client", "esp32");
    if (apiToken.length() > 0) {
        client.addHeader("X-NexusDeck-Token", apiToken);
    }
}

#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_SCLK 25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32
#define TFT_BACKLIGHT 21

// WiFi Configuration
// WiFi and server values are stored in the ignored include/config.h file.
const int SYNC_INTERVAL = 3000; // Sync every 3 seconds to reduce display/power noise

// Grid Configuration (4x3 for CYD)
#define GRID_COLS 4
#define GRID_ROWS 3
#define BUTTON_COUNT (GRID_COLS * GRID_ROWS)

// Display Configuration
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define BUTTON_PADDING 4
#define TFT_DARK_BG 0x18C5 // #1a1a2e

// Status bar + button grid layout
#define STATUS_BAR_HEIGHT 18
#define GRID_Y_OFFSET STATUS_BAR_HEIGHT
#define GRID_HEIGHT (SCREEN_HEIGHT - STATUS_BAR_HEIGHT)

// Calculate button dimensions (grid sits below the status bar)
#define BUTTON_WIDTH ((SCREEN_WIDTH - (BUTTON_PADDING * (GRID_COLS + 1))) / GRID_COLS)
#define BUTTON_HEIGHT ((GRID_HEIGHT - (BUTTON_PADDING * (GRID_ROWS + 1))) / GRID_ROWS)

// Home page pomodoro button geometry (shared by full + partial redraw)
#define HOME_POMO_X 84
#define HOME_POMO_Y 128
#define HOME_POMO_W 152
#define HOME_POMO_H 76

// ===== Objects =====
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
HTTPClient http;

// ===== Button Structure =====
struct Button {
    String label;
    String icon;
    uint16_t color;
    String actionType;
    String actionData;
    bool hasWidget;
    String widgetType;
    uint8_t state; // 0=idle, 1=pressed, 2=running, 3=success, 4=error
    bool timerRunning;
    int timerRemaining;
    int timerDuration;
    unsigned long timerLastTick;
    uint8_t pomoPhase;      // 0=work, 1=short break, 2=long break
    uint8_t pomoDone;       // completed work sessions
    int pomoWorkSec;
    int pomoShortSec;
    int pomoLongSec;
    uint8_t pomoCycle;      // sessions before long break
    String pomoTask;        // task name shown on the pomo page
    bool pomoAlert;
    unsigned long pomoAlertUntil;
    unsigned long pomoLastTap;
};

Button buttons[BUTTON_COUNT];

struct ButtonResetSchedule {
    uint8_t index;
    unsigned long resetAt;
    bool active;
};

// ===== State Variables =====
unsigned long lastSyncTime = 0;
unsigned long lastWidgetUpdate = 0;
unsigned long lastStatusBarDraw = 0;
unsigned long lastRunningBlink = 0;
unsigned long touchDebounceUntil = 0;
bool runningBlinkOn = false;
int8_t lastPressedButton = -1;
bool wifiConnected = false;
bool serverAvailable = false;
int cpuPercent = 0;
int ramPercent = 0;
unsigned long lastStatsFetch = 0;
String currentProfileName = "NexusDeck";
ButtonResetSchedule buttonReset = {255, 0, false};
bool statusBarDrawn = false;
uint16_t deckBackgroundColor = TFT_DARK_BG;
int syncFailCount = 0;
// Calendar events page state
bool calPageActive = false;
// Spotify now-playing page state (functions defined further below)
bool spotPageActive = false;
unsigned long spotOpenedAt = 0;
unsigned long spotLastRefresh = 0;
String spotTrack = "";
String spotArtist = "";
bool spotPlaying = false;
long spotProgressMs = 0;
long spotDurationMs = 0;
String spotError = "";
unsigned long calOpenedAt = 0;
struct CalEvent {
    String when;
    String end;
    String summary;
    String location;
    String desc;
};
CalEvent calEvents[8];
uint8_t calCount = 0;
String calError = "";
int8_t calDetail = -1; // -1 = list, else event index

// Pomodoro dedicated page state
bool pomoPageActive = false;
int pomoPageIndex = -1; // 0..11 grid button, 255 = standalone home pomodoro
bool pomoReturnHome = false;
int8_t pomoPressCandidate = -1;
unsigned long pomoPressStart = 0;
// Tap-vs-swipe state (grid): tap executes on release, right-edge swipe goes back
int8_t pendingButton = -1;
int16_t gestureStartX = -1, gestureStartY = -1;
bool gestureFired = false;
// Ignore touches briefly after opening a page (finger may still be down)
unsigned long ignoreTouchUntil = 0;
int pomoPageLastSecond = -1;
bool pomoPageDirty = true;
// Backlight alert state (phase-end notification)
bool backlightAlert = false;
unsigned long backlightAlertUntil = 0;
unsigned long backlightLastToggle = 0;
bool backlightLevel = true;
// Home page state
bool homePageActive = false;
String homeNames[4] = {"", "", "", ""};
String homeTime = "--:--";
String homeDate = "--";
String homeTemp = "--";
String homePrayerName = "--";
String homePrayerTime = "--:--";
String homeLastClock = "";
bool homePageDirty = true;
bool homePomoPressActive = false;
unsigned long homePomoPressStart = 0;
int homePomoPressX = 0;
unsigned long homeTimeFreshAt = 0;
unsigned long lastHomeInfoFetch = 0;
bool clockAnalog = true;
bool showTemp = true;
bool showPrayer = true;
bool showDate = true;
uint8_t screenBrightness = 255;
Button homePomo;

// ===== Function Declarations =====
void setupWiFi();
void openSetupPortal();
void drawSetupQR();
void setupDisplay();
void setupTouch();
void drawButton(uint8_t index);
void drawDeckBackground();
void drawAllButtons();
void handleTouch();
void touchSelfTest();
extern TS_Point touchPoint;
extern bool touchDown;
int8_t getTouchedButton(uint16_t x, uint16_t y);
void executeButtonAction(uint8_t index);
void syncProfile();
void applyProfilePayload(const String& payload);
void updateWidgets();
void setButtonState(uint8_t index, uint8_t state);
uint16_t parseColor(String colorHex);
void drawStatusBar();
void updateSystemStats();
String displayIcon(const String& icon);
bool drawIconShape(TFT_eSprite& sprite, const String& icon, int cx, int cy, uint16_t fg, uint16_t bg);
void scheduleButtonReset(uint8_t index, unsigned long delayMs);
void processButtonResets();
void updateRunningIndicators();
String truncateText(const String& text, uint8_t maxLen);
bool anyOverlayActive();
void openPomoPage(uint8_t index);
void closePomoPage();
void openCalendarPage();
void closeCalendarPage();
void drawCalendarPage();
void handleCalTouch();
void openSpotifyPage();
void closeSpotifyPage();
void drawSpotifyPage();
void handleSpotTouch();
bool spotFetchNow();
void drawPomoPage();
void handlePomoPageTouch();
void pomoTapAction(uint8_t index);
void pomoResetSession(uint8_t index);
void pomoToggleRun(uint8_t index);
void pomoToggleRunBtn(Button& btn);
void pomoResetBtn(Button& btn);
void triggerBacklightAlert(unsigned long durationMs);
void updateBacklight();
void applyBacklight(bool on);
void pumpBuzzer();
void playToneSeq(const int* freqs, const int* durs, uint8_t len);
void clickSound();
void alarmSound(uint8_t kind);
bool setupSD();
void sdSaveProfile(const String& name, const String& payload);
bool sdLoadProfile(const String& name);
bool sdLoadLastProfile();
void musicScanSD();
void openMusicPage();
void closeMusicPage();
void drawMusicPage();
void handleMusicTouch();
void musicPump();
void updateDHT();
void updatePresence();
void tickPomo(Button& btn, bool gridFeedback, uint8_t idx, bool& needsRedraw);
bool anyOverlayActive();
Button& pomoViewButton();
void tickPomo(Button& btn, bool gridFeedback, uint8_t idx, bool& needsRedraw);
void openHomePage();
void closeHomeToGrid();
void drawHomePage();
void updateHomeClock();
void updateHomePomoButton();
void pomoAdjustMinutes(int deltaMin);
void handleHomeTouch();
void fetchHomeInfo(bool force);
void fetchProfileNames();
void switchToHomeProfile(uint8_t slot);
String homeClockHM();

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    Serial.println("\n=================================");
    Serial.println("NexusDeck ESP32-2432S028 Firmware");
    Serial.println("FW-BUILD: stable-touch-v2");
    Serial.println("=================================\n");

    // Pin scan runs before ANY peripheral claims pins (SD/TFT/touch).
    pinMode(TOUCH_CS, OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);
    touchSelfTest();

#if ENABLE_SD
    // SD shares the touch VSPI bus on this unit: enabling it kills touch.
    // Kept for a future SoftSPI/independent-bus design.
    setupSD();
#endif

    // Initialize display FIRST
    setupDisplay();

    // Initialize touch
    setupTouch();

    // Load saved server IP and background color
    loadSettings();
    deckBackgroundColor = parseColor(bgColorHex);

    pinMode(PIR_PIN, INPUT_PULLDOWN);
    dht.begin();

    // Initialize buttons with defaults (before WiFi: an SD offline profile
    // loaded in setupWiFi() must survive instead of being wiped here)
    for (int i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].label = String(i + 1);
        buttons[i].icon = "";
        buttons[i].color = TFT_DARK_BG;
        buttons[i].actionType = "";
        buttons[i].actionData = "";
        buttons[i].hasWidget = false;
        buttons[i].widgetType = "";
        buttons[i].pomoTask = "";
        buttons[i].state = 0;
        buttons[i].timerRunning = false;
        buttons[i].timerRemaining = 0;
        buttons[i].timerDuration = 300;
        buttons[i].timerLastTick = 0;
        buttons[i].pomoPhase = 0;
        buttons[i].pomoDone = 0;
        buttons[i].pomoWorkSec = 1500;
        buttons[i].pomoShortSec = 300;
        buttons[i].pomoLongSec = 900;
        buttons[i].pomoCycle = 4;
        buttons[i].pomoAlert = false;
        buttons[i].pomoAlertUntil = 0;
        buttons[i].pomoLastTap = 0;
    }

    // Connect to WiFi or launch NexusDeck-Setup portal
    setupWiFi();

    // Standalone home-page pomodoro (independent of grid buttons)
    homePomo.label = "25:00";
    homePomo.icon = "PLAY";
    homePomo.color = TFT_DARK_BG;
    homePomo.actionType = "";
    homePomo.actionData = "{}";
    homePomo.hasWidget = true;
    homePomo.widgetType = "pomodoro";
    homePomo.state = 0;
    homePomo.timerRunning = false;
    homePomo.timerRemaining = 1500;
    homePomo.timerDuration = 1500;
    homePomo.timerLastTick = 0;
    homePomo.pomoPhase = 0;
    homePomo.pomoDone = 0;
    homePomo.pomoWorkSec = 1500;
    homePomo.pomoShortSec = 300;
    homePomo.pomoLongSec = 900;
    homePomo.pomoCycle = 4;
    homePomo.pomoAlert = false;
    homePomo.pomoAlertUntil = 0;
    homePomo.pomoLastTap = 0;

    // Boot into the home page
    openHomePage();

    Serial.println("Setup complete!");
}

// ===== Main Loop =====
void loop() {
    // Backlight phase-end alert blinking (non-blocking)
    updateBacklight();
    // Non-blocking buzzer sequences + MP3 streaming pump
    pumpBuzzer();
    musicPump();
    // Proven path: library reads on the custom-mapped bus (first init wins).
    // SD stays disabled (ENABLE_SD 0) so nothing ever remaps the bus.
    if (touch.tirqTouched() && touch.touched()) {
        touchPoint = touch.getPoint();
        touchDown = true;
    } else {
        touchDown = false;
    }

    // Hold top area/status bar for 2.5s to trigger NexusDeck-Setup portal anytime.
    // The timer MUST reset on release, otherwise any later short tap opens the portal.
    static unsigned long portalHoldStart = 0;
    if (touchDown) {
        TS_Point p = touchPoint;
        if (p.y < 800) {
            if (portalHoldStart == 0) portalHoldStart = millis();
            if (millis() - portalHoldStart > 2500) {
                portalHoldStart = 0;
                openSetupPortal();
            }
        } else {
            portalHoldStart = 0;
        }
    } else {
        portalHoldStart = 0;
    }

    // Pomodoro press: release = tap (toggle/reset), hold 900ms = open dedicated page
    if (pomoPressCandidate >= 0) {
        if (!touchDown) {
            uint8_t idx = pomoPressCandidate;
            pomoPressCandidate = -1;
            pomoTapAction(idx);
        } else if (millis() - pomoPressStart > 900) {
            uint8_t idx = pomoPressCandidate;
            pomoPressCandidate = -1;
            setButtonState(idx, 0);
            pomoReturnHome = false;
            openPomoPage(idx);
        }
    }

    unsigned long currentTime = millis();

    processButtonResets();
    updateRunningIndicators();

    // Handle touch input (pages or grid)
    if (pomoPageActive) {
        handlePomoPageTouch();
    } else if (homePageActive) {
        handleHomeTouch();
    } else if (calPageActive) {
        handleCalTouch();
    } else if (musicPageActive) {
        handleMusicTouch();
    } else if (spotPageActive) {
        handleSpotTouch();
    } else {
        handleTouch();
    }
    updatePresence();

    // Music page auto-closes after 2 minutes idle (only when stopped)
    if (musicPageActive && !musicPlaying && millis() - musicOpenedAt > 120000) {
        closeMusicPage();
    }

    // Calendar page auto-closes after 2 minutes
    if (calPageActive && millis() - calOpenedAt > 120000) {
        closeCalendarPage();
    }

    // Spotify page: refresh now-playing every 10s, auto-close after 5 min
    if (spotPageActive) {
        if (millis() - spotLastRefresh > 10000) {
            spotLastRefresh = millis();
            if (spotFetchNow()) drawSpotifyPage();
        }
        if (millis() - spotOpenedAt > 300000) {
            closeSpotifyPage();
        }
    }

    // Sync profile from server
    if (wifiConnected && (currentTime - lastSyncTime > SYNC_INTERVAL)) {
        syncProfile();
        lastSyncTime = currentTime;
    }

    // Update live widgets
    if (currentTime - lastWidgetUpdate > 1000) {
        updateWidgets();
        lastWidgetUpdate = currentTime;
    }

    // Refresh pomodoro page once per second / on state change
    if (pomoPageActive && pomoPageIndex != -1) {
        Button& pb = pomoViewButton();
        if (pomoPageDirty || pb.timerRemaining != pomoPageLastSecond) {
            pomoPageLastSecond = pb.timerRemaining;
            pomoPageDirty = false;
            drawPomoPage();
        }
    }

    // Refresh home page clock once per minute / on state change + data refresh
    if (homePageActive) {
        if (millis() - lastHomeInfoFetch > 60000) {
            fetchHomeInfo(false);
            fetchProfileNames();
        }
        String hm = homeClockHM();
        if (homePageDirty) {
            homeLastClock = hm;
            homePageDirty = false;
            drawHomePage();
        } else if (hm != homeLastClock) {
            // Minute changed: redraw only the clock text (no full-screen
            // fill, so there is no visible flicker)
            homeLastClock = hm;
            updateHomeClock();
        }
        // Live pomodoro countdown: redraw only the pomo button twice a second
        static unsigned long lastHomePomoDraw = 0;
        if ((homePomo.timerRunning || homePomo.pomoAlert) &&
            millis() - lastHomePomoDraw >= 500) {
            lastHomePomoDraw = millis();
            updateHomePomoButton();
        }
        // Home pomodoro press resolution: release = -/+1 min or
        // start-pause (double-tap = reset), hold 900ms = full page
        if (homePomoPressActive) {
            if (!touch.touched()) {
                homePomoPressActive = false;
                int px = homePomoPressX;
                unsigned long now = millis();
                if (px < HOME_POMO_X + 32) {
                    pomoAdjustMinutes(-1);
                } else if (px > HOME_POMO_X + HOME_POMO_W - 32) {
                    pomoAdjustMinutes(1);
                } else if (now - homePomo.pomoLastTap < 600) {
                    homePomo.pomoLastTap = 0;
                    pomoResetBtn(homePomo);
                    Serial.println("Home pomodoro reset");
                } else {
                    homePomo.pomoLastTap = now;
                    pomoToggleRunBtn(homePomo);
                }
                updateHomePomoButton();
            } else if (millis() - homePomoPressStart > 900) {
                homePomoPressActive = false;
                pomoReturnHome = true;
                openPomoPage(255);
            }
        }
    }

    // Refresh status bar periodically (CPU/RAM/profile name)
    if (!anyOverlayActive() && currentTime - lastStatusBarDraw > 5000) {
        drawStatusBar();
        lastStatusBarDraw = currentTime;
    }

    delay(10);
}

// ===== Display Setup =====
void setupDisplay() {
    Serial.println("Initializing display...");
    pinMode(TFT_BACKLIGHT, OUTPUT);
    applyBacklight(true);
    tft.init();
    tft.setRotation(1); // Landscape: 320x240
    Serial.print("TFT dimensions: ");
    Serial.print(tft.width());
    Serial.print("x");
    Serial.println(tft.height());
    tft.fillScreen(deckBackgroundColor);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);

    // Show splash screen
    tft.fillScreen(deckBackgroundColor);
    tft.setTextColor(TFT_WHITE, TFT_DARK_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("NexusDeck CYD", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 4);
    tft.drawString("Initializing...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 20, 2);
    delay(2000);

    Serial.println("Display initialized!");
}


// ===== Touch Setup =====
void setupTouch() {
    Serial.println("Initializing touch screen...");
    SPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI);
    touch.begin();
    SPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI);
    touch.setRotation(1);
    Serial.println("Touch screen initialized!");
}

// ===== Setup Portal (NexusDeck-Setup) =====
// QR helpers: scan to join the setup WiFi instead of typing the password.
#define SETUP_SSID "NexusDeck-Setup"
#define SETUP_PASS "password123"
#define QR_VERSION 5
static uint8_t qrModules[256]; // fits version <= 6 (41x41 modules)

bool drawQRCode(const char* text, int x, int y, int scale) {
    QRCode code;
    if (qrcode_initText(&code, qrModules, QR_VERSION, ECC_LOW, text) != 0) {
        return false;
    }
    int size = code.size;
    tft.fillRect(x - scale, y - scale,
                 size * scale + scale * 2, size * scale + scale * 2, TFT_WHITE);
    for (uint8_t row = 0; row < size; row++) {
        for (uint8_t col = 0; col < size; col++) {
            if (qrcode_getModule(&code, col, row)) {
                tft.fillRect(x + col * scale, y + row * scale, scale, scale, TFT_BLACK);
            }
        }
    }
    return true;
}

void drawSetupQR() {
    char wifiQR[96];
    snprintf(wifiQR, sizeof(wifiQR), "WIFI:T:WPA;S:%s;P:%s;;", SETUP_SSID, SETUP_PASS);
    drawQRCode(wifiQR, 12, 44, 4); // 37x37 modules * 4 = 148px
}

void openSetupPortal() {
    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_DARK_BG);
    tft.drawString("WiFi Setup", SCREEN_WIDTH / 2, 16, 4);

    drawSetupQR();

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString("Scan to join WiFi", 172, 52, 2);
    tft.setTextColor(TFT_CYAN, deckBackgroundColor);
    tft.drawString(SETUP_SSID, 172, 76, 2);
    tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
    tft.drawString("password123", 172, 98, 1);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString("Then open:", 172, 124, 2);
    tft.setTextColor(TFT_GREEN, deckBackgroundColor);
    tft.drawString("192.168.4.1", 172, 146, 2);
    tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
    tft.drawString("Set WiFi + Server IP", 172, 172, 1);
    tft.setTextDatum(MC_DATUM);

    Serial.println("Starting config portal: NexusDeck-Setup");
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);

    WiFiManagerParameter custom_server_ip("server_ip", "Server URL (e.g. http://192.168.1.5:8765)", serverIP.c_str(), 60);
    wm.addParameter(&custom_server_ip);
    WiFiManagerParameter custom_api_token("api_token", "API token (Studio > System > Security)", apiToken.c_str(), 64);
    wm.addParameter(&custom_api_token);
    WiFiManagerParameter custom_bg_color("bg_color", "Background color (pick from the list)", bgColorHex.c_str(), 8, "type=\"color\"");
    wm.addParameter(&custom_bg_color);
    WiFiManagerParameter custom_bg_follow("bg_follow", "Follow profile background instead", "1", 2, "type=\"checkbox\"");
    wm.addParameter(&custom_bg_follow);

    if (!wm.startConfigPortal("NexusDeck-Setup", "password123")) {
        Serial.println("Portal timeout, restarting...");
        delay(1000);
        ESP.restart();
    }

    String newIP = custom_server_ip.getValue();
    newIP.trim();
    if (newIP.length() > 0) {
        if (!newIP.startsWith("http://") && !newIP.startsWith("https://")) {
            newIP = "http://" + newIP;
        }
        if (newIP.indexOf(':', 7) == -1) {
            newIP += ":8765";
        }
        saveIP(newIP);
    }

    String newToken = custom_api_token.getValue();
    if (newToken.length() > 0) {
        saveApiToken(newToken);
        Serial.println("API token saved");
    }

    String followBg = custom_bg_follow.getValue();
    String newBg = custom_bg_color.getValue();
    newBg.trim();
    if (followBg.length() > 0 || newBg.length() == 0) {
        preferences.begin("deck", false);
        preferences.putBool("bg_custom", false);
        preferences.end();
        bgColorCustom = false;
    } else {
        if (!newBg.startsWith("#")) {
            newBg = "#" + newBg;
        }
        bool valid = (newBg.length() == 7);
        for (int i = 1; valid && i < 7; i++) {
            char c = newBg.charAt(i);
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                valid = false;
            }
        }
        if (valid) {
            newBg.toLowerCase();
            preferences.begin("deck", false);
            preferences.putString("bg_color", newBg);
            preferences.putBool("bg_custom", true);
            preferences.end();
            bgColorHex = newBg;
            bgColorCustom = true;
            deckBackgroundColor = parseColor(bgColorHex);
        } else {
            Serial.println("Invalid background color, keeping previous");
        }
    }

    Serial.println("Setup saved, restarting...");
    delay(500);
    ESP.restart();
}

// ===== WiFi Setup =====
void setupWiFi() {
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);

    WiFiManagerParameter custom_server_ip("server_ip", "Server URL (e.g. http://192.168.1.5:8765)", serverIP.c_str(), 60);
    wm.addParameter(&custom_server_ip);
    WiFiManagerParameter custom_api_token("api_token", "API token (Studio > System > Security)", apiToken.c_str(), 64);
    wm.addParameter(&custom_api_token);
    WiFiManagerParameter custom_bg_color("bg_color", "Background color (pick from the list)", bgColorHex.c_str(), 8, "type=\"color\"");
    wm.addParameter(&custom_bg_color);
    WiFiManagerParameter custom_bg_follow("bg_follow", "Follow profile background instead", "1", 2, "type=\"checkbox\"");
    wm.addParameter(&custom_bg_follow);

    bool needPortal = (serverIP.length() == 0 || !serverIP.startsWith("http"));

    if (needPortal) {
        openSetupPortal();
    } else {
        tft.fillScreen(deckBackgroundColor);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_DARK_BG);
        tft.drawString("Connecting to WiFi...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 2);

        if (!wm.autoConnect("NexusDeck-Setup", "password123")) {
            // No WiFi: fall back to the SD profile and run offline instead
            // of reboot-looping, so music/alarms keep working standalone.
            if (sdLoadLastProfile()) {
                offlineMode = true;
                wifiConnected = false;
                Serial.println("Offline mode: SD profile loaded");
            } else {
                Serial.println("WiFi connection failed, restarting...");
                delay(1000);
                ESP.restart();
            }
        }
    }

    String newIP = custom_server_ip.getValue();
    newIP.trim();
    if (newIP.length() > 0) {
        if (!newIP.startsWith("http://") && !newIP.startsWith("https://")) {
            newIP = "http://" + newIP;
        }
        if (newIP.indexOf(':', 7) == -1) {
            newIP += ":8765";
        }
        saveIP(newIP);
    }

    String newToken = custom_api_token.getValue();
    if (newToken.length() > 0) {
        saveApiToken(newToken);
        Serial.println("API token saved");
    }

    String followBg = custom_bg_follow.getValue();
    String newBg = custom_bg_color.getValue();
    newBg.trim();
    if (followBg.length() > 0 || newBg.length() == 0) {
        preferences.begin("deck", false);
        preferences.putBool("bg_custom", false);
        preferences.end();
        bgColorCustom = false;
    } else {
        if (!newBg.startsWith("#")) {
            newBg = "#" + newBg;
        }
        bool valid = (newBg.length() == 7);
        for (int i = 1; valid && i < 7; i++) {
            char c = newBg.charAt(i);
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                valid = false;
            }
        }
        if (valid) {
            newBg.toLowerCase();
            preferences.begin("deck", false);
            preferences.putString("bg_color", newBg);
            preferences.putBool("bg_custom", true);
            preferences.end();
            bgColorHex = newBg;
            bgColorCustom = true;
            deckBackgroundColor = parseColor(bgColorHex);
        } else {
            Serial.println("Invalid background color, keeping previous");
        }
    }

    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected) {
        WiFi.setSleep(false);
        WiFi.setTxPower(WIFI_POWER_11dBm);
    }

    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_DARK_BG);
    tft.drawString("WiFi Connected!", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 20, 4);
    tft.setTextColor(TFT_WHITE, TFT_DARK_BG);
    tft.drawString("Server: " + serverIP, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20, 2);
    delay(1500);

    Serial.println("WiFi connected!");
    Serial.println("Server URL: " + serverIP);
}

// ===== Draw Single Button =====
void drawButton(uint8_t index) {
    if (index >= BUTTON_COUNT) return;

    // Calculate button position
    uint8_t col = index % GRID_COLS;
    uint8_t row = index / GRID_COLS;

    int16_t x = BUTTON_PADDING + col * (BUTTON_WIDTH + BUTTON_PADDING);
    int16_t y = GRID_Y_OFFSET + BUTTON_PADDING + row * (BUTTON_HEIGHT + BUTTON_PADDING);

    Button& btn = buttons[index];

    // Draw button background based on state
    uint16_t bgColor = btn.color;

    if (btn.state == 1) { // Pressed
        bgColor = tft.color565(
            (((bgColor >> 11) & 0x1F) * 0.7),
            (((bgColor >> 5) & 0x3F) * 0.7),
            ((bgColor & 0x1F) * 0.7)
        );
    } else if (btn.state == 2) { // Running
        bgColor = tft.color565(30, 90, 180);
    } else if (btn.state == 3) { // Success
        bgColor = TFT_GREEN;
    } else if (btn.state == 4) { // Error
        bgColor = TFT_RED;
    }

    uint16_t borderColor = TFT_WHITE;
    if (btn.state == 2) {
        borderColor = TFT_CYAN;
    } else if (btn.state == 3) {
        borderColor = TFT_GREEN;
    } else if (btn.state == 4) {
        borderColor = TFT_RED;
    }

    TFT_eSprite sprite = TFT_eSprite(&tft);
    sprite.setColorDepth(16);
    sprite.createSprite(BUTTON_WIDTH, BUTTON_HEIGHT);
    sprite.fillSprite(deckBackgroundColor);
    sprite.fillRoundRect(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT, 6, bgColor);
    sprite.drawRoundRect(0, 0, BUTTON_WIDTH, BUTTON_HEIGHT, 6, borderColor);

    // Draw icon as a real shape when known, otherwise as text
    if (btn.icon.length() > 0) {
        if (!drawIconShape(sprite, btn.icon, BUTTON_WIDTH / 2, BUTTON_HEIGHT / 3, TFT_WHITE, bgColor)) {
            sprite.setTextColor(TFT_WHITE, bgColor);
            sprite.setTextDatum(MC_DATUM);
            sprite.drawString(truncateText(displayIcon(btn.icon), 8), BUTTON_WIDTH / 2, BUTTON_HEIGHT / 3, 2);
        }
    }

    // Draw label
    if (btn.label.length() > 0) {
        sprite.setTextColor(TFT_WHITE, bgColor);
        sprite.setTextDatum(MC_DATUM);
        int labelY = (btn.icon.length() > 0) ? ((BUTTON_HEIGHT * 2) / 3) : (BUTTON_HEIGHT / 2);
        sprite.drawString(truncateText(btn.label, 10), BUTTON_WIDTH / 2, labelY, 2);
    }

    sprite.pushSprite(x, y);
    sprite.deleteSprite();
}

// ===== Draw All Buttons =====
void drawAllButtons() {
    drawDeckBackground();
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        drawButton(i);
    }
}

void drawDeckBackground() {
    tft.fillScreen(deckBackgroundColor);
    statusBarDrawn = false;
}

// ===== Handle Touch Input =====
unsigned long lastRealTouchMs = 0;
TS_Point touchPoint(0, 0, 0);
bool touchDown = false;


void touchSelfTest() {
    // Boot touch check via the proven library path. HOLD the screen when
    // told; verdict by pressure VARIANCE (a live chip swings by hundreds).
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("TOUCH AND HOLD", SCREEN_WIDTH / 2, 100, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("15 seconds", SCREEN_WIDTH / 2, 140, 2);
    Serial.println("TOUCH-TEST: TOUCH AND HOLD NOW (15s)...");
    uint16_t zmin = 65535, zmax = 0;
    uint8_t presses = 0;
    unsigned long until = millis() + 15000;
    while (millis() < until) {
        if (touch.tirqTouched() && touch.touched()) {
            TS_Point p = touch.getPoint();
            if (p.x < 8000 && p.y < 8000) {
                presses++;
                uint16_t z = (uint16_t)p.z;
                if (z < zmin) zmin = z;
                if (z > zmax) zmax = z;
            }
        }
        delay(50);
    }
    tft.fillScreen(deckBackgroundColor);
    uint16_t spread = (zmax >= zmin && presses > 0) ? (uint16_t)(zmax - zmin) : 0;
    Serial.printf("TOUCH-TEST: presses=%u zmin=%u zmax=%u spread=%u verdict=%s\n",
                  presses, zmin == 65535 ? 0 : zmin, zmax, spread,
                  (presses > 3 && spread > 50) ? "ALIVE" : "silent");
}

void handleTouch() {
    if (touchDown) {
        TS_Point p = touchPoint;

        // Map calibrated raw touch coordinates to the rotated landscape screen.
        uint16_t x = constrain(map(p.x, 200, 3700, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
        uint16_t y = constrain(map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);

        static unsigned long lastTouchLog = 0;
        if (millis() - lastTouchLog > 500) {
            Serial.printf("Touch raw=%d,%d screen=%u,%u\n", p.x, p.y, x, y);
            lastTouchLog = millis();
        }

        // Short tap on the status bar opens the home page
        if (y < STATUS_BAR_HEIGHT && !homePageActive && !pomoPageActive &&
            millis() >= touchDebounceUntil) {
            touchDebounceUntil = millis() + 500;
            lastPressedButton = -1;
            openHomePage();
            return;
        }

        int8_t buttonIndex = getTouchedButton(x, y);

        // Right-edge swipe in progress: start right, move left, mostly horizontal
        if (gestureStartX >= 0 && !gestureFired && lastPressedButton >= 0 &&
            gestureStartX > 250 && (gestureStartX - (int)x) > 60 &&
            abs((int)y - gestureStartY) < 50) {
            gestureFired = true;
            pendingButton = -1;
            pomoPressCandidate = -1;
            setButtonState(lastPressedButton, 0);
            drawButton(lastPressedButton);
            Serial.println("Swipe back -> home page");
            openHomePage();
            return;
        }

        if (buttonIndex >= 0 && buttonIndex != lastPressedButton &&
            millis() >= touchDebounceUntil) {
            lastPressedButton = buttonIndex;
            touchDebounceUntil = millis() + 280;

            Serial.print("Button pressed: ");
            Serial.println(buttonIndex);

            // Pomodoro buttons: defer action until release (tap) or hold (open page)
            if (buttons[buttonIndex].hasWidget && buttons[buttonIndex].widgetType == "pomodoro") {
                pomoPressCandidate = buttonIndex;
                pomoPressStart = millis();
            } else {
                // Defer tap execution until release so a swipe can cancel it
                pendingButton = buttonIndex;
            }
            gestureStartX = x;
            gestureStartY = y;
            gestureFired = false;

            setButtonState(buttonIndex, 1);
            drawButton(buttonIndex);
            clickSound();
        }
    } else {
        // Finger released: run the pending tap unless a swipe already fired
        if (pendingButton >= 0 && !gestureFired) {
            uint8_t idx = pendingButton;
            pendingButton = -1;
            executeButtonAction(idx);
        } else {
            pendingButton = -1;
        }
        gestureStartX = -1;
        gestureFired = false;
        lastPressedButton = -1;
    }
}

int8_t getTouchedButton(uint16_t x, uint16_t y) {
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        uint8_t col = i % GRID_COLS;
        uint8_t row = i / GRID_COLS;

        int16_t btnX = BUTTON_PADDING + col * (BUTTON_WIDTH + BUTTON_PADDING);
        int16_t btnY = GRID_Y_OFFSET + BUTTON_PADDING + row * (BUTTON_HEIGHT + BUTTON_PADDING);

        if (x >= btnX && x <= btnX + BUTTON_WIDTH &&
            y >= btnY && y <= btnY + BUTTON_HEIGHT) {
            return i;
        }
    }
    return -1;
}

// ===== Execute Button Action =====
void executeButtonAction(uint8_t index) {
    if (index >= BUTTON_COUNT) return;

    Button& btn = buttons[index];

    if (!wifiConnected || !serverAvailable) {
        Serial.println("Cannot execute action - server not available");
        setButtonState(index, 4);
        drawButton(index);
        scheduleButtonReset(index, 600);
        return;
    }

    if (btn.actionType.length() == 0 && !btn.hasWidget) {
        Serial.println("No action configured for this button");
        scheduleButtonReset(index, 180);
        return;
    }

    if (btn.hasWidget) {
        if (btn.widgetType == "timer") {
            btn.timerRunning = !btn.timerRunning;
            btn.timerLastTick = millis();
            if (btn.timerRemaining <= 0) {
                btn.timerRemaining = (btn.timerDuration > 0) ? btn.timerDuration : 300;
            }
            btn.icon = btn.timerRunning ? "PAUSE" : "PLAY";
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d:%02d", btn.timerRemaining / 60, btn.timerRemaining % 60);
            btn.label = String(buf);
            Serial.printf("Timer button %d toggled. Running=%d Remaining=%d\n", index, btn.timerRunning, btn.timerRemaining);
            setButtonState(index, 3);
            drawButton(index);
            scheduleButtonReset(index, 250);
            return;
        } else if (btn.widgetType == "stopwatch") {
            btn.timerRunning = !btn.timerRunning;
            btn.timerLastTick = millis();
            btn.icon = btn.timerRunning ? "PAUSE" : "PLAY";
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d:%02d", btn.timerRemaining / 60, btn.timerRemaining % 60);
            btn.label = String(buf);
            setButtonState(index, 3);
            drawButton(index);
            scheduleButtonReset(index, 250);
            return;
        } else if (btn.widgetType == "pomodoro") {
            // Pomodoro taps are handled on release (tap = toggle/reset, hold = open page)
            pomoTapAction(index);
            return;
        }
    }

    if (btn.actionType == "switch_profile") {
        Serial.println("Switching profile via ESP32...");
        setButtonState(index, 2);
        drawButton(index);

        String url = String(SERVER_URL) + "/api/execute-action";
        DynamicJsonDocument doc(512);
        doc["actionType"] = "switch_profile";
        doc["actionData"] = btn.actionData; // Use the specific action data
        String jsonPayload;
        serializeJson(doc, jsonPayload);

        espHttpBegin(http, url);
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(jsonPayload);
        http.end();

        if (httpCode == 200) {
            Serial.println("Profile switched successfully! Syncing new profile...");
            setButtonState(index, 3);
            drawButton(index);
            syncProfile();
        } else {
            if (httpCode == 401) {
                Serial.println("API token rejected - set it in the NexusDeck-Setup portal");
            }
            setButtonState(index, 4);
            drawButton(index);
        }
        scheduleButtonReset(index, 600);
        return;
    }

    if (btn.hasWidget || btn.actionType == "widget" || btn.actionType == "custom" ||
        btn.actionType == "navigate") {
        Serial.println("Widget/Custom action triggered on ESP32");
        setButtonState(index, 3); // Success state
        drawButton(index);
        scheduleButtonReset(index, 500);
        return;
    }

    if (btn.actionType == "calendar") {
        Serial.println("Opening Google Calendar...");
        // The page itself is the feedback: never paint the grid button
        // (or its scheduled reset) over the open page.
        openCalendarPage();
        setButtonState(index, 0);
        return;
    }

    if (btn.actionType == "spotify") {
        Serial.println("Opening Spotify...");
        openSpotifyPage();
        setButtonState(index, 0);
        return;
    }

    if (btn.actionType == "music") {
        Serial.println("Opening music player...");
        openMusicPage();
        setButtonState(index, 0);
        return;
    }

    // Send action to server
    setButtonState(index, 2); // Running state
    drawButton(index);

    // Build request. actionData is the full action JSON (including every field).
    String url = String(SERVER_URL) + "/api/execute-action";

    DynamicJsonDocument doc(1536);
    doc["actionType"] = btn.actionType;
    doc["actionData"] = btn.actionData;

    String jsonPayload;
    serializeJson(doc, jsonPayload);
    Serial.print("Action payload: ");
    Serial.println(jsonPayload);

    http.setTimeout(20000);
    espHttpBegin(http, url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);
    String responseBody = http.getString();
    bool succeeded = (httpCode == 200);

    if (succeeded && responseBody.length() > 0) {
        DynamicJsonDocument resultDoc(512);
        if (!deserializeJson(resultDoc, responseBody) && resultDoc.containsKey("success")) {
            succeeded = resultDoc["success"] | false;
        }
    }

    if (succeeded) {
        Serial.println("Action executed successfully");
        setButtonState(index, 3); // Success state
    } else {
        Serial.print("Action failed with code: ");
        Serial.println(httpCode);
        if (httpCode == 401) {
            Serial.println("API token rejected - set it in the NexusDeck-Setup portal");
        }
        Serial.print("Action response: ");
        Serial.println(responseBody);
        setButtonState(index, 4); // Error state
    }

    http.end();
    drawButton(index);
    scheduleButtonReset(index, 800);
}

// ===== Sync Profile from Server =====
// ===== Apply Profile Payload (shared by WiFi sync + SD load) =====
void applyProfilePayload(const String& payload) {
    DynamicJsonDocument doc(12288);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print("Profile JSON parse failed: ");
        Serial.println(error.c_str());
    } else if (doc.containsKey("buttons")) {
        if (doc.containsKey("name")) {
            currentProfileName = doc["name"].as<String>();
        }
        if (!bgColorCustom) {
            const char* backgroundHex = "#1a1a2e";
            if (doc.containsKey("backgroundColor")) {
                backgroundHex = doc["backgroundColor"] | "#1a1a2e";
            } else if (doc.containsKey("background")) {
                backgroundHex = doc["background"] | "#1a1a2e";
            }
            deckBackgroundColor = parseColor(String(backgroundHex));
        }

        JsonArray buttonsArray = doc["buttons"];
        String profileSignature;
        profileSignature += String(deckBackgroundColor) + "|";

        for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
            if (i >= buttonsArray.size()) {
                buttons[i].label = String(i + 1);
                buttons[i].icon = "";
                buttons[i].color = TFT_DARK_BG;
                buttons[i].actionType = "";
                buttons[i].actionData = "{}";
                buttons[i].hasWidget = false;
                buttons[i].widgetType = "";
                buttons[i].timerRunning = false;
                buttons[i].timerRemaining = 0;
                buttons[i].pomoPhase = 0;
                buttons[i].pomoDone = 0;
                buttons[i].pomoTask = "";
                buttons[i].pomoAlert = false;
                continue;
            }

            JsonObject btnObj = buttonsArray[i];

            buttons[i].label = btnObj["label"] | "";
            buttons[i].icon = btnObj["icon"] | "";
            buttons[i].color = parseColor(btnObj["color"] | "#1a1a2e");
            buttons[i].hasWidget = false;
            buttons[i].widgetType = "";
            buttons[i].pomoTask = "";

            if (btnObj.containsKey("action") && btnObj["action"].is<JsonObject>()) {
                JsonObject actionObj = btnObj["action"].as<JsonObject>();
                buttons[i].actionType = actionObj["type"] | "";
                buttons[i].actionData = "";
                serializeJson(actionObj, buttons[i].actionData);
            } else {
                buttons[i].actionType = "";
                buttons[i].actionData = "{}";
            }

            if (btnObj.containsKey("widget") && btnObj["widget"].is<JsonObject>()) {
                buttons[i].hasWidget = true;
                buttons[i].widgetType = btnObj["widget"]["type"] | "";
                if (buttons[i].widgetType == "timer") {
                    if (!buttons[i].timerRunning) {
                        int colonIdx = buttons[i].label.indexOf(':');
                        if (colonIdx > 0) {
                            int m = buttons[i].label.substring(0, colonIdx).toInt();
                            int s = buttons[i].label.substring(colonIdx + 1).toInt();
                            buttons[i].timerDuration = (m * 60) + s;
                        }
                        if (buttons[i].timerDuration <= 0) {
                            buttons[i].timerDuration = 300;
                        }
                        buttons[i].timerRemaining = buttons[i].timerDuration;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%02d:%02d", buttons[i].timerRemaining / 60, buttons[i].timerRemaining % 60);
                        buttons[i].label = String(buf);
                        if (buttons[i].icon.length() == 0) {
                            buttons[i].icon = "PLAY";
                        }
                    }
                } else if (buttons[i].widgetType == "stopwatch") {
                    if (!buttons[i].timerRunning && buttons[i].timerRemaining == 0) {
                        buttons[i].label = "00:00";
                        if (buttons[i].icon.length() == 0) {
                            buttons[i].icon = "PLAY";
                        }
                    }
                } else if (buttons[i].widgetType == "pomodoro") {
                    int workMin = 25, shortMin = 5, longMin = 15, cyc = 4;
                    buttons[i].pomoTask = "";
                    if (!btnObj["widget"]["config"].isNull()) {
                        workMin = btnObj["widget"]["config"]["workMinutes"] | 25;
                        shortMin = btnObj["widget"]["config"]["shortBreakMinutes"] | 5;
                        longMin = btnObj["widget"]["config"]["longBreakMinutes"] | 15;
                        cyc = btnObj["widget"]["config"]["sessionsBeforeLong"] | 4;
                        buttons[i].pomoTask = btnObj["widget"]["config"]["task"] | "";
                        buttons[i].pomoTask.trim();
                    }
                    buttons[i].pomoWorkSec = workMin * 60;
                    buttons[i].pomoShortSec = shortMin * 60;
                    buttons[i].pomoLongSec = longMin * 60;
                    buttons[i].pomoCycle = (cyc > 0) ? cyc : 4;
                    if (!buttons[i].timerRunning) {
                        buttons[i].pomoPhase = 0;
                        buttons[i].pomoDone = 0;
                        buttons[i].timerRemaining = buttons[i].pomoWorkSec;
                        buttons[i].pomoAlert = false;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%02d:%02d", buttons[i].timerRemaining / 60, buttons[i].timerRemaining % 60);
                        buttons[i].label = String(buf);
                        if (buttons[i].icon.length() == 0) {
                            buttons[i].icon = "PLAY";
                        }
                    }
                }
            }

            profileSignature += buttons[i].label + "|" + buttons[i].icon + "|" +
                buttons[i].actionType + "|" + buttons[i].actionData + "|" +
                String(buttons[i].color) + "|" + buttons[i].widgetType + ";";
        }

        static String lastProfileSignature;
        if (profileSignature != lastProfileSignature) {
            lastProfileSignature = profileSignature;
            if (!anyOverlayActive()) {
                drawAllButtons();
            }
            Serial.println("Profile synced successfully");
        }
    }
}

void syncProfile() {
    if (!wifiConnected) return;

    String url = String(SERVER_URL) + "/api/health";

    http.setTimeout(8000);
    espHttpBegin(http, url);
    int httpCode = http.GET();

    if (httpCode == 401) {
        Serial.println("API token rejected - set it in the NexusDeck-Setup portal");
        serverAvailable = false;
        http.end();
        return;
    }

    if (httpCode == 200) {
        serverAvailable = true;
        syncFailCount = 0;

        // Get profile data
        http.end();
        espHttpBegin(http, String(SERVER_URL) + "/api/get-profile");
        httpCode = http.GET();

        if (httpCode == 200) {
            String newServerIP = http.header("X-Server-IP");
            String newSSID = http.header("X-WiFi-SSID");
            String newPass = http.header("X-WiFi-Pass");
            if (newServerIP.length() > 0 && newServerIP != serverIP) {
                Serial.println("Server IP changed! Updating...");
                saveIP("http://" + newServerIP + ":8765");
                ESP.restart();
            }
            if (newSSID.length() > 0) {
                String savedSSID = preferences.getString("wifi_ssid", "");
                String savedPass = preferences.getString("wifi_pass", "");
                if (newSSID != savedSSID || newPass != savedPass) {
                    preferences.begin("deck", false);
                    preferences.putString("wifi_ssid", newSSID);
                    preferences.putString("wifi_pass", newPass);
                    preferences.end();
                    Serial.println("WiFi settings updated from web! Restarting...");
                    ESP.restart();
                }
            }
            String payload = http.getString();

            applyProfilePayload(payload);
            sdSaveProfile(currentProfileName, payload);
        }
    } else {
        serverAvailable = false;
        if (++syncFailCount >= 5) {
            Serial.println("Server unreachable, opening setup portal...");
            syncFailCount = 0;
            openSetupPortal();
        }
    }

    http.end();
    if (!anyOverlayActive()) {
        drawStatusBar();
    }
}

// ===== Update Live Widgets =====
void updateWidgets() {
    bool needsRedraw = false;

    if (wifiConnected && millis() - lastStatsFetch >= 5000) {
        updateSystemStats();
        lastStatsFetch = millis();
    }
    updateDHT();

    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        if (buttons[i].hasWidget) {
            if (buttons[i].widgetType == "timer") {
                if (buttons[i].timerRunning) {
                    if (millis() - buttons[i].timerLastTick >= 1000) {
                        buttons[i].timerLastTick = millis();
                        if (buttons[i].timerRemaining > 0) {
                            buttons[i].timerRemaining--;
                        } else {
                            buttons[i].timerRunning = false;
                            buttons[i].icon = "PLAY";
                        }
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%02d:%02d", buttons[i].timerRemaining / 60, buttons[i].timerRemaining % 60);
                        buttons[i].label = String(buf);
                        needsRedraw = true;
                    }
                }
            } else if (buttons[i].widgetType == "stopwatch") {
                if (buttons[i].timerRunning) {
                    if (millis() - buttons[i].timerLastTick >= 1000) {
                        buttons[i].timerLastTick = millis();
                        buttons[i].timerRemaining++;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%02d:%02d", buttons[i].timerRemaining / 60, buttons[i].timerRemaining % 60);
                        buttons[i].label = String(buf);
                        needsRedraw = true;
                    }
                }
            } else if (buttons[i].widgetType == "pomodoro") {
                tickPomo(buttons[i], true, i, needsRedraw);
            } else if (buttons[i].widgetType == "clock") {
                unsigned long secs = (millis() / 1000) % 86400;
                int h = (secs / 3600) % 24;
                int m = (secs % 3600) / 60;
                int s = secs % 60;
                char buf[16];
                snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
                buttons[i].label = String(buf);
                needsRedraw = true;
            } else if (buttons[i].widgetType == "uptime") {
                // Update uptime
                unsigned long uptime = millis() / 1000;
                buttons[i].label = String(uptime / 3600) + "h " + String((uptime % 3600) / 60) + "m";
                needsRedraw = true;
            } else if (buttons[i].widgetType == "cpu") {
                buttons[i].label = "CPU " + String(cpuPercent) + "%";
                needsRedraw = true;
            } else if (buttons[i].widgetType == "ram") {
                buttons[i].label = "RAM " + String(ramPercent) + "%";
                needsRedraw = true;
            }
        }
    }

    if (homePomo.timerRunning || homePomo.pomoAlert) {
        tickPomo(homePomo, false, 0, needsRedraw);
    }

    if (needsRedraw && !anyOverlayActive()) {
        for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
            if (buttons[i].hasWidget) drawButton(i);
        }
        drawStatusBar();
    }
}

void updateSystemStats() {
    HTTPClient statsHttp;
    espHttpBegin(statsHttp, String(SERVER_URL) + "/api/system-stats");
    int httpCode = statsHttp.GET();

    if (httpCode == 200) {
        StaticJsonDocument<512> statsDoc;
        DeserializationError error = deserializeJson(statsDoc, statsHttp.getString());
        if (!error) {
            cpuPercent = statsDoc["cpu"] | cpuPercent;
            ramPercent = statsDoc["ram"] | ramPercent;
        }
    }

    statsHttp.end();
}

// ===== Backlight Alert (phase-end notification) =====
// ===== Buzzer (non-blocking tone sequences over LEDC PWM) =====
struct ToneStep {
    int freq; // 0 = silence
    int ms;
};
static const ToneStep SEQ_CLICK[] = {{2500, 30}, {0, 0}};
static const ToneStep SEQ_ALARM_PHASE[] = {{880, 150}, {0, 100}, {880, 150}, {0, 100}, {880, 300}, {0, 0}};
static const ToneStep SEQ_ALARM_TIMER[] = {{1200, 120}, {0, 80}, {1200, 120}, {0, 80}, {1200, 120}, {0, 80}, {1200, 400}, {0, 0}};
static const ToneStep SEQ_ERROR[] = {{300, 200}, {0, 0}};

struct TonePlayer {
    const ToneStep* seq = nullptr;
    uint8_t pos = 0;
    unsigned long until = 0;
    bool sounding = false;
};
TonePlayer tonePlayer;

void toneOn(int freq) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(SPEAKER_PIN, freq, 8);
    ledcWriteTone(SPEAKER_PIN, freq);
#else
    ledcSetup(SPEAKER_CHANNEL, freq, 8);
    ledcAttachPin(SPEAKER_PIN, SPEAKER_CHANNEL);
    ledcWriteTone(SPEAKER_CHANNEL, freq);
#endif
}

void toneOff() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWriteTone(SPEAKER_PIN, 0);
    ledcDetach(SPEAKER_PIN);
#else
    ledcWriteTone(SPEAKER_CHANNEL, 0);
    ledcDetachPin(SPEAKER_PIN);
#endif
}

void playSeq(const ToneStep* seq) {
    tonePlayer.seq = seq;
    tonePlayer.pos = 0;
    tonePlayer.until = 0;
    tonePlayer.sounding = false;
}

void pumpBuzzer() {
    if (!tonePlayer.seq) return;
    unsigned long now = millis();
    if (tonePlayer.until != 0 && (long)(now - tonePlayer.until) < 0) return;
    if (tonePlayer.sounding) {
        toneOff();
        tonePlayer.sounding = false;
    }
    ToneStep step = tonePlayer.seq[tonePlayer.pos];
    if (step.ms <= 0) {
        tonePlayer.seq = nullptr;
        tonePlayer.until = 0;
        return;
    }
    if (step.freq > 0) {
        toneOn(step.freq);
        tonePlayer.sounding = true;
    }
    tonePlayer.until = now + step.ms;
    tonePlayer.pos++;
}

void clickSound() {
    if (ENABLE_CLICK) playSeq(SEQ_CLICK);
}

// ===== DHT22 comfort sensor: real room temp/humidity =====
void updateDHT() {
    if (millis() - lastDhtRead < 30000) return;
    lastDhtRead = millis();
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) {
        dhtTempC = temp;
        dhtHumidity = hum;
        dhtOk = true;
        homeTemp = String((int)(temp >= 0 ? temp + 0.5f : temp - 0.5f)) + "C";
        Serial.printf("Room: %.1fC %.0f%%\n", temp, hum);
    } else if (!dhtOk) {
        Serial.println("DHT not detected (optional)");
    }
}

// ===== PIR presence: wake on motion, sleep the screen when idle =====
bool pirSeenMotion = false;

void updatePresence() {
    if (digitalRead(PIR_PIN) == HIGH) pirSeenMotion = true;
    bool motion = (digitalRead(PIR_PIN) == HIGH) || touchDown;
    if (motion) {
        lastMotionAt = millis();
        if (!screenAwake) {
            screenAwake = true;
            applyBacklight(true);
            Serial.println("Presence: wake");
        }
    } else if (screenAwake && !backlightAlert && pirSeenMotion &&
               millis() - lastMotionAt > (unsigned long)PIR_IDLE_SEC * 1000UL) {
        screenAwake = false;
        applyBacklight(false);
        Serial.println("Presence: sleep");
    }
}

// ===== SD card (profiles backup/offline + MP3 library) =====
bool sdReady = false;

String sdSafeName(const String& name) {
    String safe;
    for (unsigned int i = 0; i < name.length() && safe.length() < 48; i++) {
        char c = name.charAt(i);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') {
            safe += c;
        }
    }
    if (safe.length() == 0) safe = "profile";
    return safe;
}

bool setupSD() {
    // SD slot shares the touch SPI bus on this unit and cannot coexist
    // with it: music now streams from the companion over WiFi instead.
    // (Kept as a stub so callers stay untouched.)
    sdReady = false;
    Serial.println("SD: disabled (music streams from server)");
    return false;
}

void sdSaveProfile(const String& name, const String& payload) {
    (void)name;
    (void)payload;
    // Disabled: SD shares the touch bus on this unit (see setupSD).
}

bool sdLoadProfile(const String& name) {
    (void)name;
    return false;
}

bool sdLoadLastProfile() {
    return false;
}

void alarmSound(uint8_t kind) {
    if (kind == 1) playSeq(SEQ_ALARM_TIMER);
    else playSeq(SEQ_ALARM_PHASE);
}

void applyBacklight(bool on) {
    analogWrite(TFT_BACKLIGHT, on ? screenBrightness : 0);
}

void triggerBacklightAlert(unsigned long durationMs) {
    backlightAlert = true;
    backlightAlertUntil = millis() + durationMs;
    backlightLastToggle = 0;
    backlightLevel = true;
    applyBacklight(true);
}

void updateBacklight() {
    if (!backlightAlert) return;
    if ((long)(millis() - backlightAlertUntil) >= 0) {
        backlightAlert = false;
        applyBacklight(true);
        backlightLevel = true;
        return;
    }
    if (millis() - backlightLastToggle >= 250) {
        backlightLastToggle = millis();
        backlightLevel = !backlightLevel;
        applyBacklight(backlightLevel);
    }
}

// ===== Pomodoro Helpers =====
int pomoPhaseDuration(Button& btn) {
    int dur = (btn.pomoPhase == 0) ? btn.pomoWorkSec : (btn.pomoPhase == 1 ? btn.pomoShortSec : btn.pomoLongSec);
    return (dur > 0) ? dur : 300;
}

const char* pomoPhaseName(Button& btn) {
    return (btn.pomoPhase == 0) ? "FOCUS" : (btn.pomoPhase == 1 ? "SHORT BREAK" : "LONG BREAK");
}

uint16_t pomoPhaseColor(Button& btn) {
    return (btn.pomoPhase == 0) ? TFT_ORANGE : (btn.pomoPhase == 1 ? TFT_GREEN : TFT_CYAN);
}

void tickPomo(Button& btn, bool gridFeedback, uint8_t idx, bool& needsRedraw) {
    if (btn.pomoAlert && (long)(millis() - btn.pomoAlertUntil) >= 0) {
        btn.pomoAlert = false;
        btn.icon = btn.timerRunning ? "PAUSE" : "PLAY";
        needsRedraw = true;
        pomoPageDirty = true;
    }
    if (btn.timerRunning && millis() - btn.timerLastTick >= 1000) {
        btn.timerLastTick = millis();
        if (btn.timerRemaining > 0) {
            btn.timerRemaining--;
        }
        if (btn.timerRemaining <= 0) {
            if (btn.pomoPhase == 0) {
                btn.pomoDone++;
                uint8_t cycle = btn.pomoCycle > 0 ? btn.pomoCycle : 4;
                btn.pomoPhase = (btn.pomoDone % cycle == 0) ? 2 : 1;
                Serial.printf("Pomodoro session %d finished!\n", btn.pomoDone);
            } else {
                btn.pomoPhase = 0;
                Serial.println("Pomodoro break finished!");
            }
            int dur = btn.pomoPhase == 0 ? btn.pomoWorkSec : (btn.pomoPhase == 1 ? btn.pomoShortSec : btn.pomoLongSec);
            if (dur <= 0) dur = 300;
            btn.timerRemaining = dur;
            btn.pomoAlert = true;
            btn.pomoAlertUntil = millis() + 6000;
            triggerBacklightAlert(6000);
            alarmSound(btn.widgetType == "timer" ? 1 : 0);
            pomoPageDirty = true;
            if (gridFeedback) {
                setButtonState(idx, 3);
                scheduleButtonReset(idx, 800);
            }
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", btn.timerRemaining / 60, btn.timerRemaining % 60);
        btn.label = String(buf);
        needsRedraw = true;
    }
    if (btn.pomoAlert) {
        btn.icon = ((millis() / 500) % 2 == 0) ? "DONE" : "PLAY";
        needsRedraw = true;
    }
}

Button& pomoButtonByIndex(int index) {
    if (index == 255) return homePomo;
    return buttons[index];
}

Button& pomoViewButton() {
    return pomoButtonByIndex(pomoPageIndex);
}

void pomoToggleRunBtn(Button& btn) {
    btn.timerRunning = !btn.timerRunning;
    btn.timerLastTick = millis();
    if (btn.timerRemaining <= 0) {
        btn.timerRemaining = pomoPhaseDuration(btn);
    }
    btn.icon = btn.timerRunning ? "PAUSE" : "PLAY";
    pomoPageDirty = true;
    homePageDirty = true;
}

void pomoResetBtn(Button& btn) {
    btn.timerRunning = false;
    btn.pomoPhase = 0;
    btn.pomoDone = 0;
    btn.timerRemaining = (btn.pomoWorkSec > 0) ? btn.pomoWorkSec : 1500;
    btn.pomoAlert = false;
    btn.icon = "PLAY";
    pomoPageDirty = true;
    homePageDirty = true;
}

void pomoToggleRun(uint8_t index) {
    if (index != 255 && index >= BUTTON_COUNT) return;
    pomoToggleRunBtn(pomoButtonByIndex(index));
}

void pomoResetSession(uint8_t index) {
    if (index != 255 && index >= BUTTON_COUNT) return;
    pomoResetBtn(pomoButtonByIndex(index));
}

void pomoTapAction(uint8_t index) {
    if (index >= BUTTON_COUNT) return;
    Button& btn = buttons[index];
    // Double tap (release within 600ms) = reset session, single tap = start/pause
    unsigned long now = millis();
    if (now - btn.pomoLastTap < 600) {
        btn.pomoLastTap = 0;
        pomoResetSession(index);
        Serial.printf("Pomodoro button %d reset\n", index);
    } else {
        btn.pomoLastTap = now;
        pomoToggleRun(index);
        Serial.printf("Pomodoro button %d toggled. Running=%d Phase=%d Remaining=%d\n", index, btn.timerRunning, btn.pomoPhase, btn.timerRemaining);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", btn.timerRemaining / 60, btn.timerRemaining % 60);
    btn.label = String(buf);
    setButtonState(index, 3);
    if (!anyOverlayActive()) {
        drawButton(index);
    }
    scheduleButtonReset(index, 250);
}

// ===== Pomodoro Dedicated Page =====
bool anyOverlayActive() {
    return pomoPageActive || homePageActive || calPageActive || musicPageActive || spotPageActive;
}

void openPomoPage(uint8_t index) {
    if (index != 255 && index >= BUTTON_COUNT) return;
    pomoPageIndex = index;
    pomoPageActive = true;
    pomoPageLastSecond = -1;
    pomoPageDirty = true;
    ignoreTouchUntil = millis() + 400;
    Serial.printf("Pomodoro page opened for button %d\n", index);
    drawPomoPage();
}

void closePomoPage() {
    pomoPageActive = false;
    pomoPageIndex = -1;
    if (pomoReturnHome) {
        pomoReturnHome = false;
        drawHomePage();
    } else {
        drawAllButtons();
        drawStatusBar();
    }
    Serial.println("Pomodoro page closed");
}

// ===== Google Calendar events page =====
void drawCalDetail(uint8_t index) {
    CalEvent& ev = calEvents[index];
    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString("EVENT", SCREEN_WIDTH / 2, 12, 2);
    tft.drawFastHLine(10, 28, SCREEN_WIDTH - 20, TFT_DARKGREY);

    tft.setTextColor(TFT_CYAN, deckBackgroundColor);
    tft.drawString(truncateText(ev.summary, 24), SCREEN_WIDTH / 2, 42, 2);
    String range = ev.when;
    if (ev.end.length() > 0) range += " - " + ev.end;
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString(truncateText(range, 26), SCREEN_WIDTH / 2, 62, 2);
    int y = 82;
    if (ev.location.length() > 0) {
        tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
        tft.drawString(truncateText(String("LOC: ") + ev.location, 34), SCREEN_WIDTH / 2, y, 1);
        y += 16;
    }
    // Greedy word-wrap of the description into at most 6 lines
    String rest = ev.desc;
    rest.replace("\n", " ");
    rest.replace("\r", " ");
    for (uint8_t line = 0; line < 6 && rest.length() > 0; line++) {
        String chunk = rest;
        if (chunk.length() > 34) {
            int cut = 34;
            while (cut > 20 && chunk.charAt(cut) != ' ') cut--;
            if (chunk.charAt(cut) != ' ') cut = 34;
            chunk = rest.substring(0, cut);
            rest = rest.substring(cut);
            rest.trim();
        } else {
            rest = "";
        }
        tft.setTextColor(TFT_WHITE, deckBackgroundColor);
        tft.drawString(chunk, SCREEN_WIDTH / 2, y, 1);
        y += 15;
    }
    tft.setTextColor(TFT_DARKGREY, deckBackgroundColor);
    tft.drawString("tap: back", SCREEN_WIDTH / 2, 228, 1);
}

void drawCalendarPage() {
    if (calDetail >= 0 && calDetail < calCount) {
        drawCalDetail((uint8_t)calDetail);
        return;
    }
    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString("CALENDAR", SCREEN_WIDTH / 2, 12, 2);
    tft.drawFastHLine(10, 28, SCREEN_WIDTH - 20, TFT_DARKGREY);

    if (calError.length() > 0) {
        tft.setTextColor(TFT_YELLOW, deckBackgroundColor);
        tft.drawString(truncateText(calError, 26), SCREEN_WIDTH / 2, 60, 2);
        tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
        tft.drawString("Connect in Studio > System", SCREEN_WIDTH / 2, 84, 1);
        tft.drawString("tap to close", SCREEN_WIDTH / 2, 220, 1);
        return;
    }
    if (calCount == 0) {
        tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
        tft.drawString("No upcoming events", SCREEN_WIDTH / 2, 60, 2);
        tft.drawString("tap to close", SCREEN_WIDTH / 2, 220, 1);
        return;
    }
    uint8_t shown = calCount > 7 ? 7 : calCount;
    for (uint8_t i = 0; i < shown; i++) {
        int y = 44 + i * 24;
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(TFT_CYAN, deckBackgroundColor);
        tft.drawString(truncateText(calEvents[i].when, 12), 12, y, 2);
        tft.setTextColor(TFT_WHITE, deckBackgroundColor);
        tft.drawString(truncateText(calEvents[i].summary, 20), 100, y, 2);
    }
    if (calCount > shown) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
        tft.drawString(String("+") + String(calCount - shown) + " more", SCREEN_WIDTH / 2, 44 + shown * 24, 1);
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_DARKGREY, deckBackgroundColor);
    tft.drawString("tap to close", SCREEN_WIDTH / 2, 228, 1);
}

void openCalendarPage() {
    calPageActive = true;
    calOpenedAt = millis();
    calCount = 0;
    calError = "";
    calDetail = -1;
    ignoreTouchUntil = millis() + 400;
    if (!wifiConnected) {
        calError = "No WiFi";
        drawCalendarPage();
        Serial.println("Calendar: no WiFi");
        return;
    }
    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString("Loading events...", SCREEN_WIDTH / 2, 60, 2);

    HTTPClient h;
    h.setTimeout(10000);
    espHttpBegin(h, String(SERVER_URL) + "/api/google/events?limit=8");
    if (h.GET() != 200) {
        calError = "Server error";
        h.end();
        drawCalendarPage();
        return;
    }
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, h.getString())) {
        calError = "Bad response";
        h.end();
        drawCalendarPage();
        return;
    }
    h.end();
    if (!doc["success"]) {
        calError = doc["error"] | "Not connected";
        drawCalendarPage();
        Serial.print("Calendar error: ");
        Serial.println(calError);
        return;
    }
    JsonArray items = doc["events"];
    for (uint8_t i = 0; i < 8 && i < items.size(); i++) {
        calEvents[i].when = items[i]["when"] | "";
        calEvents[i].end = items[i]["end"] | "";
        calEvents[i].summary = items[i]["summary"] | "";
        calEvents[i].location = items[i]["location"] | "";
        calEvents[i].desc = items[i]["description"] | "";
        calCount++;
    }
    drawCalendarPage();
    Serial.printf("Calendar opened with %d events\n", calCount);
}

void closeCalendarPage() {
    calPageActive = false;
    drawAllButtons();
    drawStatusBar();
    Serial.println("Calendar page closed");
}

// ===== Spotify now-playing page (control + display) =====
bool spotFetchNow() {
    if (!wifiConnected) {
        spotError = "No WiFi";
        return false;
    }
    HTTPClient h;
    h.setTimeout(10000);
    espHttpBegin(h, String(SERVER_URL) + "/api/spotify/now");
    if (h.GET() != 200) {
        spotError = "Server error";
        h.end();
        return false;
    }
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, h.getString())) {
        h.end();
        spotError = "Bad response";
        return false;
    }
    h.end();
    if (!doc["success"]) {
        spotError = doc["error"] | "Not connected";
        return false;
    }
    JsonObject now = doc["now"];
    spotTrack = now["track"] | "";
    spotArtist = now["artist"] | "";
    spotPlaying = now["playing"] | false;
    spotProgressMs = now["progress_ms"] | 0;
    spotDurationMs = now["duration_ms"] | 0;
    spotError = "";
    return true;
}

bool spotSendOperation(const String& operation) {
    if (!wifiConnected) return false;
    HTTPClient h;
    h.setTimeout(10000);
    espHttpBegin(h, String(SERVER_URL) + "/api/execute-action");
    h.addHeader("Content-Type", "application/json");
    DynamicJsonDocument doc(256);
    doc["actionType"] = "spotify";
    doc["actionData"] = String("{\"operation\":\"") + operation + "\"}";
    String payload;
    serializeJson(doc, payload);
    int code = h.POST(payload);
    h.end();
    return code == 200;
}

void drawSpotifyPage() {
    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString("SPOTIFY", SCREEN_WIDTH / 2, 12, 2);
    tft.drawFastHLine(10, 28, SCREEN_WIDTH - 20, TFT_DARKGREY);
    if (spotError.length() > 0) {
        tft.setTextColor(TFT_YELLOW, deckBackgroundColor);
        tft.drawString(truncateText(spotError, 26), SCREEN_WIDTH / 2, 60, 2);
        tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
        tft.drawString("Connect in Studio > System", SCREEN_WIDTH / 2, 84, 1);
        tft.drawString("tap to close", SCREEN_WIDTH / 2, 220, 1);
        return;
    }
    tft.setTextColor(spotPlaying ? TFT_GREEN : TFT_LIGHTGREY, deckBackgroundColor);
    tft.drawString(spotPlaying ? "PLAYING" : "PAUSED", SCREEN_WIDTH / 2, 42, 2);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString(truncateText(spotTrack.length() ? spotTrack : "Nothing playing", 24),
                   SCREEN_WIDTH / 2, 68, 2);
    tft.setTextColor(TFT_CYAN, deckBackgroundColor);
    tft.drawString(truncateText(spotArtist, 26), SCREEN_WIDTH / 2, 88, 2);
    if (spotDurationMs > 0) {
        float frac = (float)spotProgressMs / (float)spotDurationMs;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        int barX = 40, barW = SCREEN_WIDTH - 80, barY = 108, barH = 8;
        tft.drawRect(barX, barY, barW, barH, TFT_DARKGREY);
        tft.fillRect(barX + 1, barY + 1, (int)((barW - 2) * frac), barH - 2, TFT_GREEN);
    }
    // Transport: PREV | PLAY/PAUSE | NEXT | BACK
    const char* keys[4] = {"|<", spotPlaying ? "PAUSE" : "PLAY", ">|", "BACK"};
    const int xs[4] = {8, 72, 170, 234};
    const int ws[4] = {60, 94, 60, 78};
    for (uint8_t b = 0; b < 4; b++) {
        tft.fillRoundRect(xs[b], 150, ws[b], 44, 6, tft.color565(40, 40, 60));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, tft.color565(40, 40, 60));
        tft.drawString(keys[b], xs[b] + ws[b] / 2, 172, 2);
    }
    tft.setTextColor(TFT_DARKGREY, deckBackgroundColor);
    tft.drawString("tap outside = close", SCREEN_WIDTH / 2, 228, 1);
}

void openSpotifyPage() {
    spotPageActive = true;
    spotOpenedAt = millis();
    spotLastRefresh = 0;
    ignoreTouchUntil = millis() + 400;
    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString("Loading Spotify...", SCREEN_WIDTH / 2, 60, 2);
    spotFetchNow();
    drawSpotifyPage();
    Serial.println("Spotify page opened");
}

void closeSpotifyPage() {
    spotPageActive = false;
    drawAllButtons();
    drawStatusBar();
    Serial.println("Spotify page closed");
}

void handleSpotTouch() {
    if (!touch.tirqTouched() || !touch.touched()) return;
    if (millis() < ignoreTouchUntil) return;
    static unsigned long spotTouchDebounce = 0;
    if (millis() < spotTouchDebounce) return;
    TS_Point p = touch.getPoint();
    uint16_t x = constrain(map(p.x, 200, 3700, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
    uint16_t y = constrain(map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);
    spotTouchDebounce = millis() + 300;
    ignoreTouchUntil = millis() + 400;
    if (y >= 150 && y <= 194) {
        String op = "";
        if (x < 72) op = "previous";
        else if (x < 170) op = "toggle";
        else if (x < 234) op = "next";
        else {
            closeSpotifyPage();
            return;
        }
        if (spotSendOperation(op)) {
            delay(400);
            spotFetchNow();
        }
        drawSpotifyPage();
        return;
    }
    closeSpotifyPage();
}

// ===== MP3 player (SD /mp3, independent of the PC) =====
void musicEnsureOutput() {
    if (!mp3Out) {
        mp3Out = new AudioOutputI2S(0, 1); // internal DAC (pins 25+26)
        mp3Out->SetOutputModeMono(true);
        mp3Out->SetGain(mp3Gain);
    }
}

String urlEncodeName(const String& name) {
    const char* hex = "0123456789ABCDEF";
    String out;
    for (unsigned int i = 0; i < name.length(); i++) {
        char c = name.charAt(i);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += c;
        } else {
            out += '%';
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        }
    }
    return out;
}

void musicScanSD() {
    // Track list comes from the companion (Documents/NexusDeck/music).
    mp3Count = 0;
    mp3Index = -1;
    if (!wifiConnected) return;
    HTTPClient h;
    h.setTimeout(8000);
    espHttpBegin(h, String(SERVER_URL) + "/api/music/tracks");
    if (h.GET() != 200) {
        h.end();
        return;
    }
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, h.getString())) {
        h.end();
        return;
    }
    h.end();
    if (!doc["success"]) return;
    JsonArray arr = doc["tracks"];
    for (uint8_t i = 0; i < 12 && i < arr.size(); i++) {
        mp3Tracks[mp3Count++] = arr[i].as<String>();
    }
}

void musicStop() {
    if (mp3Decoder) {
        mp3Decoder->stop();
        delete mp3Decoder;
        mp3Decoder = nullptr;
    }
    if (mp3Buff) {
        mp3Buff->close();
        delete mp3Buff;
        mp3Buff = nullptr;
    }
    if (mp3Source) {
        mp3Source->close();
        delete mp3Source;
        mp3Source = nullptr;
    }
    musicPlaying = false;
}

void musicPlay(int8_t index) {
    musicStop();
    if (!wifiConnected || mp3Count == 0) return;
    if (index < 0 || index >= mp3Count) index = 0;
    musicEnsureOutput();
    mp3Out->SetGain(mp3Gain);
    String url = String(SERVER_URL) + "/music/" + urlEncodeName(mp3Tracks[index]);
    // The file endpoint requires the pairing token; the stream reader
    // cannot set headers, so it travels as a query parameter instead.
    if (apiToken.length() > 0) {
        url += "?token=" + apiToken;
    }
    mp3Source = new AudioFileSourceHTTPStream();
    if (!mp3Source->open(url.c_str())) {
        Serial.printf("Cannot open stream: %s\n", mp3Tracks[index].c_str());
        musicStop();
        return;
    }
    mp3Buff = new AudioFileSourceBuffer(mp3Source, 8192);
    mp3Decoder = new AudioGeneratorMP3();
    if (mp3Decoder->begin(mp3Buff, mp3Out)) {
        mp3Index = index;
        musicPlaying = true;
        Serial.printf("Playing: %s\n", mp3Tracks[index].c_str());
    } else {
        Serial.printf("Cannot play: %s\n", mp3Tracks[index].c_str());
        musicStop();
    }
}

void musicPump() {
    if (musicPlaying && mp3Decoder) {
        bool chunkOk = false;
        if (mp3Decoder->isRunning()) {
            chunkOk = mp3Decoder->loop();
            if (!chunkOk) {
                Serial.println("Track finished");
                musicStop();
            }
        } else {
            musicStop();
        }
        if (musicPageActive && !chunkOk) drawMusicPage();
    }
}

void drawMusicPage() {
    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString("MUSIC", SCREEN_WIDTH / 2, 12, 2);
    tft.drawFastHLine(10, 28, SCREEN_WIDTH - 20, TFT_DARKGREY);
    if (!wifiConnected) {
        tft.setTextColor(TFT_YELLOW, deckBackgroundColor);
        tft.drawString("No WiFi", SCREEN_WIDTH / 2, 60, 2);
    } else if (mp3Count == 0) {
        tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
        tft.drawString("Put .mp3 in music folder", SCREEN_WIDTH / 2, 60, 2);
    } else {
        uint8_t shown = mp3Count > 5 ? 5 : mp3Count;
        uint8_t start = 0;
        if (mp3Index >= 0 && mp3Index >= shown) {
            start = mp3Index - shown + 1;
            if (start + shown > mp3Count) start = mp3Count - shown;
        }
        for (uint8_t r = 0; r < shown; r++) {
            uint8_t i = start + r;
            int y = 44 + r * 24;
            bool current = (i == mp3Index);
            if (current) {
                tft.fillRoundRect(8, y - 11, SCREEN_WIDTH - 16, 22, 4, tft.color565(30, 60, 30));
            }
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(current ? TFT_GREEN : TFT_WHITE, current ? tft.color565(30, 60, 30) : deckBackgroundColor);
            String label = mp3Tracks[i];
            if (label.startsWith("/")) label = label.substring(1);
            if (label.endsWith(".mp3") || label.endsWith(".MP3")) label = label.substring(0, label.length() - 4);
            tft.drawString(truncateText(label, 24), 14, y, 2);
        }
    }
    // Transport row: |< PLAY STOP >| VOL- BACK
    const char* keys[6] = {"|<", "PLAY", "STOP", ">|", "VOL-", "BACK"};
    const int xs[6] = {8, 59, 121, 183, 227, 271};
    for (uint8_t b = 0; b < 6; b++) {
        int w = (b == 1 || b == 2) ? 62 : 44;
        int x = xs[b];
        if (x + w > SCREEN_WIDTH - 8) w = SCREEN_WIDTH - 8 - x;
        tft.fillRoundRect(x, 196, w, 36, 6, tft.color565(40, 40, 60));
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, tft.color565(40, 40, 60));
        tft.drawString(keys[b], x + w / 2, 214, 2);
    }
}

void openMusicPage() {
    musicPageActive = true;
    musicOpenedAt = millis();
    ignoreTouchUntil = millis() + 400;
    musicScanSD();
    drawMusicPage();
    Serial.println("Music page opened");
}

void closeMusicPage() {
    musicPageActive = false;
    musicStop();
    drawAllButtons();
    drawStatusBar();
    Serial.println("Music page closed");
}

void handleMusicTouch() {
    if (!touchDown) return;
    if (millis() < ignoreTouchUntil) return;
    static unsigned long musicTouchDebounce = 0;
    if (millis() < musicTouchDebounce) return;
    TS_Point p = touchPoint;
    uint16_t x = constrain(map(p.x, 200, 3700, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
    uint16_t y = constrain(map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);
    musicTouchDebounce = millis() + 300;
    ignoreTouchUntil = millis() + 400;
    if (y >= 196) {
        if (x < 59) {
            if (mp3Count > 0) musicPlay((mp3Index <= 0) ? mp3Count - 1 : mp3Index - 1);
        } else if (x < 121) {
            if (musicPlaying) musicStop();
            else if (mp3Index >= 0) musicPlay(mp3Index);
            else musicPlay(0);
        } else if (x < 183) {
            musicStop();
        } else if (x < 227) {
            if (mp3Count > 0) musicPlay((mp3Index >= (int8_t)mp3Count - 1) ? 0 : mp3Index + 1);
        } else if (x < 271) {
            mp3Gain -= 0.1f;
            if (mp3Gain < 0.1f) mp3Gain = 0.1f;
            if (mp3Out) mp3Out->SetGain(mp3Gain);
            preferences.begin("deck", false);
            preferences.putFloat("mp3_gain", mp3Gain);
            preferences.end();
        } else {
            closeMusicPage();
            return;
        }
        drawMusicPage();
        return;
    }
    uint8_t shown = mp3Count > 5 ? 5 : mp3Count;
    uint8_t start = 0;
    if (mp3Index >= 0 && mp3Index >= shown) {
        start = mp3Index - shown + 1;
        if (start + shown > mp3Count) start = mp3Count - shown;
    }
    for (uint8_t r = 0; r < shown; r++) {
        int rowY = 44 + r * 24;
        if (y >= rowY - 12 && y <= rowY + 12) {
            musicPlay(start + r);
            drawMusicPage();
            return;
        }
    }
}

void handleCalTouch() {
    if (!touchDown) return;
    if (millis() < ignoreTouchUntil) return;
    static unsigned long calTouchDebounce = 0;
    if (millis() < calTouchDebounce) return;
    TS_Point p = touchPoint;
    uint16_t y = constrain(map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);
    calTouchDebounce = millis() + 300;
    ignoreTouchUntil = millis() + 400;
    if (calDetail >= 0) {
        calDetail = -1; // detail -> back to list
        drawCalendarPage();
        return;
    }
    // Tap a row to open its details, tap elsewhere to close
    uint8_t shown = calCount > 7 ? 7 : calCount;
    for (uint8_t i = 0; i < shown; i++) {
        int rowY = 44 + i * 24;
        if (y >= rowY - 12 && y <= rowY + 12) {
            calDetail = i;
            drawCalendarPage();
            return;
        }
    }
    closeCalendarPage();
}

void drawPomoPage() {
    if (pomoPageIndex != 255 && (pomoPageIndex < 0 || pomoPageIndex >= BUTTON_COUNT)) return;
    Button& btn = pomoViewButton();

    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(TC_DATUM);

    // Title (task name when set)
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    if (btn.pomoTask.length() > 0) {
        tft.drawString(truncateText(btn.pomoTask, 18), SCREEN_WIDTH / 2, 2, 2);
    } else {
        tft.drawString("POMODORO", SCREEN_WIDTH / 2, 2, 2);
    }

    // Phase name
    uint16_t phaseColor = pomoPhaseColor(btn);
    tft.setTextColor(phaseColor, deckBackgroundColor);
    tft.drawString(pomoPhaseName(btn), SCREEN_WIDTH / 2, 22, 4);

    // Big countdown (alert shows DONE!)
    tft.setTextColor(btn.pomoAlert ? TFT_YELLOW : TFT_WHITE, deckBackgroundColor);
    if (btn.pomoAlert) {
        tft.drawString("DONE!", SCREEN_WIDTH / 2, 52, 4);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", btn.timerRemaining / 60, btn.timerRemaining % 60);
    tft.drawString(String(buf), SCREEN_WIDTH / 2, 76, 7);

    // Session dots
    uint8_t cycle = btn.pomoCycle > 0 ? btn.pomoCycle : 4;
    if (cycle > 8) cycle = 8;
    uint8_t doneInCycle = btn.pomoDone % (btn.pomoCycle > 0 ? btn.pomoCycle : 4);
    int dotsWidth = (cycle - 1) * 24;
    int dotsX = SCREEN_WIDTH / 2 - dotsWidth / 2;
    for (uint8_t d = 0; d < cycle; d++) {
        if (d < doneInCycle) {
            tft.fillCircle(dotsX + d * 24, 152, 6, phaseColor);
        } else {
            tft.drawCircle(dotsX + d * 24, 152, 6, TFT_DARKGREY);
        }
    }

    // Progress bar
    int total = pomoPhaseDuration(btn);
    float frac = (total > 0) ? (1.0f - (float)btn.timerRemaining / (float)total) : 0.0f;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int barX = 40, barW = SCREEN_WIDTH - 80, barY = 166, barH = 8;
    tft.drawRect(barX, barY, barW, barH, TFT_DARKGREY);
    tft.fillRect(barX + 1, barY + 1, (int)((barW - 2) * frac), barH - 2, phaseColor);

    // Bottom buttons: toggle / reset / back
    struct PageBtn { int x; int w; const char* label; };
    PageBtn pageBtns[3] = {
        { 8, 98, btn.timerRunning ? "PAUSE" : "START" },
        { 111, 96, "RESET" },
        { 212, 100, "BACK" }
    };
    tft.setTextDatum(MC_DATUM);
    for (uint8_t b = 0; b < 3; b++) {
        uint16_t bg = (b == 0 && btn.timerRunning) ? tft.color565(30, 90, 180) : tft.color565(40, 40, 60);
        if (b == 2) bg = tft.color565(60, 40, 40);
        tft.fillRoundRect(pageBtns[b].x, 186, pageBtns[b].w, 40, 6, bg);
        tft.drawRoundRect(pageBtns[b].x, 186, pageBtns[b].w, 40, 6, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, bg);
        tft.drawString(pageBtns[b].label, pageBtns[b].x + pageBtns[b].w / 2, 206, 2);
    }
}

void handlePomoPageTouch() {
    if (!touchDown) return;
    TS_Point p = touchPoint;
    uint16_t x = constrain(map(p.x, 200, 3700, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
    uint16_t y = constrain(map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);

    static unsigned long pomoTouchDebounce = 0;
    if (millis() < pomoTouchDebounce) return;
    if (millis() < ignoreTouchUntil) return;
    pomoTouchDebounce = millis() + 300;

    if (y < 180 || pomoPageIndex < 0) return;
    if (x < 106) {
        pomoToggleRun(pomoPageIndex);
        drawPomoPage();
    } else if (x < 208) {
        pomoResetSession(pomoPageIndex);
        drawPomoPage();
    } else {
        closePomoPage();
    }
}

// ===== Home Page =====
String homeClockHM() {
    if (homeTime.length() >= 5 && millis() - homeTimeFreshAt < 300000) {
        return homeTime.substring(0, 5);
    }
    unsigned long secs = (millis() / 1000) % 86400;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", (secs / 3600) % 24, (secs % 3600) / 60);
    return String(buf);
}

void fetchProfileNames() {
    if (!wifiConnected) return;
    HTTPClient h;
    h.setTimeout(5000);
    espHttpBegin(h, String(SERVER_URL) + "/api/profiles");
    if (h.GET() == 200) {
        DynamicJsonDocument doc(2048);
        if (!deserializeJson(doc, h.getString())) {
            // Prefer user-configured slots, fall back to first 4 profiles
            if (doc.containsKey("slots")) {
                JsonArray arr = doc["slots"];
                for (uint8_t s = 0; s < 4; s++) {
                    homeNames[s] = (s < arr.size()) ? arr[s].as<String>() : "";
                }
            } else if (doc.containsKey("profiles")) {
                JsonArray arr = doc["profiles"];
                for (uint8_t s = 0; s < 4; s++) {
                    homeNames[s] = (s < arr.size()) ? arr[s].as<String>() : "";
                }
            }
            Serial.println("Profile names synced for home page");
        }
    }
    h.end();
}

void fetchHomeInfo(bool force) {
    if (!wifiConnected) return;
    if (!force && millis() - lastHomeInfoFetch < 60000) return;
    lastHomeInfoFetch = millis();
    HTTPClient h;
    h.setTimeout(8000);
    espHttpBegin(h, String(SERVER_URL) + "/api/home-info");
    if (h.GET() != 200) {
        h.end();
        return;
    }
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, h.getString())) {
        h.end();
        return;
    }
    h.end();
    String t = doc["time"] | "";
    if (t.length() >= 5) {
        homeTime = t;
        homeTimeFreshAt = millis();
    }
    String d = doc["date"] | "";
    if (d.length() > 0) homeDate = d;
    if (!doc["temp_c"].isNull()) {
        float tc = doc["temp_c"];
        homeTemp = String((int)(tc >= 0 ? tc + 0.5f : tc - 0.5f)) + "C";
    } else {
        homeTemp = "--";
    }
    String pn = doc["next_prayer"] | "";
    String pt = doc["next_prayer_time"] | "";
    if (pn.length() > 0) {
        homePrayerName = pn;
        homePrayerTime = (pt.length() >= 5) ? pt.substring(0, 5) : "--:--";
    } else {
        homePrayerName = "--";
        homePrayerTime = "--:--";
    }
    clockAnalog = doc["clock_analog"] | true;
    showTemp = doc["show_temp"] | true;
    showPrayer = doc["show_prayer"] | true;
    showDate = doc["show_date"] | true;
    int br = doc["brightness"] | 100;
    if (br < 10) br = 10;
    if (br > 100) br = 100;
    uint8_t level = (uint8_t)(br * 255 / 100);
    if (level != screenBrightness) {
        screenBrightness = level;
        if (!backlightAlert) applyBacklight(true);
    }
    homePageDirty = true;
}

void openHomePage() {
    homePageActive = true;
    homePageDirty = true;
    homeLastClock = "";
    ignoreTouchUntil = millis() + 400;
    fetchProfileNames();
    fetchHomeInfo(true);
    drawHomePage();
    Serial.println("Home page opened");
}

void closeHomeToGrid() {
    homePageActive = false;
    drawAllButtons();
    drawStatusBar();
}

String homeShortName(uint8_t slot) {
    if (slot < 4 && homeNames[slot].length() > 0) {
        String name = homeNames[slot];
        int sp = name.indexOf(' ');
        if (sp > 0) name = name.substring(0, sp);
        if (name.length() > 7) name = name.substring(0, 7);
        return name;
    }
    char fb[3];
    snprintf(fb, sizeof(fb), "P%d", (slot % 4) + 1);
    return String(fb);
}

void drawHomeProfileButton(int x, int y, int w, int h, const String& label) {
    tft.fillRoundRect(x, y, w, h, 8, tft.color565(40, 40, 60));
    tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, tft.color565(40, 40, 60));
    tft.drawString(label, x + w / 2, y + h / 2, 2);
}

void updateHomeClock() {
    // Partial update: overwrite only the clock text area (same fixed-width
    // "HH:MM" string, text background fills the old glyphs — no flicker)
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString(homeClockHM(), 160, 74, 4);
}

void drawHomePage() {
    tft.fillScreen(deckBackgroundColor);
    drawStatusBar();
    tft.setTextDatum(TC_DATUM);

    // Info values only (no labels): date | prayer + time | temp
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString(homeDate, 62, 20, 2);
    tft.setTextColor(TFT_CYAN, deckBackgroundColor);
    tft.drawString(homePrayerName + " " + homePrayerTime, 160, 20, 2);
    tft.setTextColor(TFT_YELLOW, deckBackgroundColor);
    tft.drawString(homeTemp, 262, 20, 2);

    // Middle row: profile 3 | clock circle | profile 1
    drawHomeProfileButton(8, 40, 64, 76, homeShortName(2));
    drawHomeProfileButton(248, 40, 64, 76, homeShortName(0));
    tft.drawCircle(160, 78, 42, TFT_WHITE);
    tft.drawCircle(160, 78, 39, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.drawString(homeClockHM(), 160, 74, 4);
    tft.setTextColor(TFT_LIGHTGREY, deckBackgroundColor);
    tft.drawString("CLOCK", 160, 98, 1);

    // Bottom row: profile 4 | pomodoro (wide, live countdown) | profile 2
    drawHomeProfileButton(8, 128, 64, 76, homeShortName(3));
    drawHomeProfileButton(248, 128, 64, 76, homeShortName(1));
    updateHomePomoButton();
}

void pomoAdjustMinutes(int deltaMin) {
    // Adjust home pomodoro time from the screen (± minutes, 1..180).
    // Applies to the remaining time and sticks as the current phase duration.
    int nv = homePomo.timerRemaining + deltaMin * 60;
    if (nv < 60) nv = 60;
    if (nv > 180 * 60) nv = 180 * 60;
    homePomo.timerRemaining = nv;
    if (homePomo.pomoPhase == 0) homePomo.pomoWorkSec = nv;
    else if (homePomo.pomoPhase == 1) homePomo.pomoShortSec = nv;
    else homePomo.pomoLongSec = nv;
    homePomo.timerLastTick = millis();
    Serial.printf("Home pomodoro duration: %d sec\n", nv);
}

void updateHomePomoButton() {
    // Partial redraw of the home pomodoro button only, no icon:
    // [-] zone | live MM:SS countdown + phase line | [+] zone
    const uint16_t bg = tft.color565(90, 30, 30);
    tft.fillRoundRect(HOME_POMO_X, HOME_POMO_Y, HOME_POMO_W, HOME_POMO_H, 8, bg);
    tft.drawRoundRect(HOME_POMO_X, HOME_POMO_Y, HOME_POMO_W, HOME_POMO_H, 8, TFT_WHITE);
    int cx = HOME_POMO_X + HOME_POMO_W / 2, cy = HOME_POMO_Y + HOME_POMO_H / 2;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_LIGHTGREY, bg);
    tft.drawString("-", HOME_POMO_X + 16, cy, 4);
    tft.drawString("+", HOME_POMO_X + HOME_POMO_W - 16, cy, 4);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d",
             homePomo.timerRemaining / 60, homePomo.timerRemaining % 60);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, bg);
    tft.drawString(String(buf), HOME_POMO_X + 76, cy - 9, 4);
    const char* phase = homePomo.pomoPhase == 0 ? "FOCUS"
        : (homePomo.pomoPhase == 1 ? "SHORT BREAK" : "LONG BREAK");
    tft.setTextColor(homePomo.pomoAlert ? TFT_YELLOW
        : (homePomo.timerRunning ? TFT_GREEN : TFT_LIGHTGREY), bg);
    tft.drawString(homePomo.pomoAlert ? "DONE!" : phase, HOME_POMO_X + 76, cy + 19, 1);
}

void switchToHomeProfile(uint8_t slot) {
    if (slot >= 4 || homeNames[slot].length() == 0) {
        Serial.println("Home profile slot empty");
        return;
    }
    String url = String(SERVER_URL) + "/api/execute-action";
    DynamicJsonDocument doc(512);
    doc["actionType"] = "switch_profile";
    doc["actionData"] = String("{\"name\":\"") + homeNames[slot] + "\"}";
    String payload;
    serializeJson(doc, payload);
    http.setTimeout(10000);
    espHttpBegin(http, url);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    http.end();
    if (code == 200) {
        Serial.printf("Switched to home profile: %s\n", homeNames[slot].c_str());
        syncProfile();
        closeHomeToGrid();
    } else {
        Serial.printf("Home profile switch failed: %d\n", code);
    }
}

void handleHomeTouch() {
    if (!touchDown) return;
    TS_Point p = touchPoint;
    uint16_t x = constrain(map(p.x, 200, 3700, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
    uint16_t y = constrain(map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);

    static unsigned long homeTouchDebounce = 0;
    if (millis() < homeTouchDebounce) return;
    if (millis() < ignoreTouchUntil) return;
    homeTouchDebounce = millis() + 300;

    if (y < GRID_Y_OFFSET) return; // status bar reserved
    if (y < 40) return;            // info strip not touchable

    // Middle row: profile 3 (left), clock circle (center), profile 1 (right)
    if (y < 122) {
        if (x < 84) {
            switchToHomeProfile(2);
        } else if (x > 236) {
            switchToHomeProfile(0);
        } else {
            int dx = (int)x - 160, dy = (int)y - 78;
            if (dx * dx + dy * dy <= 44 * 44) {
                closeHomeToGrid(); // tap clock = open grid
            }
        }
        return;
    }

    // Bottom row: profile 4 (left), pomodoro (center), profile 2 (right).
    // Pomodoro press is resolved in loop(): release = -/+1 min or
    // start-pause (double-tap = reset), hold = full pomodoro page.
    if (x < 84) {
        switchToHomeProfile(3);
    } else if (x > 236) {
        switchToHomeProfile(1);
    } else if (!homePomoPressActive) {
        homePomoPressActive = true;
        homePomoPressStart = millis();
        homePomoPressX = x;
    }
}

String displayIcon(const String& icon) {
    if (icon == "PLAY" || icon == "▶" || icon == "▶️") return ">";
    if (icon == "PAUSE" || icon == "⏸" || icon == "⏸️") return "||";
    if (icon == "STOP" || icon == "⏹" || icon == "⏹️") return "[]";
    if (icon == "TMR" || icon == "⏲" || icon == "⏲️") return "TMR";
    if (icon == "TIME" || icon == "🕐" || icon == "🕒") return "TIME";
    if (icon == "SW" || icon == "⏱" || icon == "⏱️") return "SW";
    if (icon == "🍅") return "POMO";
    if (icon == "💻") return "PC";
    if (icon == "⚡") return "CMD";
    if (icon == "🐳") return "DOCKER";
    if (icon == "🐙") return "GH";
    if (icon == "🌐") return "WEB";
    if (icon == "📡") return "PING";
    if (icon == "📊") return "STAT";
    if (icon == "🔄") return "RESTART";
    if (icon == "📋") return "LIST";
    if (icon == "🗑️") return "CLEAR";
    if (icon == "🔥") return "CPU";
    if (icon == "💾") return "RAM";
    if (icon == "🎥") return "OBS";
    if (icon == "🎮") return "GAME";
    if (icon == "💬") return "CHAT";
    if (icon == "🖥️") return "DESK";
    if (icon == "📷") return "CAM";
    if (icon == "🚫") return "OFF";
    if (icon == "⏺️") return "REC";
    if (icon == "⏯️") return "TOG";
    if (icon == "📧") return "MAIL";
    if (icon == "📅") return "CAL";
    if (icon == "📄") return "DOC";
    if (icon == "👥") return "TEAM";
    if (icon == "📸") return "SNAP";
    if (icon == "🔢") return "NUM";
    if (icon == "⚙️") return "SET";
    if (icon == "🎵") return "MUSIC";
    if (icon == "📺") return "TV";
    if (icon == "📝") return "NOTE";
    if (icon == "🎬") return "SCENE";
    if (icon == "🎞") return "FILM";
    if (icon == "🎧") return "AUDIO";
    if (icon == "🎤") return "MIC";
    if (icon == "🔊") return "VOL";
    if (icon == "🔇") return "MUTE";
    if (icon == "⏩") return "FWD";
    if (icon == "⏪") return "REW";
    if (icon == "🔀") return "SHUF";
    if (icon == "🔁") return "LOOP";
    if (icon == "📻") return "RADIO";
    if (icon == "⏭") return "NEXT";
    if (icon == "⏮") return "PREV";
    if (icon == "⏏") return "EJECT";
    if (icon == "📂") return "APP";
    if (icon == "📁") return "DIR";
    if (icon == "🔍") return "FIND";
    if (icon == "✂") return "CUT";
    if (icon == "📌") return "PIN";
    if (icon == "🖨") return "PRINT";
    if (icon == "🎯") return "TARGET";
    if (icon == "❤") return "FAV";
    if (icon == "⭐") return "STAR";
    if (icon == "✅") return "DONE";
    if (icon == "❌") return "NO";
    if (icon == "➕") return "ADD";
    if (icon == "➖") return "SUB";
    if (icon == "✏") return "EDIT";
    if (icon == "❓") return "HELP";
    if (icon == "❗") return "ALERT";
    if (icon == "🔔") return "BELL";
    if (icon == "🔕") return "QUIET";
    if (icon == "📞") return "CALL";
    if (icon == "📹") return "MEET";
    if (icon == "🤖") return "BOT";
    if (icon == "🦊") return "FOX";
    if (icon == "✈") return "SEND";
    if (icon == "📩") return "INBOX";
    if (icon == "🏠") return "HOME";
    if (icon == "💡") return "LIGHT";
    if (icon == "🔌") return "PLUG";
    if (icon == "🌡") return "TEMP";
    if (icon == "🚪") return "DOOR";
    if (icon == "🔒") return "LOCK";
    if (icon == "🔓") return "OPEN";
    if (icon == "🌀") return "FAN";
    if (icon == "☀") return "SUN";
    if (icon == "🌙") return "NIGHT";
    if (icon == "⏰") return "ALARM";
    if (icon == "🛋") return "SOFA";
    if (icon == "🔋") return "BATT";
    if (icon == "📶") return "WIFI";
    if (icon == "🌤") return "WTHR";
    if (icon == "🌧") return "RAIN";
    if (icon == "❄") return "SNOW";
    if (icon == "💤") return "SLEEP";
    if (icon == "☕") return "COFFEE";
    if (icon == "🐛") return "BUG";
    if (icon == "🔧") return "TOOL";
    if (icon == "📦") return "BOX";
    if (icon == "🚀") return "DEPLOY";
    if (icon == "⌨") return "KEYS";
    if (icon == "🎹") return "PIANO";
    if (icon == "🧠") return "BRAIN";
    if (icon == "🧪") return "TEST";
    if (icon == "🏷") return "TAG";
    if (icon == "🔗") return "LINK";
    if (icon == "📎") return "ATT";
    if (icon == "🧹") return "CLEAN";
    if (icon == "📈") return "TREND";
    if (icon == "📉") return "DROP";
    if (icon == "🔐") return "SEC";
    if (icon == "📖") return "READ";
    if (icon == "🚗") return "CAR";
    if (icon == "🛒") return "CART";
    if (icon == "💰") return "CASH";
    if (icon == "🕹") return "JOY";
    if (icon == "🎲") return "DICE";
    if (icon == "🔴") return "LIVE";
    if (icon == "🟢") return "ON";
    if (icon == "⏸") return "||";
    if (icon == "⌨️") return "KEYS";
    if (icon == "✈️") return "SEND";
    if (icon.length() > 6) return icon.substring(0, 6);
    return icon;
}

// ===== Vector Icon Shapes (drawn on screen, not text) =====
bool drawIconShape(TFT_eSprite& sprite, const String& icon, int cx, int cy, uint16_t fg, uint16_t bg) {
    // Transport controls
    if (icon == "PLAY" || icon == "▶" || icon == "▶️") {
        sprite.fillTriangle(cx - 6, cy - 8, cx - 6, cy + 8, cx + 8, cy, fg);
        return true;
    }
    if (icon == "PAUSE" || icon == "⏸" || icon == "⏸️") {
        sprite.fillRect(cx - 7, cy - 8, 5, 16, fg);
        sprite.fillRect(cx + 2, cy - 8, 5, 16, fg);
        return true;
    }
    if (icon == "STOP" || icon == "⏹" || icon == "⏹️") {
        sprite.fillRect(cx - 7, cy - 7, 14, 14, fg);
        return true;
    }
    if (icon == "⏺️") {
        sprite.fillCircle(cx, cy, 8, TFT_RED);
        sprite.drawCircle(cx, cy, 8, fg);
        return true;
    }
    if (icon == "⏯️" || icon == "TOG") {
        sprite.fillTriangle(cx - 9, cy - 7, cx - 9, cy + 7, cx - 1, cy, fg);
        sprite.fillRect(cx + 3, cy - 7, 4, 14, fg);
        return true;
    }
    if (icon == "DONE") {
        for (int o = -1; o <= 1; o++) {
            sprite.drawLine(cx - 8, cy + o, cx - 2, cy + 6 + o, fg);
            sprite.drawLine(cx - 2, cy + 6 + o, cx + 8, cy - 6 + o, fg);
        }
        return true;
    }
    // Clock / timer / stopwatch
    if (icon == "TIME" || icon == "🕐" || icon == "🕒" || icon == "TMR" || icon == "⏲" || icon == "⏲️") {
        sprite.drawCircle(cx, cy, 9, fg);
        sprite.drawLine(cx, cy, cx, cy - 6, fg);
        sprite.drawLine(cx, cy, cx + 4, cy + 2, fg);
        sprite.fillCircle(cx, cy, 1, fg);
        return true;
    }
    if (icon == "SW" || icon == "⏱" || icon == "⏱️") {
        sprite.drawCircle(cx, cy + 1, 8, fg);
        sprite.fillRect(cx - 2, cy - 12, 4, 3, fg);
        sprite.drawLine(cx + 5, cy - 6, cx + 8, cy - 9, fg);
        sprite.drawLine(cx, cy + 1, cx, cy - 4, fg);
        sprite.drawLine(cx, cy + 1, cx + 3, cy + 3, fg);
        return true;
    }
    // Pomodoro tomato
    if (icon == "🍅" || icon == "POMO") {
        sprite.fillCircle(cx, cy + 1, 8, TFT_RED);
        sprite.fillTriangle(cx - 1, cy - 7, cx + 8, cy - 9, cx + 4, cy - 3, TFT_GREEN);
        sprite.drawLine(cx + 1, cy - 7, cx + 1, cy - 10, TFT_GREEN);
        return true;
    }
    // Monitor / PC
    if (icon == "💻" || icon == "🖥️" || icon == "DESK" || icon == "PC") {
        sprite.drawRect(cx - 10, cy - 8, 20, 12, fg);
        sprite.fillRect(cx - 1, cy + 4, 2, 4, fg);
        sprite.drawLine(cx - 6, cy + 8, cx + 6, cy + 8, fg);
        return true;
    }
    // Terminal
    if (icon == "⚡" || icon == "CMD" || icon == "WSL") {
        sprite.drawRect(cx - 10, cy - 8, 20, 14, fg);
        sprite.drawLine(cx - 7, cy - 4, cx - 3, cy + 0, fg);
        sprite.drawLine(cx - 3, cy + 0, cx - 7, cy + 4, fg);
        sprite.drawLine(cx - 1, cy + 4, cx + 6, cy + 4, fg);
        return true;
    }
    // Code brackets (VS Code)
    if (icon == "Code") {
        sprite.drawLine(cx - 2, cy - 7, cx - 9, cy, fg);
        sprite.drawLine(cx - 9, cy, cx - 2, cy + 7, fg);
        sprite.drawLine(cx + 2, cy - 7, cx + 9, cy, fg);
        sprite.drawLine(cx + 9, cy, cx + 2, cy + 7, fg);
        sprite.drawLine(cx + 2, cy - 8, cx - 2, cy + 8, fg);
        return true;
    }
    // Docker containers stack
    if (icon == "🐳" || icon == "DOCKER") {
        sprite.drawRect(cx - 9, cy - 9, 18, 5, fg);
        sprite.drawRect(cx - 9, cy - 2, 18, 5, fg);
        sprite.drawRect(cx - 9, cy + 5, 18, 5, fg);
        return true;
    }
    // Git branch (GitHub)
    if (icon == "🐙" || icon == "GH") {
        sprite.drawLine(cx - 5, cy - 6, cx - 5, cy + 6, fg);
        sprite.fillCircle(cx - 5, cy - 6, 3, fg);
        sprite.fillCircle(cx - 5, cy + 6, 3, fg);
        sprite.drawLine(cx - 5, cy + 1, cx + 5, cy - 6, fg);
        sprite.fillCircle(cx + 5, cy - 6, 3, fg);
        return true;
    }
    // Globe (web)
    if (icon == "🌐" || icon == "WEB") {
        sprite.drawCircle(cx, cy, 9, fg);
        sprite.drawEllipse(cx, cy, 4, 9, fg);
        sprite.drawLine(cx - 9, cy, cx + 9, cy, fg);
        return true;
    }
    // Ping signal rings
    if (icon == "📡" || icon == "PING") {
        sprite.fillCircle(cx, cy + 6, 2, fg);
        sprite.drawCircle(cx, cy + 6, 5, fg);
        sprite.drawCircle(cx, cy + 6, 8, fg);
        return true;
    }
    // Bar chart stats
    if (icon == "📊" || icon == "STAT") {
        sprite.drawLine(cx - 9, cy + 8, cx + 9, cy + 8, fg);
        sprite.fillRect(cx - 8, cy + 1, 4, 7, fg);
        sprite.fillRect(cx - 2, cy - 3, 4, 11, fg);
        sprite.fillRect(cx + 4, cy - 7, 4, 15, fg);
        return true;
    }
    // List
    if (icon == "📋" || icon == "LIST") {
        for (int i = 0; i < 3; i++) {
            int yy = cy - 6 + i * 6;
            sprite.fillCircle(cx - 7, yy, 1, fg);
            sprite.drawLine(cx - 4, yy, cx + 8, yy, fg);
        }
        return true;
    }
    // Document
    if (icon == "📄" || icon == "DOC") {
        sprite.drawRect(cx - 6, cy - 9, 12, 18, fg);
        sprite.fillTriangle(cx + 6, cy - 9, cx + 6, cy - 4, cx + 1, cy - 9, fg);
        sprite.drawLine(cx - 3, cy - 2, cx + 3, cy - 2, fg);
        sprite.drawLine(cx - 3, cy + 2, cx + 3, cy + 2, fg);
        sprite.drawLine(cx - 3, cy + 6, cx + 1, cy + 6, fg);
        return true;
    }
    // Note pencil
    if (icon == "📝" || icon == "NOTE") {
        sprite.drawLine(cx - 6, cy + 6, cx + 3, cy - 3, fg);
        sprite.drawLine(cx - 5, cy + 7, cx + 4, cy - 2, fg);
        sprite.fillTriangle(cx + 3, cy - 3, cx + 7, cy - 7, cx + 5, cy - 1, fg);
        return true;
    }
    // Trash
    if (icon == "🗑️" || icon == "CLEAR") {
        sprite.fillRect(cx - 8, cy - 7, 16, 3, fg);
        sprite.fillRect(cx - 2, cy - 10, 4, 3, fg);
        sprite.fillRect(cx - 6, cy - 4, 12, 13, fg);
        sprite.drawLine(cx - 2, cy - 1, cx - 2, cy + 6, bg);
        sprite.drawLine(cx + 2, cy - 1, cx + 2, cy + 6, bg);
        return true;
    }
    // Mail envelope
    if (icon == "📧" || icon == "MAIL") {
        sprite.drawRect(cx - 10, cy - 7, 20, 14, fg);
        sprite.drawLine(cx - 10, cy - 7, cx, cy + 1, fg);
        sprite.drawLine(cx, cy + 1, cx + 10, cy - 7, fg);
        return true;
    }
    // Calendar
    if (icon == "📅" || icon == "CAL") {
        sprite.drawRect(cx - 9, cy - 6, 18, 14, fg);
        sprite.fillRect(cx - 9, cy - 6, 18, 5, fg);
        sprite.fillRect(cx - 5, cy - 10, 3, 5, fg);
        sprite.fillRect(cx + 2, cy - 10, 3, 5, fg);
        return true;
    }
    // Team (two people)
    if (icon == "👥" || icon == "TEAM") {
        sprite.fillCircle(cx - 5, cy - 4, 4, fg);
        sprite.fillCircle(cx + 5, cy - 4, 4, fg);
        sprite.fillRect(cx - 11, cy + 2, 22, 6, fg);
        return true;
    }
    // Camera
    if (icon == "📷" || icon == "📸" || icon == "CAM" || icon == "SNAP") {
        sprite.fillRect(cx - 10, cy - 4, 20, 11, fg);
        sprite.fillRect(cx - 4, cy - 8, 8, 4, fg);
        sprite.drawCircle(cx, cy + 1, 4, bg);
        sprite.fillCircle(cx, cy + 1, 1, bg);
        return true;
    }
    // Video camera (OBS)
    if (icon == "🎥" || icon == "OBS") {
        sprite.fillRect(cx - 10, cy - 6, 14, 12, fg);
        sprite.fillTriangle(cx + 4, cy - 6, cx + 4, cy + 6, cx + 10, cy, fg);
        return true;
    }
    // Gamepad
    if (icon == "🎮" || icon == "GAME") {
        sprite.fillRoundRect(cx - 10, cy - 6, 20, 12, 4, fg);
        sprite.drawLine(cx - 6, cy - 2, cx - 6, cy + 2, bg);
        sprite.drawLine(cx - 8, cy, cx - 4, cy, bg);
        sprite.fillCircle(cx + 4, cy - 1, 1, bg);
        sprite.fillCircle(cx + 7, cy + 2, 1, bg);
        return true;
    }
    // Chat bubble
    if (icon == "💬" || icon == "CHAT") {
        sprite.fillRoundRect(cx - 10, cy - 8, 20, 12, 3, fg);
        sprite.fillTriangle(cx - 4, cy + 4, cx + 2, cy + 4, cx - 4, cy + 9, fg);
        sprite.drawLine(cx - 6, cy - 4, cx + 6, cy - 4, bg);
        sprite.drawLine(cx - 6, cy - 1, cx + 3, cy - 1, bg);
        return true;
    }
    // Music note
    if (icon == "🎵" || icon == "MUSIC") {
        sprite.fillCircle(cx - 5, cy + 5, 3, fg);
        sprite.fillCircle(cx + 5, cy + 6, 3, fg);
        sprite.drawLine(cx - 2, cy + 5, cx - 2, cy - 7, fg);
        sprite.drawLine(cx + 8, cy + 6, cx + 8, cy - 6, fg);
        sprite.drawLine(cx - 2, cy - 7, cx + 8, cy - 6, fg);
        return true;
    }
    // TV
    if (icon == "📺" || icon == "TV") {
        sprite.drawRect(cx - 10, cy - 4, 20, 12, fg);
        sprite.drawLine(cx - 3, cy - 4, cx - 7, cy - 10, fg);
        sprite.drawLine(cx + 3, cy - 4, cx + 7, cy - 10, fg);
        sprite.drawLine(cx - 4, cy + 8, cx + 4, cy + 8, fg);
        return true;
    }
    // Numbers hash
    if (icon == "🔢" || icon == "NUM") {
        sprite.drawLine(cx - 3, cy - 8, cx - 5, cy + 8, fg);
        sprite.drawLine(cx + 4, cy - 8, cx + 2, cy + 8, fg);
        sprite.drawLine(cx - 8, cy - 2, cx + 8, cy - 3, fg);
        sprite.drawLine(cx - 8, cy + 4, cx + 8, cy + 3, fg);
        return true;
    }
    // Gear (settings)
    if (icon == "⚙️" || icon == "SET") {
        for (int a = 0; a < 8; a++) {
            float t = a * 3.14159f / 4.0f;
            int x0 = cx + (int)(cos(t) * 6.0f);
            int y0 = cy + (int)(sin(t) * 6.0f);
            int x1 = cx + (int)(cos(t) * 9.0f);
            int y1 = cy + (int)(sin(t) * 9.0f);
            sprite.drawLine(x0, y0, x1, y1, fg);
        }
        sprite.drawCircle(cx, cy, 5, fg);
        sprite.fillCircle(cx, cy, 2, bg);
        return true;
    }
    // CPU chip
    if (icon == "🔥" || icon == "CPU") {
        sprite.drawRect(cx - 6, cy - 6, 12, 12, fg);
        sprite.fillRect(cx - 3, cy - 3, 6, 6, fg);
        for (int i = -1; i <= 1; i++) {
            sprite.drawLine(cx + i * 4, cy - 9, cx + i * 4, cy - 6, fg);
            sprite.drawLine(cx + i * 4, cy + 6, cx + i * 4, cy + 9, fg);
            sprite.drawLine(cx - 9, cy + i * 4, cx - 6, cy + i * 4, fg);
            sprite.drawLine(cx + 6, cy + i * 4, cx + 9, cy + i * 4, fg);
        }
        return true;
    }
    // RAM stick
    if (icon == "💾" || icon == "RAM") {
        sprite.fillRect(cx - 10, cy - 4, 20, 8, fg);
        sprite.fillRect(cx - 2, cy - 4, 4, 3, bg);
        for (int i = 0; i < 5; i++) {
            sprite.drawLine(cx - 8 + i * 4, cy + 4, cx - 8 + i * 4, cy + 7, fg);
        }
        return true;
    }
    // Restart circular arrow
    if (icon == "🔄" || icon == "RESTART") {
        sprite.drawCircle(cx, cy, 8, fg);
        sprite.fillTriangle(cx + 2, cy - 12, cx + 9, cy - 8, cx + 2, cy - 5, fg);
        return true;
    }
    // Power off
    if (icon == "🚫" || icon == "OFF") {
        sprite.drawCircle(cx, cy, 8, fg);
        sprite.drawLine(cx - 6, cy + 6, cx + 6, cy - 6, fg);
        return true;
    }
    // Key (SSH)
    if (icon == "SSH") {
        sprite.drawCircle(cx - 5, cy - 3, 4, fg);
        sprite.drawLine(cx - 1, cy - 3, cx + 9, cy - 3, fg);
        sprite.drawLine(cx + 5, cy - 3, cx + 5, cy, fg);
        sprite.drawLine(cx + 8, cy - 3, cx + 8, cy + 1, fg);
        return true;
    }
    return false;
}

// ===== Set Button State =====
void setButtonState(uint8_t index, uint8_t state) {
    if (index < BUTTON_COUNT) {
        buttons[index].state = state;
    }
}

// ===== Parse Color from Hex String =====
uint16_t parseColor(String colorHex) {
    if (colorHex.length() < 7 || colorHex[0] != '#') {
        return TFT_DARK_BG;
    }

    colorHex = colorHex.substring(1); // Remove #

    long color = strtol(colorHex.c_str(), NULL, 16);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    return tft.color565(r, g, b);
}

String truncateText(const String& text, uint8_t maxLen) {
    if (text.length() <= maxLen) {
        return text;
    }
    if (maxLen <= 1) {
        return text.substring(0, maxLen);
    }
    return text.substring(0, maxLen - 1) + "~";
}

void scheduleButtonReset(uint8_t index, unsigned long delayMs) {
    buttonReset.index = index;
    buttonReset.resetAt = millis() + delayMs;
    buttonReset.active = true;
}

void processButtonResets() {
    if (!buttonReset.active) {
        return;
    }

    if ((long)(millis() - buttonReset.resetAt) >= 0) {
        if (buttonReset.index < BUTTON_COUNT) {
            setButtonState(buttonReset.index, 0);
            if (!anyOverlayActive()) {
                drawButton(buttonReset.index);
            }
        }
        buttonReset.active = false;
    }
}

void updateRunningIndicators() {
    // Keep running state static. Blinking redraws can look like display flicker on CYD panels.
}

// ===== Draw Status Bar =====
void drawStatusBar() {
    static bool lastWifiConnected = false;
    static bool lastServerAvailable = false;
    static int lastCpuPercent = -1;
    static int lastRamPercent = -1;
    static String lastProfileName = "";

    if (statusBarDrawn &&
        lastWifiConnected == wifiConnected &&
        lastServerAvailable == serverAvailable &&
        lastCpuPercent == cpuPercent &&
        lastRamPercent == ramPercent &&
        lastProfileName == currentProfileName) {
        return;
    }

    lastWifiConnected = wifiConnected;
    lastServerAvailable = serverAvailable;
    lastCpuPercent = cpuPercent;
    lastRamPercent = ramPercent;
    lastProfileName = currentProfileName;
    statusBarDrawn = true;

    tft.fillRect(0, 0, SCREEN_WIDTH, STATUS_BAR_HEIGHT, deckBackgroundColor);
    tft.drawFastHLine(0, STATUS_BAR_HEIGHT - 1, SCREEN_WIDTH, deckBackgroundColor);

    uint16_t wifiColor = wifiConnected ? TFT_GREEN : TFT_RED;
    uint16_t serverColor = serverAvailable ? TFT_GREEN : (wifiConnected ? TFT_YELLOW : deckBackgroundColor);

    tft.fillCircle(7, STATUS_BAR_HEIGHT / 2, 3, wifiColor);
    tft.fillCircle(18, STATUS_BAR_HEIGHT / 2, 3, serverColor);

    tft.setTextColor(TFT_WHITE, deckBackgroundColor);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(truncateText(currentProfileName, 14), 28, STATUS_BAR_HEIGHT / 2 + 1, 1);

    // Visible HOME back button (tap anywhere on the status bar = home page)
    const int homeBtnW = 46;
    const int homeBtnX = SCREEN_WIDTH - 4 - homeBtnW;
    tft.drawRoundRect(homeBtnX, 1, homeBtnW, STATUS_BAR_HEIGHT - 2, 4, TFT_CYAN);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TFT_CYAN, deckBackgroundColor);
    tft.drawString("< HOME", SCREEN_WIDTH - 6, STATUS_BAR_HEIGHT / 2 + 1, 1);

    if (wifiConnected) {
        String stats = String(cpuPercent) + "% " + String(ramPercent) + "%";
        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(TFT_WHITE, deckBackgroundColor);
        tft.drawString(stats, homeBtnX - 6, STATUS_BAR_HEIGHT / 2 + 1, 1);
    }
}

