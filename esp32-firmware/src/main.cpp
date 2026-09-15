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
#include "config.h"

// ===== Configuration =====
#define TOUCH_CS 33
#define TOUCH_IRQ 36
#define TOUCH_SCLK 25
#define TOUCH_MISO 39
#define TOUCH_MOSI 32
#define TFT_BACKLIGHT 21

// WiFi Configuration
// WiFi and server values are stored in the ignored include/config.h file.
const int SYNC_INTERVAL = 5000; // Sync every 5 seconds

// Grid Configuration (4x3 for CYD)
#define GRID_COLS 4
#define GRID_ROWS 3
#define BUTTON_COUNT (GRID_COLS * GRID_ROWS)

// Display Configuration
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define BUTTON_PADDING 4

// Calculate button dimensions
#define BUTTON_WIDTH ((SCREEN_WIDTH - (BUTTON_PADDING * (GRID_COLS + 1))) / GRID_COLS)
#define BUTTON_HEIGHT ((SCREEN_HEIGHT - (BUTTON_PADDING * (GRID_ROWS + 1))) / GRID_ROWS)

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
};

Button buttons[BUTTON_COUNT];

// ===== State Variables =====
unsigned long lastSyncTime = 0;
unsigned long lastWidgetUpdate = 0;
int8_t lastPressedButton = -1;
bool wifiConnected = false;
bool serverAvailable = false;
int cpuPercent = 0;
int ramPercent = 0;
unsigned long lastStatsFetch = 0;

// ===== Function Declarations =====
void setupWiFi();
void setupDisplay();
void setupTouch();
void drawButton(uint8_t index);
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

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    Serial.println("\n=================================");
    Serial.println("StreamDeck ESP32-2432S028 Firmware");
    Serial.println("=================================\n");

    // Initialize display
    setupDisplay();

    // Initialize touch
    setupTouch();

    // Initialize WiFi
    setupWiFi();

    // Initialize buttons with defaults
    for (int i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].label = String(i + 1);
        buttons[i].icon = "";
        buttons[i].color = TFT_DARKGREY;
        buttons[i].actionType = "";
        buttons[i].actionData = "";
        buttons[i].hasWidget = false;
        buttons[i].state = 0;
    }

    // Draw initial UI
    drawAllButtons();
    drawStatusBar();

    Serial.println("Setup complete!");
}

// ===== Main Loop =====
void loop() {
    unsigned long currentTime = millis();

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
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);

    // Show splash screen
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
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

// ===== WiFi Setup =====
void setupWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Connecting to WiFi...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 2);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\nWiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        tft.fillScreen(TFT_BLACK);
        tft.drawString("WiFi Connected!", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 2);
        tft.drawString(WiFi.localIP().toString(), SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 20, 2);
        delay(2000);
    } else {
        wifiConnected = false;
        Serial.println("\nWiFi connection failed!");
        Serial.println("Running in offline mode...");

        tft.fillScreen(TFT_BLACK);
        tft.drawString("WiFi Failed", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20, 2);
        tft.drawString("Offline Mode", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 20, 2);
        delay(2000);
    }
}

// ===== Draw Single Button =====
void drawButton(uint8_t index) {
    if (index >= BUTTON_COUNT) return;

    // Calculate button position
    uint8_t col = index % GRID_COLS;
    uint8_t row = index / GRID_COLS;

    int16_t x = BUTTON_PADDING + col * (BUTTON_WIDTH + BUTTON_PADDING);
    int16_t y = BUTTON_PADDING + row * (BUTTON_HEIGHT + BUTTON_PADDING);

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
        // Blinking effect (handled in main loop)
    } else if (btn.state == 3) { // Success
        bgColor = TFT_GREEN;
    } else if (btn.state == 4) { // Error
        bgColor = TFT_RED;
    }

    // Draw button rectangle
    tft.fillRoundRect(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, 6, bgColor);

    // Draw border
    tft.drawRoundRect(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, 6, TFT_WHITE);

    // Draw icon (if exists)
    if (btn.icon.length() > 0) {
        tft.setTextColor(TFT_WHITE, bgColor);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(displayIcon(btn.icon), x + BUTTON_WIDTH/2, y + BUTTON_HEIGHT/2 - 10, 2);
    }

    // Draw label
    if (btn.label.length() > 0) {
        tft.setTextColor(TFT_WHITE, bgColor);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(btn.label, x + BUTTON_WIDTH/2, y + BUTTON_HEIGHT - 12, 2);
    }
}

