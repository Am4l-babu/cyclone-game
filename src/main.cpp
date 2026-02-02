/*
 * CYCLONE GAME - ESP8266 LED Game with Web Interface
 * 
 * A classic arcade-style cyclone game using WS2812B LEDs
 * Features:
 * - 24 main game LEDs that cycle around
 * - 6 score LEDs to track level progress
 * - WiFi Access Point for remote configuration
 * - Web UI for adjusting difficulty and brightness
 * - EEPROM storage for saving settings
 * 
 * Access web interface at: http://192.168.4.1
 */

#include <FastLED.h>           // LED control library for WS2812B strips
#include <ESP8266WiFi.h>       // WiFi functionality for ESP8266
#include <ESP8266WebServer.h>  // Web server for configuration interface
#include <EEPROM.h>            // Persistent storage for settings

/* ---------- ESP8266 FIX ---------- */
#define tone(a,b,c)  // Disable tone() to avoid conflicts on ESP8266

/* ---------- LED & PIN CONFIG ---------- */
#define NUM_LEDS     24    // Number of LEDs in the main game ring
#define SCORE_LEDS   6     // Number of LEDs for score/level display (one per level)
#define DATA_PIN     2     // GPIO2 (D4) - Data pin for main LED strip
#define SCORE_PIN    14    // GPIO14 (D5) - Data pin for score LED strip
#define BUTTON_PIN   12    // GPIO12 (D6) - Button input pin for player action

CRGB leds[NUM_LEDS];       // Array to control main game LEDs
CRGB sleds[SCORE_LEDS];    // Array to control score indicator LEDs

/* ---------- WIFI AP ---------- */
// WiFi Access Point Configuration
// Connect to network "CycloneGame" with password "12345678"
// Then access web interface at: http://192.168.4.1
const char* ap_ssid = "CycloneGame";  // WiFi network name
const char* ap_pass = "12345678";     // WiFi password (min 8 characters)
ESP8266WebServer server(80);          // Web server on port 80

/* ---------- GAME STATE ---------- */
bool reachedEnd = false;        // Flag for end of cycle detection
byte gameState = 0;             // Current game state: 0=attract, 1-6=levels, 98=win, 99=lose
int period = 1000;              // Current LED movement period in milliseconds
unsigned long time_now = 0;     // Timestamp for LED movement timing
byte Position = 0;              // Current position of the moving red LED (0-23)
byte level = 0;                 // Last completed level

/* ---------- SETTINGS ---------- */
// LED speed values for each level (milliseconds between LED movements)
// Lower values = faster/harder. Each level gets progressively faster.
byte ledSpeed[6] = {50, 40, 30, 20, 14, 7};  // Level 1-6 speeds (default: Insane preset)
byte brightness = 55;                        // LED brightness (5-255, default 55)

bool findRandom = false;  // Flag to generate new random target spot
byte spot = 0;            // Target LED position (green LED) that player must hit

/* ---------- FUNCTION DECLARATIONS ---------- */
void clearLEDS();                  // Turn off all LEDs (main and score)
void PlayGame(byte b1, byte b2);   // Move the red LED and check boundaries
void winner();                     // Play winning animation and advance level
void loser();                      // Play losing animation and reset game

/* ---------- EEPROM FUNCTIONS ---------- */
// Load saved settings from EEPROM (persistent storage)
void loadSettings() {
  EEPROM.begin(64);  // Initialize EEPROM with 64 bytes
  
  // Load LED speeds for all 6 levels from EEPROM addresses 0-5
  for (int i = 0; i < 6; i++) {
    byte v = EEPROM.read(i);
    if (v >= 10 && v <= 250) ledSpeed[i] = v;  // Validate and load if in range
  }
  
  // Load brightness from EEPROM address 10
  byte b = EEPROM.read(10);
  if (b >= 5 && b <= 255) brightness = b;  // Validate and load if in range
}

// Save a specific level's speed to EEPROM
void saveSpeed(byte i, byte v) {
  EEPROM.write(i, v);  // Write speed to EEPROM address 0-5 (level index)
  EEPROM.commit();     // Commit changes to flash memory
}

// Save brightness setting to EEPROM
void saveBrightness(byte b) {
  EEPROM.write(10, b);  // Write brightness to EEPROM address 10
  EEPROM.commit();      // Commit changes to flash memory
}

