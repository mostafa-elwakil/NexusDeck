#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <Preferences.h>

// WS2812B-64 (8x8 matrix) on a free CYD header pin.
// GPIO 22 - third pin of the white CN1 connector (3V3 / 27 / 22 / GND),
// free when no DHT22 sensor is attached (DHT support is compiled out).
// Alternatives (same terms): 4, 16, 17 drive the onboard RGB LED pads.
#ifndef WS2812_LED_PIN
#define WS2812_LED_PIN 22
#endif
#ifndef WS2812_LED_COUNT
#define WS2812_LED_COUNT 64
#endif
#ifndef WS2812_LED_WIDTH
#define WS2812_LED_WIDTH 8
#endif
// Most 8x8 panels are serpentine (zigzag rows). Set to 0 for row-major scan.
#ifndef WS2812_SERPENTINE
#define WS2812_SERPENTINE 1
#endif
// USB power: 5V rail on the CYD USB port. Keep brightness low or the
// ESP32 browns out. Hard cap keeps 64 LEDs under ~0.45A.
#define WS2812_BRIGHTNESS_MAX 80
#define WS2812_BRIGHTNESS_DEFAULT 40
#define WS2812_USB_MAX_MA 450

CRGB wsLeds[WS2812_LED_COUNT];

enum LedAnimMode : uint8_t {
    LED_MODE_OFF = 0,
    LED_MODE_SOLID,
    LED_MODE_RAINBOW,
    LED_MODE_BREATHE,
    LED_MODE_CHASE,
    LED_MODE_TEST,    // walk every pixel once - dead-LED check
    LED_MODE_PIXEL,   // one individually addressed pixel
    LED_MODE_TWINKLE, // twinkling sparks (stars)
    LED_MODE_CONFETTI,// scattered random dots with fading trails
    LED_MODE_COMET,   // moving comet with a fading tail
    LED_MODE_MARQUEE, // moving banner of pixels
    LED_MODE_FIRE,    // Fire2012 style flame (bottom = connector side)
    LED_MODE_GRADIENT,// flowing rainbow gradient across the panel
    LED_MODE_COUNT
};

static const char* const ledModeNames[LED_MODE_COUNT] = {
    "off", "solid", "rainbow", "breathe", "chase", "test", "pixel",
    "twinkle", "confetti", "comet", "marquee", "fire", "gradient"
};

static uint8_t ledMode = LED_MODE_OFF;
static CRGB ledColor = CRGB::Teal;
static uint8_t ledBrightness = WS2812_BRIGHTNESS_DEFAULT;
static uint8_t ledPixelIndex = 0;
static uint32_t ledLastTick = 0;
static uint8_t ledHue = 0;
static uint8_t ledPulse = 0;
static bool ledPendingCommand = false;
static char ledPendingJson[384] = "";
static Preferences ledPrefs;
static uint32_t ledServerRev = 0; // master-state revision applied from server

static inline uint16_t ledXY(uint16_t x, uint16_t y) {
#if WS2812_SERPENTINE
    if (y % 2 == 1) {
        return y * WS2812_LED_WIDTH + (WS2812_LED_WIDTH - 1 - x);
    }
#endif
    return y * WS2812_LED_WIDTH + x;
}

static CRGB ledParseHex(const String& hexColor) {
    String h = hexColor;
    h.trim();
    h.replace("#", "");
    if (h.length() == 6) {
        unsigned long v = strtoul(h.c_str(), nullptr, 16);
        return CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
    }
    return ledColor;
}

static void ledPersist() {
    ledPrefs.begin("leds", false);
    ledPrefs.putUChar("mode", ledMode);
    ledPrefs.putUChar("bright", ledBrightness);
    ledPrefs.putUChar("pix", ledPixelIndex);
    ledPrefs.putUInt("color", (ledColor.r << 16) | (ledColor.g << 8) | ledColor.b);
    ledPrefs.end();
}

