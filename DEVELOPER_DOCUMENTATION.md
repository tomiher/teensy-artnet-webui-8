# Teensy 4 Art-Net LED Controller - Developer Documentation

## Project Overview

This project implements a comprehensive Art-Net LED controller for Teensy 4.1 microcontroller with web-based configuration interface. The system supports multiple independent outputs, EEPROM configuration storage, and various test modes.

## Development Environment

### Required Software
- **Arduino IDE:** 2.0.0 or later
- **Teensyduino:** 1.59.0 or later
- **Board:** Teensy 4.1

### Required Libraries
- **NativeEthernet:** Built-in Teensy library (preferred over external Ethernet libraries)
- **NativeEthernetUdp:** Built-in Teensy library for UDP communication
- **OctoWS2811:** Built-in Teensy library for driving WS281x LEDs
- **EEPROM:** Built-in Arduino library for persistent storage
- **SPI:** Built-in Arduino library

### Hardware Requirements
- **Teensy 4.1** microcontroller
- **Ethernet connection** (built-in Ethernet on Teensy 4.1)
- **WS281x compatible LEDs** (WS2811, WS2812, WS2812B, etc.)
- **Push button** (optional, for test modes and reset functions)

## Code Structure Overview

### Core Functionality
The system operates as both an Art-Net receiver and a web server, allowing real-time LED control via Art-Net protocol and configuration through a web interface.

### Main Components
1. **Art-Net Reception:** Native UDP-based Art-Net packet processing
2. **LED Control:** OctoWS2811-based multi-output LED driving
3. **Web Server:** HTTP server for configuration interface
4. **EEPROM Storage:** Persistent configuration storage
5. **Button Control:** Physical button for test modes and system reset

## Detailed Code Structure

### Lines 1-50: Headers and Initial Setup
```cpp
// Library includes
#include <NativeEthernet.h>      // Line 14: Native Ethernet support
#include <NativeEthernetUdp.h>   // Line 15: UDP communication
#include <OctoWS2811.h>          // Line 17: LED control
#include <EEPROM.h>              // Line 18: Persistent storage
```

### Lines 51-100: Configuration Structure
```cpp
struct Config {                   // Line 29: Main configuration structure
  int ledsPerPin[8/16/32];       // Line 30: LEDs per output
  int startUniverse[8/16/32];    // Line 31: Starting universe per output
  int startAddress[8/16/32];     // Line 32: Starting address per output
  int brightness[8/16/32];       // Line 33: Brightness per output
  // Network and LED type settings
};
```

### Lines 100-200: Hardware Configuration
```cpp
const int numPins = 8/16/32;     // Line 95: Number of outputs
byte pinList[numPins] = {...};   // Line 96: Pin assignments
const int ledsPerPin[numPins];   // Line 100: Static LED allocation
```

### Lines 200-300: Memory and Object Setup
```cpp
DMAMEM int displayMemory[...];   // Line 152: DMA memory for LEDs
OctoWS2811 leds(...);           // Line 155: LED controller object
```

### Lines 300-400: Art-Net Implementation
```cpp
byte artnetPacket[530];          // Line 283: Art-Net packet buffer
void readArtNet() {              // Line 399: Art-Net packet parser
  // UDP packet reading and Art-Net header validation
}
```

### Lines 400-500: DMX Data Processing
```cpp
void onDmxFrame(...) {           // Line 421: DMX data handler
  // Universe mapping and LED pixel setting
  // Independent output control logic
}
```

### Lines 500-700: Button Control System
```cpp
void handleButton() {            // Line 487: Button state machine
  // Short press: Test mode cycling
  // 3s press: Art-Net ↔ Test mode toggle
  // 10s press: Network settings reset
  // 20s press: Full configuration reset
}
```

### Lines 700-900: Test Mode Functions
```cpp
void updateRainbow() {           // Line 739: Rainbow animation
void updateSolidColor() {        // Line 696: Solid color test modes
void hsvToRgb(...) {            // Line 807: Color conversion
```

### Lines 900-1200: Web Server Core
```cpp
void handleWebServer() {         // Line 925: HTTP request handler
void sendConfigPage(...) {       // Line 987: Main web interface
  // Dynamic HTML generation with current configuration
}
```

### Lines 1200-1500: Configuration Management
```cpp
void handleConfigPost(...) {     // Line 1538: Form data processing
void sendConfigJSON(...) {       // Line 1756: AJAX configuration API
```

### Lines 1500-1900: EEPROM Functions
```cpp
void saveConfigToEEPROM() {      // Line 1836: Configuration saving
bool loadConfigFromEEPROM() {    // Line 1847: Configuration loading
void checkMemoryAndResetIfNeeded(); // Line 1869: Memory overflow protection
```

## Key Technical Details