// ===== Draw All Buttons =====
void drawAllButtons() {
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        drawButton(i);
    }
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

        if (buttonIndex >= 0 && buttonIndex != lastPressedButton) {
            lastPressedButton = buttonIndex;

            Serial.print("Button pressed: ");
            Serial.println(buttonIndex);

            // Visual feedback
            setButtonState(buttonIndex, 1); // Pressed state
            drawButton(buttonIndex);

            // Execute action
            executeButtonAction(buttonIndex);

            // Reset after delay
            delay(200);
            setButtonState(buttonIndex, 0); // Idle state
            drawButton(buttonIndex);
        }
    } else {
        lastPressedButton = -1;
    }
}

// ===== Get Touched Button =====
int8_t getTouchedButton(uint16_t x, uint16_t y) {
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        uint8_t col = i % GRID_COLS;
        uint8_t row = i / GRID_COLS;

        int16_t btnX = BUTTON_PADDING + col * (BUTTON_WIDTH + BUTTON_PADDING);
        int16_t btnY = BUTTON_PADDING + row * (BUTTON_HEIGHT + BUTTON_PADDING);

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
        setButtonState(index, 4); // Error state
        drawButton(index);
        delay(500);
        setButtonState(index, 0);
        drawButton(index);
        return;
    }

    if (btn.actionType.length() == 0) {
        Serial.println("No action configured for this button");
        return;
    }

    if (btn.actionType == "custom" || btn.actionType == "widget" ||
        btn.actionType == "navigate" || btn.actionType == "macro") {
        Serial.println("This action runs only in the browser simulator");
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

    delay(800);
    setButtonState(index, 0); // Reset to idle
    drawButton(index);
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

        // Get profile data
        http.end();
        http.begin(String(SERVER_URL) + "/api/get-profile");
        httpCode = http.GET();

        if (httpCode == 200) {
            String payload = http.getString();

            DynamicJsonDocument doc(12288);
            DeserializationError error = deserializeJson(doc, payload);

            if (error) {
                Serial.print("Profile JSON parse failed: ");
                Serial.println(error.c_str());
            } else if (doc.containsKey("buttons")) {
                JsonArray buttonsArray = doc["buttons"];
                String profileSignature;

                for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
                    if (i >= buttonsArray.size()) {
                        buttons[i].label = String(i + 1);
                        buttons[i].icon = "";
                        buttons[i].color = TFT_DARKGREY;
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
            if (buttons[i].widgetType == "clock") {
                // Update clock widget (would need RTC or NTP time)
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
    if (icon == "💻") return "PC";
    if (icon == "⚡") return "CMD";
    if (icon == "🐳") return "DOCKER";
    if (icon == "🐙") return "GH";
    if (icon == "🌐") return "WEB";
    if (icon == "📡") return "PING";
    if (icon == "📊") return "STAT";
    if (icon == "🕐") return "TIME";
    if (icon == "🔄") return "RESTART";
    if (icon == "📋") return "LIST";
    if (icon == "🗑️") return "CLEAR";
    if (icon == "⏱️") return "UP";
    if (icon == "🔥") return "CPU";
    if (icon == "💾") return "RAM";
    if (icon == "🎥") return "OBS";
    if (icon == "🎮") return "GAME";
    if (icon == "💬") return "CHAT";
    if (icon == "🖥️") return "DESK";
    if (icon == "📷") return "CAM";
    if (icon == "🚫") return "OFF";
    if (icon == "⏺️") return "REC";
    if (icon == "⏹️") return "STOP";
    if (icon == "⏯️") return "TOG";
    if (icon == "⏲️") return "TMR";
    if (icon == "▶️") return "PLAY";
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
        return TFT_DARKGREY;
    }

    colorHex = colorHex.substring(1); // Remove #

    long color = strtol(colorHex.c_str(), NULL, 16);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    return tft.color565(r, g, b);
}

// ===== Draw Status Bar =====
void drawStatusBar() {
    // Small status indicator in corner
    uint8_t statusSize = 8;
    uint16_t statusColor = wifiConnected ? (serverAvailable ? TFT_GREEN : TFT_YELLOW) : TFT_RED;

    tft.fillCircle(SCREEN_WIDTH - 12, 12, statusSize/2, statusColor);
}