static void ledSetFontMode(uint8_t mode, const CRGB& color, uint8_t brightness) {
    if (mode >= LED_MODE_COUNT) mode = LED_MODE_OFF;
    if (brightness > WS2812_BRIGHTNESS_MAX) brightness = WS2812_BRIGHTNESS_MAX;
    ledMode = mode;
    ledColor = color;
    ledBrightness = brightness;
    ledPersist();
}

// WLED-style presets: name -> (mode, color, brightness, pixel). The list is
// synced from the server (led_presets.json) and lives in RAM; buttons with
// actionData {"preset":"name"} switch the whole strip instantly.
#define LED_PRESETS_MAX 12
#define LED_PRESET_NAME_LEN 20
static char ledPresetNames[LED_PRESETS_MAX][LED_PRESET_NAME_LEN];
static uint8_t ledPresetMode[LED_PRESETS_MAX];
static CRGB ledPresetColor[LED_PRESETS_MAX];
static uint8_t ledPresetBright[LED_PRESETS_MAX];
static uint8_t ledPresetPixel[LED_PRESETS_MAX];
static uint8_t ledPresetCount = 0;

bool ledApplyPreset(const String& name) {
    for (uint8_t i = 0; i < ledPresetCount; i++) {
        if (name.equals(ledPresetNames[i])) {
            ledMode = ledPresetMode[i];
            if (ledMode >= LED_MODE_COUNT) ledMode = LED_MODE_SOLID;
            ledColor = ledPresetColor[i];
            ledBrightness = ledPresetBright[i];
            ledPixelIndex = ledPresetPixel[i];
            return true;
        }
    }
    return false;
}

bool ledLoadPresetsJson(const String& json) {
    DynamicJsonDocument doc(3072);
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;
    JsonArrayConst arr = doc["presets"].as<JsonArrayConst>();
    if (arr.isNull()) return false;
    uint8_t count = 0;
    for (JsonObjectConst entry : arr) {
        if (count >= LED_PRESETS_MAX) break;
        String name = String(entry["name"] | "");
        name.trim();
        if (name.length() == 0) continue;
        String modeStr = String(entry["mode"] | "solid");
        uint8_t modeIdx = LED_MODE_SOLID;
        for (uint8_t m = 0; m < LED_MODE_COUNT; m++) {
            if (modeStr.equalsIgnoreCase(ledModeNames[m])) {
                modeIdx = m;
                break;
            }
        }
        strncpy(ledPresetNames[count], name.c_str(), LED_PRESET_NAME_LEN - 1);
        ledPresetNames[count][LED_PRESET_NAME_LEN - 1] = '\0';
        ledPresetMode[count] = modeIdx;
        ledPresetColor[count] = ledParseHex(String(entry["color"] | "#008080"));
        int b = entry["brightness"] | 40;
        ledPresetBright[count] = (uint8_t)constrain(b, 0, WS2812_BRIGHTNESS_MAX);
        int px = entry["index"] | 0;
        ledPresetPixel[count] = (uint8_t)constrain(px, 0, WS2812_LED_COUNT - 1);
        count++;
    }
    ledPresetCount = count;
    Serial.printf("LED presets synced: %u\n", ledPresetCount);

    // Master state pushed from the Studio light panel (whole-strip control).
    // Applied only when the revision changes, so device preset buttons and
    // the web panel never fight each other.
    JsonObjectConst current = doc["current"].as<JsonObjectConst>();
    if (!current.isNull()) {
        uint32_t rev = current["rev"] | 0;
        if (rev != ledServerRev) {
            ledServerRev = rev;
            if (current["power"].as<bool>() == false) {
                ledSetFontMode(LED_MODE_OFF, ledColor, ledBrightness);
                Serial.printf("LED master state applied (rev=%u): power=off\n", (unsigned)rev);
            } else {
                String modeStr = String(current["mode"] | "solid");
                uint8_t modeIdx = LED_MODE_SOLID;
                for (uint8_t m = 0; m < LED_MODE_COUNT; m++) {
                    if (modeStr.equalsIgnoreCase(ledModeNames[m])) { modeIdx = m; break; }
                }
                int b = current["brightness"] | ledBrightness;
                b = constrain(b, 0, WS2812_BRIGHTNESS_MAX);
                ledSetFontMode(modeIdx, ledParseHex(String(current["color"] | "#008080")),
                               (uint8_t)b);
                if (modeIdx == LED_MODE_PIXEL) {
                    ledPixelIndex = (uint8_t)constrain(current["index"] | 0, 0,
                                                       WS2812_LED_COUNT - 1);
                    ledPersist();
                }
                Serial.printf("LED master state applied (rev=%u): mode=%s\n",
                              (unsigned)rev, modeStr.c_str());
            }
        }
    }
    return true;
}