### Art-Net Implementation
- **Protocol:** Art-Net 4 compatible
- **Port:** 6454 (standard Art-Net port)
- **Universe Range:** 0-999 (configurable per output)
- **Channel Mapping:** 510 channels per universe (standard DMX)
- **Packet Validation:** Full Art-Net header and opcode verification

### LED Control
- **Library:** OctoWS2811 (hardware-optimized)
- **Supported Types:** WS2811, WS2812, WS2812B, WS2813, SK6812
- **Color Orders:** RGB, GRB, BRG, RBG, GBR, BGR, WRGB
- **Max LEDs per Output:** 1020 (memory limited)
- **Refresh Rate:** Up to 40 FPS

### Memory Management
- **DMA Memory:** Allocated for OctoWS2811 display buffer
- **Static Allocation:** All major objects use static memory
- **Overflow Protection:** Automatic reset to safe values if memory exceeded
- **EEPROM Size:** 1080 bytes per configuration

### Network Configuration
- **Default IP:** 10.10.0.42 (configurable)
- **Subnet:** 255.255.255.0 (configurable)
- **MAC Address:** 04:E5:E9:00:69:EC (configurable)
- **Web Port:** 80 (HTTP)
- **Art-Net Port:** 6454 (UDP)

### EEPROM Layout
- **Address 0:** Configuration structure
- **Magic Number:** Version-specific validation
- **Size:** Varies by output count (8/16/32)
- **Validation:** CRC-like magic number checking

## Compilation Notes

### Memory Considerations
- **RAM Usage:** ~50KB for 8x1020 LEDs
- **Flash Usage:** ~200KB
- **EEPROM Usage:** ~1KB per configuration

### Optimization Flags
- **Speed Optimization:** -O2 recommended
- **USB Type:** Serial (for debugging) or No USB (production)
- **CPU Speed:** 600MHz (balanced performance/power)

### Common Issues
1. **Multiple .ino files:** Only one .ino file per project folder
2. **Library conflicts:** Use Teensy native libraries over external ones
3. **Memory overflow:** Reduce LED counts if system restarts
4. **Network conflicts:** Ensure unique IP addresses on network

## Version Differences

### 8-Output Version (Teensy_4_Art-Net_8_webui_1.0.0)
- **Outputs:** 8 independent channels
- **Default Universes:** 0, 6, 12, 18, 24, 30, 36, 42
- **Memory Usage:** ~33KB RAM
- **Pin Assignment:** 2, 3, 4, 5, 6, 7, 8, 9

### 16-Output Version (Teensy_4_Art-Net_16_webui_1.0.0)
- **Outputs:** 16 independent channels
- **Default Universes:** 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90
- **Memory Usage:** ~65KB RAM
- **Pin Assignment:** 2, 3, 4, 5, 6, 7, 8, 9, 21, 20, 19, 18, 17, 16, 15, 14

### 32-Output Version (Teensy_4_Art-Net_32_webui_1.0.0)
- **Outputs:** 32 independent channels
- **Default Universes:** 0, 6, 12, 18, 24, 30, 36, 42, 48, 54, 60, 66, 72, 78, 84, 90, 96, 102, 108, 114, 120, 126, 132, 138, 144, 150, 156, 162, 168, 174, 180, 186
- **Memory Usage:** ~130KB RAM
- **Pin Assignment:** Extended pin list for all 32 outputs

## Development Workflow

### Initial Setup
1. Install Arduino IDE and Teensyduino
2. Select Teensy 4.1 board
3. Configure libraries (use native Teensy versions)
4. Set optimization level to -O2

### Testing
1. **Serial Monitor:** Debug output during development
2. **Network Testing:** Ping test for connectivity
3. **Art-Net Testing:** Use lighting software (MadMapper, Resolume, etc.)
4. **Web Interface:** Test all configuration options

### Debugging
1. **Serial Output:** Enable debug messages during development
2. **LED Indicators:** Built-in LED shows system status
3. **Network Tools:** Use ping, telnet for connectivity testing
4. **Memory Monitoring:** Check free RAM in setup()

### Production Deployment
1. Remove all Serial.print() statements
2. Set USB Type to "No USB" (optional)
3. Test all functionality
4. Document network configuration
5. Create backup of working configuration

## Future Development Notes

### Potential Improvements
1. **Wireless Support:** Add Wi-Fi capability
2. **sACN Support:** Implement E1.31 protocol
3. **DMX Output:** Add DMX512 output capability
4. **Advanced Effects:** Built-in effect generator
5. **Multi-Device Sync:** Synchronization between multiple controllers

### Code Maintenance
1. **Library Updates:** Monitor Teensyduino updates
2. **Memory Optimization:** Consider dynamic allocation for larger setups
3. **Web Interface:** Modernize with responsive design
4. **Security:** Add authentication for web interface
5. **Backup/Restore:** Configuration import/export functionality



