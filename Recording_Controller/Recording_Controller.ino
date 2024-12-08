/*
 * DIY RGB Smart Keyboard - OBS Control Panel
 * https://github.com/platima/diy-rgb-smartkeyboard
 * 
 * Created by: Platima <wavenospam@plati.ma>
 * Created: 2024-12-08
 * 
 * Description:
 * Arduino Pro Micro-based RGB keyboard for OBS Studio control. Features 12 programmable
 * buttons with integrated WS2812B RGB LEDs for visual feedback. Includes dedicated
 * controls for recording, pause, autofocus, manual focus points, and scene selection.
 * 
 * The code sends F13 through F24 keys, which can be mapped to functions in OBS or
 * on your system. When first started, it will sit in a 'rainbow' mode until you press
 * a key, which will not be actioned.
 * 
 * If using Ubuntu, you'll want to install and run `gconf-editor` and unbind the Gnome
 * plugin mappings that toggle touchpad/audio/etc.
 * 
 * Hardware:
 * - Arduino Pro Micro (ATmega32U4)
 * - 12x WS2812B RGB LEDs (or equivalent)
 * - 12x Momentary switches
 * 
 * Intended Button Mappings:
 * 1. Recording control (red LED)
 * 2. Pause control (yellow LED)
 * 3. TBC
 * 4. TBC
 * 5. Autofocus toggle (rainbow effect)
 * 6. Manual focus point 1
 * 7. Manual focus point 2
 * 8. Manual focus point 3
 * 9. OBS scene 1
 * 10. OBS scene 2
 * 11. OBS scene 3
 * 12. OBS scene 4
 * 
 * Dependencies:
 * - FastLED library
 * - Bounce2 library
 * - Keyboard library (standard Arduino)
 * 
 * License:
 * Creative Commons Attribution-ShareAlike 4.0 International (CC BY-SA 4.0)
 * https://creativecommons.org/licenses/by-sa/4.0/
 * 
 * You are free to:
 * - Share: copy and redistribute the material in any medium or format
 * - Adapt: remix, transform, and build upon the material for any purpose
 * 
 * Under the following terms:
 * - Attribution: You must give appropriate credit, provide a link to the
 *   license, and indicate if changes were made.
 * - ShareAlike: If you remix, transform, or build upon the material, you
 *   must distribute your contributions under the same license as the original.
 */

#include <FastLED.h>
#include <Bounce2.h>
#include <Keyboard.h>

#define LED_PIN     A0
#define NUM_LEDS    12
#define DEBOUNCE_MS 5
#define RAINBOW_SPEED 5
#define MAX_BRIGHTNESS 255
#define STARTUP_SPEED 50  // Speed of startup animation
#define STARTUP_LENGTH 3  // Three LEDs for snake body

const uint8_t buttonPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 14, 15};
const uint8_t ledPositions[] = {0, 1, 2, 3, 7, 6, 5, 4, 8, 9, 10, 11};
const uint8_t NUM_BUTTONS = sizeof(buttonPins) / sizeof(buttonPins[0]);

const uint8_t keyboardKeys[] = {
    KEY_F13, KEY_F14, KEY_F15, KEY_F16, 
    KEY_F17, KEY_F18, KEY_F19, KEY_F20, 
    KEY_F21, KEY_F22, KEY_F23, KEY_F24
};

const CRGB BRIGHT_RED(255, 0, 0);
const CRGB BRIGHT_YELLOW(255, 255, 0);
const CRGB BRIGHT_WHITE(255, 255, 255);
const CRGB BRIGHT_AQUA(0, 255, 255);  // Bright aqua for scene buttons

enum ButtonType {
    TOGGLE_RED,
    TOGGLE_YELLOW,
    NORMAL,
    AUTOFOCUS,
    FOCUS_POINT,
    SCENE_SELECT   // Renamed from GROUP_RAINBOW to match new behavior
};

const ButtonType buttonTypes[] = {
    TOGGLE_RED, TOGGLE_YELLOW, NORMAL, NORMAL,
    AUTOFOCUS, FOCUS_POINT, FOCUS_POINT, FOCUS_POINT,
    SCENE_SELECT, SCENE_SELECT, SCENE_SELECT, SCENE_SELECT
};

CRGB leds[NUM_LEDS];

Bounce * buttons = new Bounce[NUM_BUTTONS];

bool ledStates[12] = {false};
bool rainbowMode[12] = {false};
uint8_t hueValues[12] = {0};
bool startupMode = true;
uint8_t startupPos = 0;

void clearFocusGroup() {
    for (uint8_t i = 5; i <= 7; i++) {
        ledStates[i] = false;
        leds[ledPositions[i]] = CRGB::Black;
    }
}