static String ledSetJson(const String& json) {
    if (json.length() == 0) return String("empty command");
    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, json);
    if (err) return String("bad json: ") + err.c_str();

    // WLED-style preset first: {"preset":"name"} (server-synced). Fields
    // mode/color/brightness/index that follow override the stored values.
    String presetName = doc["preset"] | "";
    if (presetName.length() > 0 && !ledApplyPreset(presetName)) {
        return String("unknown preset: ") + presetName;
    }

    String modeStr = doc["mode"] | "";
    uint8_t newMode = ledMode;
    bool modeGiven = false;

    if (modeStr.length() > 0) {
        for (uint8_t i = 0; i < LED_MODE_COUNT; i++) {
            if (modeStr.equalsIgnoreCase(ledModeNames[i])) {
                newMode = i;
                modeGiven = true;
                break;
            }
        }
        if (!modeGiven) {
            if (modeStr.toInt() >= 0 && modeStr.toInt() < LED_MODE_COUNT) {
                newMode = (uint8_t)modeStr.toInt();
                modeGiven = true;
            } else {
                return String("unknown mode: ") + modeStr;
            }
        }
    }

    CRGB newColor = ledColor;
    if (doc.containsKey("color")) newColor = ledParseHex(String(doc["color"] | ""));

    uint8_t newBrightness = ledBrightness;
    if (doc.containsKey("brightness")) {
        int b = doc["brightness"] | -1;
        if (b < 0 || b > 255) return String("brightness must be 0-255 (capped ") +
                                     String(WS2812_BRIGHTNESS_MAX) + ")";
        newBrightness = (uint8_t)b;
    }

    // Individually addressed pixel: "index" (linear LED as wired) or
    // "x"/"y" grid coordinates on the 8x8 panel.
    if (doc.containsKey("x") && doc.containsKey("y")) {
        int x = doc["x"] | -1;
        int y = doc["y"] | -1;
        if (x < 0 || x >= WS2812_LED_WIDTH) return "x out of range";
        if (y < 0 || y >= (int)(WS2812_LED_COUNT / WS2812_LED_WIDTH)) return "y out of range";
        ledPixelIndex = (uint8_t)ledXY((uint16_t)x, (uint16_t)y);
        if (!modeGiven) { newMode = LED_MODE_PIXEL; modeGiven = true; }
    } else if (doc.containsKey("index")) {
        int idx = doc["index"] | -1;
        if (idx < 0 || idx >= WS2812_LED_COUNT) {
            return String("index must be 0-") + String(WS2812_LED_COUNT - 1);
        }
        ledPixelIndex = (uint8_t)idx;
        if (!modeGiven) { newMode = LED_MODE_PIXEL; modeGiven = true; }
    }

    ledSetFontMode(newMode, newColor, newBrightness);

    uint32_t rgb = (ledColor.r << 16) | (ledColor.g << 8) | ledColor.b;
    Serial.print("WS2812 mode=");
    Serial.print(ledModeNames[ledMode]);
    Serial.printf(" color=#%06lX bright=%u", (unsigned long)rgb, ledBrightness);
    if (ledMode == LED_MODE_PIXEL) {
        Serial.printf(" pixel=%u", ledPixelIndex);
    }
    if (presetName.length() > 0) {
        Serial.print(" preset=");
        Serial.print(presetName);
    }
    Serial.println();
    return String("ok");
}

