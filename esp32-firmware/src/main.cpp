/**
 * ESP32-2432S028 StreamDeck Firmware
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
#include "config.h"
#undef SERVER_URL
#define SERVER_URL serverIP.c_str()

// ===== Configuration & Settings =====
Preferences preferences;
String serverIP = "";
String bgColorHex = "#1a1a2e";
bool bgColorCustom = false;

void loadSettings() {
    preferences.begin("deck", false);
    serverIP = preferences.getString("srv_ip", "");
    bgColorHex = preferences.getString("bg_color", "#1a1a2e");
    bgColorHex.trim();
    if (bgColorHex.length() == 0) {
        bgColorHex = "#1a1a2e";
    }
    bgColorCustom = preferences.getBool("bg_custom", false);
    preferences.end();
}

void saveIP(String ip) {
    preferences.begin("deck", false);
    preferences.putString("srv_ip", ip);
    preferences.end();
    serverIP = ip;
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
String currentProfileName = "StreamDeck";
ButtonResetSchedule buttonReset = {255, 0, false};
bool statusBarDrawn = false;
uint16_t deckBackgroundColor = TFT_DARK_BG;
int syncFailCount = 0;

// ===== Function Declarations =====
void setupWiFi();
void openSetupPortal();
void setupDisplay();
void setupTouch();
void drawButton(uint8_t index);
void drawDeckBackground();
void drawAllButtons();
void handleTouch();
int8_t getTouchedButton(uint16_t x, uint16_t y);
void executeButtonAction(uint8_t index);
void syncProfile();
void updateWidgets();
void setButtonState(uint8_t index, uint8_t state);
uint16_t parseColor(String colorHex);
void drawStatusBar();
void updateSystemStats();
String displayIcon(const String& icon);
void scheduleButtonReset(uint8_t index, unsigned long delayMs);
void processButtonResets();
void updateRunningIndicators();
String truncateText(const String& text, uint8_t maxLen);

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    Serial.println("\n=================================");
    Serial.println("StreamDeck ESP32-2432S028 Firmware");
    Serial.println("=================================\n");

    // Initialize display FIRST
    setupDisplay();

    // Initialize touch
    setupTouch();

    // Load saved server IP and background color
    loadSettings();
    deckBackgroundColor = parseColor(bgColorHex);

    // Connect to WiFi or launch StreamDeck-Setup portal
    setupWiFi();

    // Initialize buttons with defaults
    for (int i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].label = String(i + 1);
        buttons[i].icon = "";
        buttons[i].color = TFT_DARK_BG;
        buttons[i].actionType = "";
        buttons[i].actionData = "";
        buttons[i].hasWidget = false;
        buttons[i].widgetType = "";
        buttons[i].state = 0;
        buttons[i].timerRunning = false;
        buttons[i].timerRemaining = 0;
        buttons[i].timerDuration = 300;
        buttons[i].timerLastTick = 0;
    }

    // Draw initial UI
    drawAllButtons();
    drawStatusBar();

    Serial.println("Setup complete!");
}

// ===== Main Loop =====
void loop() {
    // Hold top area/status bar for 2.5s to trigger StreamDeck-Setup portal anytime
    if (touch.touched()) {
        TS_Point p = touch.getPoint();
        if (p.y < 800) {
            static unsigned long holdStart = 0;
            if (holdStart == 0) holdStart = millis();
            if (millis() - holdStart > 2500) {
                holdStart = 0;
                openSetupPortal();
            }
        }
    }

    unsigned long currentTime = millis();

    processButtonResets();
    updateRunningIndicators();

    // Handle touch input
    handleTouch();

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

    // Refresh status bar periodically (CPU/RAM/profile name)
    if (currentTime - lastStatusBarDraw > 5000) {
        drawStatusBar();
        lastStatusBarDraw = currentTime;
    }

    delay(10);
}

// ===== Display Setup =====
void setupDisplay() {
    Serial.println("Initializing display...");
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);
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
    tft.drawString("StreamDeck CYD", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 4);
    tft.drawString("Initializing...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 20, 2);
    delay(2000);

    Serial.println("Display initialized!");
}

// ===== Touch Setup =====
void setupTouch() {
    Serial.println("Initializing touch screen...");
    // CYD touch controller uses its own SPI wiring, separate from the TFT HSPI bus.
    SPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI);
    touch.begin();
    SPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI);
    touch.setRotation(1);
    Serial.println("Touch screen initialized!");
}

// ===== Setup Portal (StreamDeck-Setup) =====
void openSetupPortal() {
    tft.fillScreen(deckBackgroundColor);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_DARK_BG);
    tft.drawString("WiFi Setup Needed", SCREEN_WIDTH / 2, 35, 4);

    tft.setTextColor(TFT_WHITE, TFT_DARK_BG);
    tft.drawString("1. Connect Phone/PC to WiFi:", SCREEN_WIDTH / 2, 75, 2);

    tft.setTextColor(TFT_CYAN, TFT_DARK_BG);
    tft.drawString("StreamDeck-Setup", SCREEN_WIDTH / 2, 105, 4);

    tft.setTextColor(TFT_WHITE, TFT_DARK_BG);
    tft.drawString("Password: password123", SCREEN_WIDTH / 2, 135, 2);
    tft.drawString("2. Set WiFi, Server IP & BG color", SCREEN_WIDTH / 2, 165, 2);

    tft.setTextColor(TFT_GREEN, TFT_DARK_BG);
    tft.drawString("Open: 192.168.4.1", SCREEN_WIDTH / 2, 195, 2);

    Serial.println("Starting config portal: StreamDeck-Setup");
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);

    WiFiManagerParameter custom_server_ip("server_ip", "Server URL (e.g. http://192.168.1.5:8765)", serverIP.c_str(), 60);
    wm.addParameter(&custom_server_ip);
    WiFiManagerParameter custom_bg_color("bg_color", "Background color (pick from the list)", bgColorHex.c_str(), 8, "type=\"color\"");
    wm.addParameter(&custom_bg_color);
    WiFiManagerParameter custom_bg_follow("bg_follow", "Follow profile background instead", "1", 2, "type=\"checkbox\"");
    wm.addParameter(&custom_bg_follow);

    if (!wm.startConfigPortal("StreamDeck-Setup", "password123")) {
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

        if (!wm.autoConnect("StreamDeck-Setup", "password123")) {
            Serial.println("WiFi connection failed, restarting...");
            delay(1000);
            ESP.restart();
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

    // Draw icon (if exists)
    if (btn.icon.length() > 0) {
        sprite.setTextColor(TFT_WHITE, bgColor);
        sprite.setTextDatum(MC_DATUM);
        sprite.drawString(truncateText(displayIcon(btn.icon), 8), BUTTON_WIDTH / 2, BUTTON_HEIGHT / 3, 2);
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
void handleTouch() {
    if (touch.tirqTouched() && touch.touched()) {
        TS_Point p = touch.getPoint();

        // Map calibrated raw touch coordinates to the rotated landscape screen.
        uint16_t x = constrain(map(p.x, 200, 3700, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
        uint16_t y = constrain(map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);

        static unsigned long lastTouchLog = 0;
        if (millis() - lastTouchLog > 500) {
            Serial.printf("Touch raw=%d,%d screen=%u,%u\n", p.x, p.y, x, y);
            lastTouchLog = millis();
        }

        int8_t buttonIndex = getTouchedButton(x, y);

        if (buttonIndex >= 0 && buttonIndex != lastPressedButton &&
            millis() >= touchDebounceUntil) {
            lastPressedButton = buttonIndex;
            touchDebounceUntil = millis() + 280;

            Serial.print("Button pressed: ");
            Serial.println(buttonIndex);

            setButtonState(buttonIndex, 1);
            drawButton(buttonIndex);
            executeButtonAction(buttonIndex);
        }
    } else {
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

        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(jsonPayload);
        http.end();

        if (httpCode == 200) {
            Serial.println("Profile switched successfully! Syncing new profile...");
            setButtonState(index, 3);
            drawButton(index);
            syncProfile();
        } else {
            setButtonState(index, 4);
            drawButton(index);
        }
        scheduleButtonReset(index, 600);
        return;
    }

    if (btn.hasWidget || btn.actionType == "widget" || btn.actionType == "custom" ||
        btn.actionType == "navigate" || btn.actionType == "macro") {
        Serial.println("Widget/Custom action triggered on ESP32");
        setButtonState(index, 3); // Success state
        drawButton(index);
        scheduleButtonReset(index, 500);
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
    http.begin(url);
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
        Serial.print("Action response: ");
        Serial.println(responseBody);
        setButtonState(index, 4); // Error state
    }

    http.end();
    drawButton(index);
    scheduleButtonReset(index, 800);
}

// ===== Sync Profile from Server =====
void syncProfile() {
    if (!wifiConnected) return;

    String url = String(SERVER_URL) + "/api/health";

    http.setTimeout(8000);
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == 200) {
        serverAvailable = true;
        syncFailCount = 0;

        // Get profile data
        http.end();
        http.begin(String(SERVER_URL) + "/api/get-profile");
        http.addHeader("X-StreamDeck-Client", "esp32");
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
                        continue;
                    }

                    JsonObject btnObj = buttonsArray[i];

                    buttons[i].label = btnObj["label"] | "";
                    buttons[i].icon = btnObj["icon"] | "";
                    buttons[i].color = parseColor(btnObj["color"] | "#1a1a2e");
                    buttons[i].hasWidget = false;
                    buttons[i].widgetType = "";

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
                        }
                    }

                    profileSignature += buttons[i].label + "|" + buttons[i].icon + "|" +
                        buttons[i].actionType + "|" + buttons[i].actionData + "|" +
                        String(buttons[i].color) + "|" + buttons[i].widgetType + ";";
                }

                static String lastProfileSignature;
                if (profileSignature != lastProfileSignature) {
                    lastProfileSignature = profileSignature;
                    drawAllButtons();
                    Serial.println("Profile synced successfully");
                }
            }
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
    drawStatusBar();
}

// ===== Update Live Widgets =====
void updateWidgets() {
    bool needsRedraw = false;

    if (wifiConnected && millis() - lastStatsFetch >= 5000) {
        updateSystemStats();
        lastStatsFetch = millis();
    }

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

    if (needsRedraw) {
        for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
            if (buttons[i].hasWidget) drawButton(i);
        }
        drawStatusBar();
    }
}

void updateSystemStats() {
    HTTPClient statsHttp;
    statsHttp.begin(String(SERVER_URL) + "/api/system-stats");
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

String displayIcon(const String& icon) {
    if (icon == "PLAY" || icon == "▶" || icon == "▶️") return ">";
    if (icon == "PAUSE" || icon == "⏸" || icon == "⏸️") return "||";
    if (icon == "STOP" || icon == "⏹" || icon == "⏹️") return "[]";
    if (icon == "TMR" || icon == "⏲" || icon == "⏲️") return "TMR";
    if (icon == "TIME" || icon == "🕐" || icon == "🕒") return "TIME";
    if (icon == "SW" || icon == "⏱" || icon == "⏱️") return "SW";
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
    if (icon.length() > 6) return icon.substring(0, 6);
    return icon;
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
            drawButton(buttonReset.index);
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

    if (wifiConnected) {
        String stats = String(cpuPercent) + "% " + String(ramPercent) + "%";
        tft.setTextDatum(MR_DATUM);
        tft.drawString(stats, SCREEN_WIDTH - 4, STATUS_BAR_HEIGHT / 2 + 1, 1);
    }
}

