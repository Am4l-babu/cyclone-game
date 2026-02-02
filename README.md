# Cyclone Game - ESP8266 LED Arcade Game

**Version 1.0** - A classic arcade-style cyclone game built with ESP8266 and WS2812B LEDs, featuring an advanced web-based configuration interface with full customization.

## ✨ Features

- 🎮 Classic cyclone gameplay with 6 progressive difficulty levels
- 💡 24 WS2812B LEDs for the main game ring
- 📊 **6 LED score indicators with gradient colors** - Green (easy) to Red (hard) showing all completed levels
- 🌐 WiFi Access Point with modern web interface for remote configuration
- 🎨 **RGB Color Pickers** - Customize running and target LED colors
- ⭐ **Custom Preset System** - Create, save, and manage your own difficulty presets
- ⚙️ Built-in difficulty presets (Easy, Medium, Hard, Insane)
- 💾 **Persistent Storage** - All settings, colors, and custom presets saved to EEPROM
- 🌈 **Waving rainbow attract mode** - Dynamic flowing animation when idle
- 📱 Responsive dark-themed web UI optimized for mobile and desktop

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

1. **Power on** - LEDs display flowing rainbow wave animation (attract mode)
2. **Press button** to start the game
3. A **moving LED** (customizable color, default red) cycles around the ring
4. **Target LED(s)** (customizable color, default green) indicate the target zone
5. **Press button** when the moving LED is in the target zone
6. **Watch your progress** - Score LEDs light up with gradient colors (green=easy, red=hard)
7. Complete all 6 levels to win!

### Difficulty Levels
- **Levels 1-2**: Wider target zone (3 LEDs with gradient)
- **Levels 3-6**: Narrow target zone (1 LED - exact hit required)
- Each level progressively increases the LED speed

### Score LED Indicators
- **Accumulative Display**: All completed levels stay lit
- **Color Gradient**: Visual difficulty feedback
  - Level 1: 🟢 Green (easiest)
  - Level 2: 🟢 Yellow-Green
  - Level 3: 🟡 Yellow
  - Level 4: 🟠 Orange
  - Level 5: 🟠 Orange-Red
  - Level 6: 🔴 Red (hardest)

## Web Interface

Connect to the WiFi network and access the modern web interface to customize all game settings.

### WiFi Credentials
- **SSID**: `CycloneGame`
- **Password**: `12345678`
- **URL**: http://192.168.4.1

### Web Interface Features

#### 💡 Brightness Control
- Adjust LED brightness from 5 to 255
- Large, easy-to-use slider with real-time value display
- Changes apply instantly

#### 🎨 LED Color Customization (NEW in v1.0)
- **Running LED Color**: Choose any RGB color for the moving LED
- **Target LED Color**: Choose any RGB color for the target zone
- Beautiful circular color pickers
- Colors persist across power cycles
- Affects gameplay and win/lose animations

#### ⚡ Built-in Difficulty Presets
Quick one-click difficulty selection:
- 🟢 **Easy** (160-60 ms) - Perfect for beginners
- 🟡 **Medium** (120-30 ms) - Moderate challenge
- 🟠 **Hard** (80-18 ms) - Fast-paced gameplay
- 🔴 **Insane** (50-7 ms) - Expert level

#### ⭐ Custom Preset System (NEW in v1.0)
Create and manage your own difficulty presets:
- **Name your presets** - Give each preset a unique name (up to 19 characters)
- **Set individual speeds** - Define speed for each of the 6 levels (10-250 ms)
- **Save up to 5 presets** - Store your favorite configurations
- **Persistent storage** - All custom presets saved to EEPROM
- **Easy management** - Delete presets you no longer need
- **Dynamic buttons** - Each saved preset gets its own button for quick access

#### 🎚️ Manual Speed Control
- Fine-tune each of the 6 levels individually
- Large sliders for easier adjustment (10-250 ms range)
- Real-time speed display in milliseconds
- Instant save to EEPROM

## Game States

| State | Description |
|-------|-------------|
| 0 | Attract mode (rainbow animation) |
| 1-6 | Playing levels 1 through 6 |
| 98 | Winner animation (flashes target color) |
| 99 | Loser animation (flashes running color) |

## Persistent Configuration

All settings are automatically saved to EEPROM (512 bytes) and persist across power cycles:
- ✅ LED speeds for all 6 levels
- ✅ Brightness setting
- ✅ Running LED color (RGB)
- ✅ Target LED color (RGB)
- ✅ Custom presets (up to 5, with names and speeds)

## What's New in Version 1.0

### Enhanced Visual Feedback
- **Waving Rainbow Attract Mode**: Dynamic flowing rainbow animation instead of static pattern
- **Gradient Score LEDs**: Color-coded difficulty from green (easy) to red (hard)
- **Accumulative Progress Display**: All completed levels stay lit on score strip
- Clear visual representation of game progression

### Enhanced Web UI
- Modern dark theme with purple and pink accents
- Larger, more responsive sliders
- Real-time value displays next to all controls
- Mobile-friendly responsive design
- Version number displayed in header

### Color Customization
- RGB color pickers for both running and target LEDs
- Circular color picker interface
- Colors saved automatically to EEPROM
- Custom colors used in gameplay and animations

### Custom Preset System
- Create personalized difficulty presets
- Name your presets for easy identification
- Set individual speeds for all 6 levels
- Save up to 5 custom presets
- Delete unwanted presets
- Presets persist across reboots
- Dynamic buttons for saved presets

### Improved Storage
- Expanded EEPROM to 512 bytes
- Organized memory map for better management
- All customizations survive power cycles

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

### Change Default Colors
Edit in `main.cpp`:
```cpp
CRGB runningLedColor = CRGB(255, 0, 0);  // Default: Red
CRGB targetLedColor = CRGB(0, 255, 0);   // Default: Green
```

### Modify Max Custom Presets
Edit in `main.cpp`:
```cpp
#define MAX_CUSTOM_PRESETS 5  // Maximum number of custom presets
```

### Adjust Default Speeds
Edit the preset arrays in `applyPreset()` function or use the web interface.

## Screenshots & Usage Tips

### Creating a Custom Preset
1. Open http://192.168.4.1 in your browser
2. Scroll to "Custom Presets" section
3. Enter a preset name (e.g., "My Challenge")
4. Set speeds for levels 1-6 (lower = faster)
5. Click "➕ Add Preset"
6. Your preset appears as a button - click it anytime to apply

### Changing LED Colors
1. Navigate to "LED Colors" section
2. Click the circular color picker
3. Select your desired color
4. Color updates instantly and saves automatically
5. Try the game to see your new colors in action!

## License

This project is open source and available under the MIT License.

## Contributing

Contributions are welcome! Feel free to submit issues or pull requests.

## Author

Am4l-babu

## Acknowledgments

- FastLED library for LED control
- ESP8266 Arduino core