static void ledRestore() {
    ledPrefs.begin("leds", true);
    ledMode = ledPrefs.getUChar("mode", LED_MODE_RAINBOW);
    ledBrightness = ledPrefs.getUChar("bright", WS2812_BRIGHTNESS_DEFAULT);
    ledPixelIndex = ledPrefs.getUChar("pix", 0);
    uint32_t rgb = ledPrefs.getUInt("color", 0x008080);
    ledPrefs.end();
    if (ledMode >= LED_MODE_COUNT) ledMode = LED_MODE_RAINBOW;
    if (ledBrightness > WS2812_BRIGHTNESS_MAX) ledBrightness = WS2812_BRIGHTNESS_MAX;
    if (ledPixelIndex >= WS2812_LED_COUNT) ledPixelIndex = 0;
    ledColor = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

void ledSetup() {
    FastLED.addLeds<WS2812B, WS2812_LED_PIN, GRB>(wsLeds, WS2812_LED_COUNT);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, WS2812_USB_MAX_MA);
    FastLED.setBrightness(WS2812_BRIGHTNESS_DEFAULT);
    ledRestore();
    ledLastTick = 0;
    // Boot self-test: walk every pixel once (~1.6s). Any LED that stays dark
    // is dead, if only pixel 0 lights the data chain stops there (wiring).
    for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
        fill_solid(wsLeds, WS2812_LED_COUNT, CRGB::Black);
        wsLeds[i] = CRGB(80, 80, 80);
        FastLED.show();
        delay(25);
    }
    fill_solid(wsLeds, WS2812_LED_COUNT, CRGB::Black);
    FastLED.show();
    Serial.println("WS2812 BOOT SELF-TEST: walked all pixels");
    Serial.print("WS2812 matrix ready: ");
    Serial.print(WS2812_LED_COUNT);
    Serial.print(" LEDs on GPIO ");
    Serial.print(WS2812_LED_PIN);
    Serial.print(" mode=");
    Serial.println(ledModeNames[ledMode]);
}

