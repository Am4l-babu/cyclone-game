/*
 * CYCLONE GAME - ESP8266 LED Game with Web Interface
 * Version: 1.0
 * 
 * A classic arcade-style cyclone game using WS2812B LEDs
 * Features:
 * - 24 main game LEDs that cycle around
 * - 6 score LEDs to track level progress
 * - WiFi Access Point for remote configuration
 * - Web UI with RGB color pickers for LED customization
 * - Custom preset creation and management
 * - EEPROM storage for saving all settings and presets
 * 
 * Access web interface at: http://192.168.4.1
 */

#include <FastLED.h>           // LED control library for WS2812B strips
#include <ESP8266WiFi.h>       // WiFi functionality for ESP8266
#include <ESP8266WebServer.h>  // Web server for configuration interface
#include <EEPROM.h>            // Persistent storage for settings

/* ---------- ESP8266 FIX ---------- */
#define tone(a,b,c)  // Disable tone() to avoid conflicts on ESP8266

/* ---------- VERSION ---------- */
#define VERSION "1.0"

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

/* ---------- CUSTOM PRESET STRUCTURE ---------- */
#define MAX_CUSTOM_PRESETS 5
struct CustomPreset {
  char name[20];      // Preset name
  byte speeds[6];     // Speed for each level
  bool active;        // Is this slot used?
};

/* ---------- SETTINGS ---------- */
// LED speed values for each level (milliseconds between LED movements)
// Lower values = faster/harder. Each level gets progressively faster.
byte ledSpeed[6] = {50, 40, 30, 20, 14, 7};  // Level 1-6 speeds (default: Insane preset)
byte brightness = 55;                        // LED brightness (5-255, default 55)

// LED Color settings (RGB values)
CRGB runningLedColor = CRGB(255, 0, 0);  // Default: Red
CRGB targetLedColor = CRGB(0, 255, 0);   // Default: Green

bool findRandom = false;  // Flag to generate new random target spot
byte spot = 0;            // Target LED position (green LED) that player must hit

// Custom presets storage
CustomPreset customPresets[MAX_CUSTOM_PRESETS];

/* ---------- FUNCTION DECLARATIONS ---------- */
void clearLEDS();                  // Turn off all LEDs (main and score)
void PlayGame(byte b1, byte b2);   // Move the red LED and check boundaries
void winner();                     // Play winning animation and advance level
void loser();                      // Play losing animation and reset game

/* ---------- EEPROM FUNCTIONS ---------- */
// EEPROM Memory Map:
// 0-5: Level speeds
// 10: Brightness
// 11-13: Running LED color (R, G, B)
// 14-16: Target LED color (R, G, B)
// 20-200: Custom presets (5 presets x 36 bytes each)

