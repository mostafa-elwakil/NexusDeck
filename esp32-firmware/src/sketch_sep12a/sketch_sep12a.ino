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

// ===== Configuration =====
#define TOUCH_CS 33
#define TOUCH_IRQ 36

// WiFi Configuration
const char* WIFI_SSID = "STC_4G";
const char* WIFI_PASSWORD = "Mdyou2023";

// Server Configuration
const char* SERVER_URL = "http://192.168.100.31:8765"; // Change to your PC IP
const int SYNC_INTERVAL = 5000; // Sync every 5 seconds

// Grid Configuration (4x3 for CYD)
#define GRID_COLS 4
#define GRID_ROWS 3
#define BUTTON_COUNT (GRID_COLS * GRID_ROWS)

// Display Configuration
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
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
    tft.init();
    tft.setRotation(0); // Portrait mode
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
    touch.begin();
    touch.setRotation(0);
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
        tft.drawString(btn.icon, x + BUTTON_WIDTH/2, y + BUTTON_HEIGHT/2 - 10, 4);
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
    tft.fillScreen(TFT_BLACK);
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        drawButton(i);
    }
}

// ===== Handle Touch Input =====
void handleTouch() {
    if (touch.tirqTouched() && touch.touched()) {
        TS_Point p = touch.getPoint();

        // Map touch coordinates to screen coordinates
        uint16_t x = map(p.x, 200, 3700, 0, SCREEN_WIDTH);
        uint16_t y = map(p.y, 240, 3800, 0, SCREEN_HEIGHT);

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

    // Send action to server
    setButtonState(index, 2); // Running state
    drawButton(index);

    // Build request
    String url = String(SERVER_URL) + "/api/execute-action";

    StaticJsonDocument<512> doc;
    doc["actionType"] = btn.actionType;
    doc["actionData"] = btn.actionData;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonPayload);

    if (httpCode == 200) {
        Serial.println("Action executed successfully");
        setButtonState(index, 3); // Success state
    } else {
        Serial.print("Action failed with code: ");
        Serial.println(httpCode);
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

            StaticJsonDocument<4096> doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error && doc.containsKey("buttons")) {
                JsonArray buttonsArray = doc["buttons"];

                for (uint8_t i = 0; i < BUTTON_COUNT && i < buttonsArray.size(); i++) {
                    JsonObject btnObj = buttonsArray[i];

                    buttons[i].label = btnObj["label"] | "";
                    buttons[i].icon = btnObj["icon"] | "";
                    buttons[i].color = parseColor(btnObj["color"] | "#1a1a2e");

                    if (btnObj.containsKey("action") && !btnObj["action"].isNull()) {
                        buttons[i].actionType = btnObj["action"]["type"] | "";
                        buttons[i].actionData = btnObj["action"]["data"] | "";
                    }

                    if (btnObj.containsKey("widget") && !btnObj["widget"].isNull()) {
                        buttons[i].hasWidget = true;
                        buttons[i].widgetType = btnObj["widget"]["type"] | "";
                    }
                }

                drawAllButtons();
                Serial.println("Profile synced successfully");
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
            }
        }
    }

    if (needsRedraw) {
        drawAllButtons();
    }
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