static void ledRenderFrame() {
    switch (ledMode) {
        case LED_MODE_OFF:
            fill_solid(wsLeds, WS2812_LED_COUNT, CRGB::Black);
            break;
        case LED_MODE_SOLID:
            fill_solid(wsLeds, WS2812_LED_COUNT, ledColor);
            break;
        case LED_MODE_RAINBOW:
            fill_rainbow(wsLeds, WS2812_LED_COUNT, ledHue, 7);
            ledHue += 2;
            break;
        case LED_MODE_BREATHE: {
            uint8_t wave = sin8(ledPulse);
            CRGB dim = ledColor;
            dim.fadeToBlackBy(255 - wave);
            fill_solid(wsLeds, WS2812_LED_COUNT, dim);
            ledPulse += 1;
            break;
        }
        case LED_MODE_CHASE:
            fadeToBlackBy(wsLeds, WS2812_LED_COUNT, 24);
            wsLeds[ledXY(ledPulse % WS2812_LED_WIDTH, ledPulse / WS2812_LED_WIDTH)] = ledColor;
            ledPulse = (ledPulse + 1) % (WS2812_LED_WIDTH * WS2812_LED_WIDTH);
            break;
        case LED_MODE_TEST: {
            // Diagnostic: one white pixel at a time (ledPump ticks at 25ms,
            // so the walk repeats every ~4.8s). Dark LED = dead pixel.
            fill_solid(wsLeds, WS2812_LED_COUNT, CRGB::Black);
            wsLeds[(ledPulse / 3) % WS2812_LED_COUNT] = CRGB::White;
            ledPulse++;
            break;
        }
        case LED_MODE_PIXEL:
            // Individually addressed pixel (index/x/y stored in ledPixelIndex).
            fill_solid(wsLeds, WS2812_LED_COUNT, CRGB::Black);
            wsLeds[ledPixelIndex] = ledColor;
            break;
        case LED_MODE_TWINKLE: {
            // Stars: dots appear at random spots and keep sparkling briefly.
            fadeToBlackBy(wsLeds, WS2812_LED_COUNT, 12);
            for (uint8_t s = 0; s < 3; s++) {
                CRGB spark = ledColor;
                spark %= random8(140, 255);
                wsLeds[random16(WS2812_LED_COUNT)] = spark;
            }
            break;
        }
        case LED_MODE_CONFETTI: {
            // Random rainbow confetti with fading trails.
            fadeToBlackBy(wsLeds, WS2812_LED_COUNT, 10);
            wsLeds[random16(WS2812_LED_COUNT)] =
                hsv2rgb_rainbow(CHSV(random8(), 210, 255));
            break;
        }
        case LED_MODE_COMET: {
            // Comet running along the strip (follows the wiring order).
            fadeToBlackBy(wsLeds, WS2812_LED_COUNT, 22);
            wsLeds[ledPulse % WS2812_LED_COUNT] = ledColor;
            ledPulse++;
            break;
        }
        case LED_MODE_MARQUEE: {
            // Banner of pixels travelling around the panel.
            fill_solid(wsLeds, WS2812_LED_COUNT, CRGB::Black);
            for (uint8_t k = 0; k < 6; k++) {
                wsLeds[(ledPulse + k) % WS2812_LED_COUNT] = ledColor;
            }
            ledPulse++;
            break;
        }
        case LED_MODE_FIRE: {
            // Fire2012: heat rises from the connector-side index 0.
            static uint8_t heat[WS2812_LED_COUNT];
            const uint8_t COOLING = 55;
            const uint8_t SPARKING = 110;
            for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
                uint8_t cooldown = random8(0, ((COOLING * 10) / WS2812_LED_COUNT) + 2);
                heat[i] = qsub8(heat[i], cooldown);
            }
            for (uint8_t k = WS2812_LED_COUNT - 1; k >= 2; k--) {
                heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
            }
            if (random8() < SPARKING) {
                uint8_t y = random8(7);
                heat[y] = qadd8(heat[y], random8(160, 255));
            }
            for (uint8_t j = 0; j < WS2812_LED_COUNT; j++) {
                wsLeds[j] = HeatColor(heat[j]);
            }
            break;
        }
        case LED_MODE_GRADIENT: {
            // Smooth rainbow gradient flowing across the whole panel.
            for (uint16_t i = 0; i < WS2812_LED_COUNT; i++) {
                wsLeds[i] = ColorFromPalette(RainbowColors_p,
                                             ledHue + i * 4, 255, LINEARBLEND);
            }
            ledHue += 1;
            break;
        }
        default:
            fill_solid(wsLeds, WS2812_LED_COUNT, CRGB::Black);
            break;
    }
}

void ledPump() {
    if (ledPendingCommand) {
        ledPendingCommand = false;
        ledSetJson(String(ledPendingJson));
    }
    uint32_t now = millis();
    if (now - ledLastTick < 25) return;
    ledLastTick = now;
    if (ledMode == LED_MODE_SOLID) {
        FastLED.setBrightness(ledBrightness);
        FastLED.show();
        return;
    }
    ledRenderFrame();
    FastLED.setBrightness(ledBrightness);
    FastLED.show();
}

String ledApplyAction(const String& actionData) {
    if (actionData.length() >= sizeof(ledPendingJson)) return String("payload too large");
    actionData.toCharArray(ledPendingJson, sizeof(ledPendingJson));
    ledPendingCommand = true;
    return String("queued");
}
