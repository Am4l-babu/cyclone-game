# Cyclone Game - ESP8266 LED Arcade Game

A classic arcade-style cyclone game built with ESP8266 and WS2812B LEDs, featuring a web-based configuration interface.

## Features

- 🎮 Classic cyclone gameplay with 6 progressive difficulty levels
- 💡 24 WS2812B LEDs for the main game ring
- 📊 6 LED score indicators to track level progress
- 🌐 WiFi Access Point with web interface for remote configuration
- ⚙️ Adjustable difficulty presets (Easy, Medium, Hard, Insane)
- 💾 EEPROM storage to save settings
- 🎨 Rainbow attract mode animation

## Hardware Requirements

- ESP8266 board (ESP-12E or similar)
- 24x WS2812B LEDs (main game strip)
- 6x WS2812B LEDs (score indicator strip)
- Push button
- Power supply (5V recommended for LEDs)

## Pin Configuration

| Component | Pin | GPIO |
|-----------|-----|------|
| Main LED Strip | D4 | GPIO2 |
| Score LED Strip | D5 | GPIO14 |
| Button | D6 | GPIO12 |

## Installation

### PlatformIO
```bash
# Clone the repository
git clone https://github.com/Am4l-babu/cyclone-game.git
cd cyclone-game

# Build and upload
pio run --target upload
```

### Dependencies
- FastLED @ 3.10.3
- ESP8266WiFi
- ESP8266WebServer
- EEPROM

## How to Play

1. **Power on** - LEDs display rainbow animation (attract mode)
2. **Press button** to start the game
3. A **red LED** cycles around the ring
4. **Green LED(s)** indicate the target zone
5. **Press button** when red LED is in the green zone
6. Complete all 6 levels to win!

### Difficulty Levels
- **Levels 1-2**: Wider target zone (3 LEDs)
- **Levels 3-6**: Narrow target zone (1 LED)
- Each level increases the LED speed

## Web Interface

Connect to the WiFi network and access the web interface to customize game settings.

### WiFi Credentials
- **SSID**: `CycloneGame`
- **Password**: `12345678`
- **URL**: http://192.168.4.1

### Web Interface Features
- **Brightness Control**: Adjust LED brightness (5-255)
- **Difficulty Presets**: One-click difficulty selection
  - Easy (160-60 ms)
  - Medium (120-30 ms)
  - Hard (80-18 ms)
  - Insane (50-7 ms)
- **Manual Speed Control**: Fine-tune each level individually (10-250 ms)

## Game States

| State | Description |
|-------|-------------|
| 0 | Attract mode (rainbow animation) |
| 1-6 | Playing levels 1 through 6 |
| 98 | Winner animation (green flash) |
| 99 | Loser animation (red flash) |

## Configuration

All settings are automatically saved to EEPROM and persist across power cycles:
- LED speeds for all 6 levels
- Brightness setting

## Code Structure

```
cyclone_game/
├── src/
│   └── main.cpp          # Main game code with web server
├── platformio.ini        # PlatformIO configuration
├── include/
├── lib/
└── README.md
```

## Customization

### Change WiFi Credentials
Edit in `main.cpp`:
```cpp
const char* ap_ssid = "CycloneGame";
const char* ap_pass = "12345678";
```

### Adjust LED Count
Edit in `main.cpp`:
```cpp
#define NUM_LEDS     24  // Main game LEDs
#define SCORE_LEDS   6   // Score indicator LEDs
```

### Modify Default Speeds
Edit the preset arrays in `applyPreset()` function or adjust via web interface.

## License

This project is open source and available under the MIT License.

## Contributing

Contributions are welcome! Feel free to submit issues or pull requests.

## Author

Am4l-babu

## Acknowledgments

- FastLED library for LED control
- ESP8266 Arduino core