void clearSceneGroup() {
    for (uint8_t i = 8; i < 12; i++) {
        ledStates[i] = false;
        leds[ledPositions[i]] = CRGB::Black;
    }
}

void updateStartupAnimation() {
    static uint32_t lastUpdate = 0;
    uint32_t currentTime = millis();
    
    if (currentTime - lastUpdate >= STARTUP_SPEED) {
        lastUpdate = currentTime;
        
        // Clear all LEDs
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB::Black;
        }
        
        // Draw the snake with 3 segments
        for (int i = 0; i < STARTUP_LENGTH; i++) {
            int pos = (startupPos - i + NUM_LEDS) % NUM_LEDS;
            if (i == 0) {
                // Head - full brightness
                leds[pos] = CHSV(hueValues[0], 255, 255);
            } else {
                // Body segments - decreasing brightness
                leds[pos] = CHSV(hueValues[0], 255, 255 - (i * 80));
            }
        }
        
        startupPos = (startupPos + 1) % NUM_LEDS;
        hueValues[0] += 5;  // Slowly change the base hue
        
        FastLED.show();
    }
}

void setup() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(MAX_BRIGHTNESS);
    FastLED.clear();
    FastLED.show();
    
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        buttons[i].attach(buttonPins[i], INPUT_PULLUP);
        buttons[i].interval(DEBOUNCE_MS);
    }
    
    Keyboard.begin();
}

// ... [keep all includes, defines, and declarations the same until the loop() function] ...

void loop() {
    if (startupMode) {
        updateStartupAnimation();
        
        for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
            buttons[i].update();
            if (buttons[i].fell()) {
                startupMode = false;
                FastLED.clear();
                FastLED.show();
                return;
            }
        }
        return;
    }
    
    uint32_t currentTime = millis();
    bool updateLeds = false;
    
    // Set autofocus LED state (always on unless manual focus is active)
    if (!ledStates[5] && !ledStates[6] && !ledStates[7]) {  // If no manual focus points are active
        ledStates[4] = true;
        rainbowMode[4] = true;
    }
    
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        buttons[i].update();
        
        if (buttons[i].fell()) {
            Keyboard.press(keyboardKeys[i]);
            Keyboard.release(keyboardKeys[i]);
            
            // ... [keep all previous code until the switch statement] ...

            switch (buttonTypes[i]) {
                case TOGGLE_RED:  // Recording button
                    ledStates[i] = !ledStates[i];
                    leds[ledPositions[i]] = ledStates[i] ? BRIGHT_RED : CRGB::Black;
                    if (!ledStates[i]) {  // If stopping recording
                        // Turn off pause LED as well
                        ledStates[1] = false;
                        leds[ledPositions[1]] = CRGB::Black;
                    }
                    updateLeds = true;
                    break;
                    
                case TOGGLE_YELLOW:  // Pause button
                    if (ledStates[0]) {  // Only work if recording is active
                        ledStates[i] = !ledStates[i];
                        leds[ledPositions[i]] = ledStates[i] ? BRIGHT_YELLOW : CRGB::Black;
                        updateLeds = true;
                    }
                    break;
                    
                case AUTOFOCUS:  // Button 5 - Actively enables autofocus
                    clearFocusGroup();  // Turn off manual focus points
                    ledStates[4] = true;  // Enable autofocus
                    rainbowMode[4] = true;
                    updateLeds = true;
                    break;
                    
                case FOCUS_POINT:  // Buttons 6-8
                    // Turn off autofocus rainbow
                    ledStates[4] = false;
                    rainbowMode[4] = false;
                    leds[ledPositions[4]] = CRGB::Black;
                    
                    clearFocusGroup();
                    ledStates[i] = true;
                    leds[ledPositions[i]] = BRIGHT_WHITE;
                    updateLeds = true;
                    break;
                    
                case SCENE_SELECT:
                    clearSceneGroup();
                    ledStates[i] = true;
                    leds[ledPositions[i]] = BRIGHT_AQUA;
                    updateLeds = true;
                    break;
            }
        }
    }
    
    // Update rainbow cycling for autofocus LED
    static uint32_t lastRainbowUpdate = 0;
    if (currentTime - lastRainbowUpdate >= RAINBOW_SPEED) {
        lastRainbowUpdate = currentTime;
        
        if (ledStates[4] && rainbowMode[4]) {
            leds[ledPositions[4]] = CHSV(hueValues[4]++, 255, 255);
            updateLeds = true;
        }
    }
    
    if (updateLeds) {
        FastLED.show();
    }
}