// Load saved settings from EEPROM (persistent storage)
void loadSettings() {
  EEPROM.begin(512);  // Initialize EEPROM with 512 bytes for more storage
  
  // Load LED speeds for all 6 levels from EEPROM addresses 0-5
  for (int i = 0; i < 6; i++) {
    byte v = EEPROM.read(i);
    if (v >= 10 && v <= 250) ledSpeed[i] = v;  // Validate and load if in range
  }
  
  // Load brightness from EEPROM address 10
  byte b = EEPROM.read(10);
  if (b >= 5 && b <= 255) brightness = b;  // Validate and load if in range
  
  // Load running LED color
  runningLedColor.r = EEPROM.read(11);
  runningLedColor.g = EEPROM.read(12);
  runningLedColor.b = EEPROM.read(13);
  
  // Load target LED color
  targetLedColor.r = EEPROM.read(14);
  targetLedColor.g = EEPROM.read(15);
  targetLedColor.b = EEPROM.read(16);
  
  // Load custom presets
  for (int i = 0; i < MAX_CUSTOM_PRESETS; i++) {
    int addr = 20 + (i * 36);
    customPresets[i].active = EEPROM.read(addr);
    if (customPresets[i].active) {
      for (int j = 0; j < 20; j++) {
        customPresets[i].name[j] = EEPROM.read(addr + 1 + j);
      }
      for (int j = 0; j < 6; j++) {
        customPresets[i].speeds[j] = EEPROM.read(addr + 21 + j);
      }
    }
  }
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

// Save running LED color to EEPROM
void saveRunningColor(byte r, byte g, byte b) {
  EEPROM.write(11, r);
  EEPROM.write(12, g);
  EEPROM.write(13, b);
  EEPROM.commit();
}

// Save target LED color to EEPROM
void saveTargetColor(byte r, byte g, byte b) {
  EEPROM.write(14, r);
  EEPROM.write(15, g);
  EEPROM.write(16, b);
  EEPROM.commit();
}

// Save a custom preset to EEPROM
void saveCustomPreset(int idx, String name, byte speeds[6]) {
  if (idx >= 0 && idx < MAX_CUSTOM_PRESETS) {
    int addr = 20 + (idx * 36);
    EEPROM.write(addr, 1);  // Mark as active
    
    // Save name (max 19 chars + null terminator)
    for (int i = 0; i < 20; i++) {
      EEPROM.write(addr + 1 + i, i < name.length() ? name[i] : 0);
    }
    
    // Save speeds
    for (int i = 0; i < 6; i++) {
      EEPROM.write(addr + 21 + i, speeds[i]);
    }
    
    EEPROM.commit();
    
    // Update in memory
    customPresets[idx].active = true;
    name.toCharArray(customPresets[idx].name, 20);
    for (int i = 0; i < 6; i++) {
      customPresets[idx].speeds[i] = speeds[i];
    }
  }
}

// Delete a custom preset
void deleteCustomPreset(int idx) {
  if (idx >= 0 && idx < MAX_CUSTOM_PRESETS) {
    int addr = 20 + (idx * 36);
    EEPROM.write(addr, 0);  // Mark as inactive
    EEPROM.commit();
    customPresets[idx].active = false;
  }
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
// Provides controls for brightness, colors, presets, and manual speed adjustment
String webPage() {
  String h = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  h += "<style>";
  h += "body{font-family:Arial;margin:20px;background:#1a1a2e;color:#eee}";
  h += "h2{color:#0f3460;background:#16213e;padding:15px;border-radius:8px;text-align:center}";
  h += "h3{color:#e94560;border-bottom:2px solid #e94560;padding-bottom:5px}";
  h += ".section{background:#16213e;padding:20px;margin:15px 0;border-radius:10px;box-shadow:0 4px 6px rgba(0,0,0,0.3)}";
  h += "input[type=range]{width:100%;height:25px;-webkit-appearance:none;background:#0f3460;border-radius:5px;outline:none}";
  h += "input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:25px;height:25px;background:#e94560;border-radius:50%;cursor:pointer}";
  h += "input[type=range]::-moz-range-thumb{width:25px;height:25px;background:#e94560;border-radius:50%;cursor:pointer}";
  h += "button{background:#e94560;color:#fff;border:none;padding:12px 24px;margin:5px;border-radius:6px;cursor:pointer;font-size:16px;font-weight:bold}";
  h += "button:hover{background:#ff6b81}";
  h += "button:active{background:#c23b55}";
  h += ".preset-btn{background:#533483}";
  h += ".preset-btn:hover{background:#6a4494}";
  h += ".custom-preset-btn{background:#05a8aa}";
  h += ".custom-preset-btn:hover{background:#08c9cc}";
  h += ".delete-btn{background:#dc3545;padding:8px 16px;font-size:14px}";
  h += ".delete-btn:hover{background:#c82333}";
  h += "input[type=text],input[type=number]{width:100%;padding:10px;margin:5px 0;border:2px solid #0f3460;border-radius:5px;background:#1a1a2e;color:#eee;font-size:16px}";
  h += ".color-picker{display:inline-block;width:60px;height:60px;border-radius:50%;border:3px solid #eee;cursor:pointer;margin:10px}";
  h += ".speed-input{width:70px;padding:8px;display:inline-block;text-align:center}";
  h += ".value-display{display:inline-block;min-width:60px;color:#e94560;font-weight:bold;font-size:18px}";
  h += ".info{background:#533483;padding:10px;border-radius:5px;margin:10px 0;font-size:14px}";
  h += ".custom-preset-item{background:#0f3460;padding:15px;margin:10px 0;border-radius:8px;display:flex;justify-content:space-between;align-items:center}";
  h += "</style></head><body>";
  
  h += "<h2>🎮 Cyclone Game Control Panel v" + String(VERSION) + "</h2>";
  
  // Brightness Control
  h += "<div class='section'><h3>💡 Brightness</h3>";
  h += "<input type='range' min='5' max='255' value='" + String(brightness) + "' ";
  h += "oninput='this.nextElementSibling.innerHTML=this.value;fetch(\"/bright?v=\"+this.value)'>";
  h += "<span class='value-display'>" + String(brightness) + "</span></div>";
  
  // Color Pickers
  h += "<div class='section'><h3>🎨 LED Colors</h3>";
  h += "<div><b>Running LED Color:</b><br>";
  h += "<input type='color' class='color-picker' id='runColor' value='#";
  h += String(runningLedColor.r < 16 ? "0" : "") + String(runningLedColor.r, HEX);
  h += String(runningLedColor.g < 16 ? "0" : "") + String(runningLedColor.g, HEX);
  h += String(runningLedColor.b < 16 ? "0" : "") + String(runningLedColor.b, HEX);
  h += "' onchange='setRunColor(this.value)'></div>";
  
  h += "<div><b>Target LED Color:</b><br>";
  h += "<input type='color' class='color-picker' id='targColor' value='#";
  h += String(targetLedColor.r < 16 ? "0" : "") + String(targetLedColor.r, HEX);
  h += String(targetLedColor.g < 16 ? "0" : "") + String(targetLedColor.g, HEX);
  h += String(targetLedColor.b < 16 ? "0" : "") + String(targetLedColor.b, HEX);
  h += "' onchange='setTargColor(this.value)'></div></div>";
  
  // Built-in Presets
  h += "<div class='section'><h3>⚡ Built-in Difficulty Presets</h3>";
  h += "<button class='preset-btn' onclick='fetch(\"/preset?p=0\")'>🟢 Easy</button>";
  h += "<button class='preset-btn' onclick='fetch(\"/preset?p=1\")'>🟡 Medium</button>";
  h += "<button class='preset-btn' onclick='fetch(\"/preset?p=2\")'>🟠 Hard</button>";
  h += "<button class='preset-btn' onclick='fetch(\"/preset?p=3\")'>🔴 Insane</button></div>";
  
  // Custom Presets
  h += "<div class='section'><h3>⭐ Custom Presets</h3>";
  
  // Show existing custom presets
  for (int i = 0; i < MAX_CUSTOM_PRESETS; i++) {
    if (customPresets[i].active) {
      h += "<div class='custom-preset-item'>";
      h += "<button class='custom-preset-btn' onclick='fetch(\"/custompreset?i=" + String(i) + "\")'>";
      h += String(customPresets[i].name) + "</button>";
      h += "<button class='delete-btn' onclick='if(confirm(\"Delete preset?\"))fetch(\"/delpreset?i=" + String(i) + "\").then(()=>location.reload())'>Delete</button>";
      h += "</div>";
    }
  }
  
  // Add new preset form
  h += "<div class='info'>Create new custom preset:</div>";
  h += "<input type='text' id='presetName' placeholder='Preset Name' maxlength='19'><br>";
  h += "Level Speeds (ms):<br>";
  for (int i = 0; i < 6; i++) {
    h += "L" + String(i+1) + ": <input type='number' class='speed-input' id='s" + String(i) + "' value='" + String(ledSpeed[i]) + "' min='10' max='250'> ";
  }
  h += "<br><button onclick='addPreset()'>➕ Add Preset</button></div>";
  
  // Manual Speed Control
  h += "<div class='section'><h3>🎚️ Manual Speed Control</h3>";
  for (int i = 0; i < 6; i++) {
    h += "<b>Level " + String(i + 1) + ":</b> ";
    h += "<input type='range' min='10' max='250' value='" + String(ledSpeed[i]) + "' ";
    h += "oninput='this.nextElementSibling.innerHTML=this.value+\" ms\";fetch(\"/speed?l=" + String(i) + "&v=\"+this.value)'>";
    h += "<span class='value-display'>" + String(ledSpeed[i]) + " ms</span><br>";
  }
  h += "</div>";
  
  // JavaScript
  h += "<script>";
  h += "function setRunColor(c){";
  h += "let r=parseInt(c.substr(1,2),16);";
  h += "let g=parseInt(c.substr(3,2),16);";
  h += "let b=parseInt(c.substr(5,2),16);";
  h += "fetch('/runcolor?r='+r+'&g='+g+'&b='+b);}";
  
  h += "function setTargColor(c){";
  h += "let r=parseInt(c.substr(1,2),16);";
  h += "let g=parseInt(c.substr(3,2),16);";
  h += "let b=parseInt(c.substr(5,2),16);";
  h += "fetch('/targcolor?r='+r+'&g='+g+'&b='+b);}";
  
  h += "function addPreset(){";
  h += "let n=document.getElementById('presetName').value;";
  h += "if(!n){alert('Enter preset name');return;}";
  h += "let s='';";
  h += "for(let i=0;i<6;i++){";
  h += "let v=document.getElementById('s'+i).value;";
  h += "if(v<10||v>250){alert('Speed must be 10-250');return;}";
  h += "s+=(i>0?',':'')+v;}";
  h += "fetch('/addpreset?n='+encodeURIComponent(n)+'&s='+s).then(()=>location.reload());}";
  h += "</script>";
  
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

// Handle running LED color change
void handleRunningColor() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    byte r = server.arg("r").toInt();
    byte g = server.arg("g").toInt();
    byte b = server.arg("b").toInt();
    runningLedColor = CRGB(r, g, b);
    saveRunningColor(r, g, b);
  }
  server.send(200, "text/plain", "OK");
}

// Handle target LED color change
void handleTargetColor() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    byte r = server.arg("r").toInt();
    byte g = server.arg("g").toInt();
    byte b = server.arg("b").toInt();
    targetLedColor = CRGB(r, g, b);
    saveTargetColor(r, g, b);
  }
  server.send(200, "text/plain", "OK");
}