/* ---------- DIFFICULTY PRESETS ---------- */
// Apply a difficulty preset (Easy/Medium/Hard/Insane)
// Each preset defines the speed (ms) for all 6 levels
void applyPreset(byte p) {
  const byte presets[4][6] = {
    {160,140,120,100,80,60},   // Easy - slower speeds for beginners
    {120,100,80,60,40,30},     // Medium - moderate challenge
    {80,65,50,35,25,18},       // Hard - fast paced gameplay
    {50,40,30,20,14,7}         // Insane - expert level speeds
  };
  
  if (p < 4) {  // Validate preset index
    // Apply preset speeds to all 6 levels and save to EEPROM
    for (byte i = 0; i < 6; i++) {
      ledSpeed[i] = presets[p][i];
      saveSpeed(i, presets[p][i]);
    }
  }
}

/* ---------- WEB UI ---------- */
// Generate HTML page for web interface
// Provides controls for brightness, presets, and manual speed adjustment
String webPage() {
  String h = "<html><body><h2>Cyclone Game Control</h2>";

  // Brightness slider (5-255)
  h += "<h3>Brightness</h3>";
  h += "<input type='range' min='5' max='255' value='" + String(brightness) +
       "' oninput='fetch(\"/bright?v=\"+this.value)'> " +
       String(brightness) + "<hr>";

  // Difficulty preset buttons
  h += "<h3>Difficulty Presets</h3>";
  h += "<button onclick='fetch(\"/preset?p=0\")'>Easy</button> ";
  h += "<button onclick='fetch(\"/preset?p=1\")'>Medium</button> ";
  h += "<button onclick='fetch(\"/preset?p=2\")'>Hard</button> ";
  h += "<button onclick='fetch(\"/preset?p=3\")'>Insane</button><hr>";

  // Individual speed sliders for each level (10-250 ms)
  h += "<h3>Manual Speed Control</h3>";
  for (int i = 0; i < 6; i++) {
    h += "Level " + String(i + 1) + ": ";
    h += "<input type='range' min='10' max='250' value='" + String(ledSpeed[i]) +
         "' oninput='fetch(\"/speed?l=" + String(i) + "&v=\"+this.value)'> ";
    h += String(ledSpeed[i]) + " ms<br><br>";
  }

  h += "</body></html>";
  return h;
}

/* ---------- WEB HANDLERS ---------- */
// Handle root page request - serve the main web interface
void handleRoot() { server.send(200, "text/html", webPage()); }

// Handle speed adjustment request from web UI
void handleSpeed() {
  if (server.hasArg("l") && server.hasArg("v")) {  // Check if level and value args exist
    int l = server.arg("l").toInt();  // Get level index (0-5)
    int v = server.arg("v").toInt();  // Get speed value in milliseconds
    if (l >= 0 && l < 6 && v >= 10 && v <= 250) {  // Validate inputs
      ledSpeed[l] = v;      // Update speed for this level
      saveSpeed(l, v);      // Save to EEPROM
    }
  }
  server.send(200, "text/plain", "OK");  // Send response
}

// Handle brightness adjustment request from web UI
void handleBrightness() {
  if (server.hasArg("v")) {  // Check if value arg exists
    int b = server.arg("v").toInt();  // Get brightness value
    if (b >= 5 && b <= 255) {  // Validate range
      brightness = b;                    // Update brightness variable
      saveBrightness(b);                 // Save to EEPROM
      FastLED.setBrightness(brightness); // Apply immediately to LEDs
    }
  }
  server.send(200, "text/plain", "OK");  // Send response
}

// Handle preset selection request from web UI
void handlePreset() {
  if (server.hasArg("p")) {  // Check if preset arg exists
    applyPreset(server.arg("p").toInt());  // Apply preset (0-3)
  }
  server.send(200, "text/plain", "OK");  // Send response
}

/* ---------- SETUP ---------- */
void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);

  // Initialize LED strips (WS2812B)
  FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);    // Main game LEDs
  FastLED.addLeds<WS2812B, SCORE_PIN, RGB>(sleds, SCORE_LEDS); // Score LEDs

  // Configure button pin with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Load saved settings from EEPROM
  loadSettings();
  FastLED.setBrightness(brightness);

  // Setup WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_pass);  // Default IP will be 192.168.4.1

  // Setup web server routes
  server.on("/", handleRoot);            // Main page
  server.on("/speed", handleSpeed);      // Speed adjustment endpoint
  server.on("/bright", handleBrightness); // Brightness endpoint
  server.on("/preset", handlePreset);    // Preset selection endpoint
  server.begin();  // Start the web server
}