// Handle add custom preset
void handleAddPreset() {
  if (server.hasArg("n") && server.hasArg("s")) {
    String name = server.arg("n");
    String speedsStr = server.arg("s");
    
    // Parse speeds
    byte speeds[6];
    int idx = 0;
    int lastComma = -1;
    for (int i = 0; i <= speedsStr.length(); i++) {
      if (i == speedsStr.length() || speedsStr[i] == ',') {
        speeds[idx++] = speedsStr.substring(lastComma + 1, i).toInt();
        lastComma = i;
        if (idx >= 6) break;
      }
    }
    
    // Find empty slot
    for (int i = 0; i < MAX_CUSTOM_PRESETS; i++) {
      if (!customPresets[i].active) {
        saveCustomPreset(i, name, speeds);
        break;
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

// Handle apply custom preset
void handleCustomPreset() {
  if (server.hasArg("i")) {
    int idx = server.arg("i").toInt();
    if (idx >= 0 && idx < MAX_CUSTOM_PRESETS && customPresets[idx].active) {
      for (int i = 0; i < 6; i++) {
        ledSpeed[i] = customPresets[idx].speeds[i];
        saveSpeed(i, customPresets[idx].speeds[i]);
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

// Handle delete custom preset
void handleDeletePreset() {
  if (server.hasArg("i")) {
    int idx = server.arg("i").toInt();
    deleteCustomPreset(idx);
  }
  server.send(200, "text/plain", "OK");
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
  server.on("/", handleRoot);                  // Main page
  server.on("/speed", handleSpeed);            // Speed adjustment endpoint
  server.on("/bright", handleBrightness);      // Brightness endpoint
  server.on("/preset", handlePreset);          // Preset selection endpoint
  server.on("/runcolor", handleRunningColor);  // Running LED color
  server.on("/targcolor", handleTargetColor);  // Target LED color
  server.on("/addpreset", handleAddPreset);    // Add custom preset
  server.on("/custompreset", handleCustomPreset); // Apply custom preset
  server.on("/delpreset", handleDeletePreset); // Delete custom preset
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
      
      // Levels 1-2: Wider target (3 LEDs - use target color with dimmed sides)
      if (gameState <= 2) {
        leds[spot - 1] = targetLedColor;   // Target color (dimmed)
        leds[spot - 1].nscale8(180);       // Dim to 70%
        leds[spot] = targetLedColor;       // Full brightness target LED
        leds[spot + 1] = targetLedColor;   // Target color (dimmed)
        leds[spot + 1].nscale8(140);       // Dim to 55%
        PlayGame(spot - 1, spot + 1);      // Set boundaries for hit detection
      } 
      // Levels 3-6: Narrow target (1 LED - target color only)
      else {
        leds[spot] = targetLedColor;  // Single target LED with custom color
        PlayGame(spot, spot);          // Exact hit required
      }
      
      sleds[gameState - 1] = targetLedColor;  // Light up score LED with target color
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
// Move the running LED around the ring and manage trail
void PlayGame(byte b1, byte b2) {
  leds[Position] = runningLedColor;  // Set current position to running LED color
  
  // Clear the LED behind if it's outside the target zone
  if (Position < b1 + 1 || Position > b2 + 1)
    leds[(Position + NUM_LEDS - 1) % NUM_LEDS] = CRGB::Black;
  
  // Advance to next position (wraps around at end)
  Position = (Position + 1) % NUM_LEDS;
}

// Win animation - flash target color LEDs 3 times and advance to next level
void winner() {
  // Flash target color 3 times
  for (byte i = 0; i < 3; i++) {
    fill_solid(leds, NUM_LEDS, targetLedColor);  // All LEDs in target color
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

// Lose animation - flash running color LEDs 3 times and return to attract mode
void loser() {
  // Flash running color 3 times
  for (byte i = 0; i < 3; i++) {
    fill_solid(leds, NUM_LEDS, runningLedColor);  // All LEDs in running color
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