/* ---------- LOOP ---------- */
void loop() {
  // Handle incoming web requests
  server.handleClient();
  FastLED.setBrightness(brightness);

  // ATTRACT MODE (gameState = 0)
  // Show rainbow animation while waiting for player to start
  if (gameState == 0) {
    fill_rainbow(leds, NUM_LEDS, 0, 20);    // Rainbow on main LEDs
    fill_rainbow(sleds, SCORE_LEDS, 0, 40); // Rainbow on score LEDs
    
    // Start game when button is pressed
    if (digitalRead(BUTTON_PIN) == LOW) {
      Position = 0;        // Reset LED position
      findRandom = true;   // Flag to generate new target
      delay(500);          // Debounce delay
      clearLEDS();         // Clear all LEDs
      gameState = 1;       // Start at level 1
    }
    FastLED.show();
  }

  // GAMEPLAY MODE (gameState = 1-6, representing levels 1-6)
  if (gameState >= 1 && gameState <= 6) {
    // Set speed based on current level
    period = ledSpeed[gameState - 1];
    
    // Move LED at the appropriate speed
    if (millis() > time_now + period) {
      time_now = millis();
      
      // Generate random target spot at start of each level
      if (findRandom) {
        spot = random(NUM_LEDS - 6) + 3;  // Random position avoiding edges
        findRandom = false;
      }
      
      // Levels 1-2: Wider target (3 LEDs - orange, green, orange)
      if (gameState <= 2) {
        leds[spot - 1] = CRGB(255,140,0);  // Orange LED before target
        leds[spot] = CRGB::Green;           // Green target LED
        leds[spot + 1] = CRGB(255,110,0);  // Orange LED after target
        PlayGame(spot - 1, spot + 1);       // Set boundaries for hit detection
      } 
      // Levels 3-6: Narrow target (1 LED - green only)
      else {
        leds[spot] = CRGB::Green;  // Single green target LED
        PlayGame(spot, spot);       // Exact hit required
      }
      
      sleds[gameState - 1] = CRGB::Green;  // Light up score LED for current level
      FastLED.show();
    }

    // Check if player pressed button to stop the LED
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(300);  // Debounce delay
      
      // Check if player hit the target zone
      if ((gameState <= 2 && Position > spot - 1 && Position < spot + 3) ||  // Wide target
          (gameState > 2 && Position == spot + 1)) {                          // Narrow target
        level = gameState;  // Save current level
        gameState = 98;     // Go to winner state
      } else {
        gameState = 99;  // Go to loser state
      }
    }
  }

  // Trigger win/lose animations
  if (gameState == 98) winner();  // Player successfully hit target
  if (gameState == 99) loser();   // Player missed target
}

/* ---------- GAME ENGINE ---------- */
// Move the red LED around the ring and manage trail
void PlayGame(byte b1, byte b2) {
  leds[Position] = CRGB::Red;  // Set current position to red
  
  // Clear the LED behind if it's outside the target zone
  if (Position < b1 + 1 || Position > b2 + 1)
    leds[(Position + NUM_LEDS - 1) % NUM_LEDS] = CRGB::Black;
  
  // Advance to next position (wraps around at end)
  Position = (Position + 1) % NUM_LEDS;
}

// Win animation - flash green LEDs 3 times and advance to next level
void winner() {
  // Flash green 3 times
  for (byte i = 0; i < 3; i++) {
    fill_solid(leds, NUM_LEDS, CRGB::Green);  // All LEDs green
    FastLED.show();
    delay(500);
    clearLEDS();  // Turn off
    FastLED.show();
    delay(500);
  }
  
  // Setup for next level
  findRandom = true;      // Generate new target position
  Position = 0;           // Reset LED position
  gameState = level + 1;  // Advance to next level
  
  // If all 6 levels completed, return to attract mode
  if (gameState > 6) gameState = 0;
}

// Lose animation - flash red LEDs 3 times and return to attract mode
void loser() {
  // Flash red 3 times
  for (byte i = 0; i < 3; i++) {
    fill_solid(leds, NUM_LEDS, CRGB::Red);  // All LEDs red
    FastLED.show();
    delay(500);
    clearLEDS();  // Turn off
    FastLED.show();
    delay(500);
  }
  
  gameState = 0;  // Return to attract mode
}

// Turn off all LEDs (both main game strip and score strip)
void clearLEDS() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);     // Clear main LEDs
  fill_solid(sleds, SCORE_LEDS, CRGB::Black);  // Clear score LEDs
}

void winAll() {}
