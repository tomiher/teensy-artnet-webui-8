# Teensy 4 Art-Net LED Controller - User Manual

## Quick Start Guide

### System Overview
The Teensy 4 Art-Net LED Controller is a professional-grade device that receives Art-Net data over Ethernet and drives WS281x LED strips. It features a web-based configuration interface and supports multiple independent outputs.

### Basic Specifications
- **Supported LEDs:** WS2811, WS2812, WS2812B, WS2813, SK6812
- **Maximum LEDs per Output:** 1020 pixels
- **Available Versions:** 8, 16, or 32 independent outputs
- **Network:** 10/100 Ethernet with Art-Net protocol
- **Configuration:** Web-based interface
- **Power:** 5V via USB or external supply

## Initial Setup

### 1. Hardware Connections
1. Connect Teensy 4.1 to your network via Ethernet cable
2. Connect LED strips to designated output pins (see pin mapping below)
3. Connect push button to pin 30 (optional, for test modes)
4. Power the Teensy via USB or external 5V supply

### 2. Network Configuration
**Default Network Settings:**
- **IP Address:** 10.10.0.42
- **Subnet Mask:** 255.255.255.0
- **MAC Address:** 04:E5:E9:00:69:EC

### 3. Web Interface Access
1. Connect your computer to the same network
2. Open web browser and navigate to: `http://10.10.0.42`
3. The configuration page will load automatically

## Pin Mapping

### 8-Output Version
- **Outputs 1-8:** Pins 2, 3, 4, 5, 6, 7, 8, 9

### 16-Output Version  
- **Outputs 1-8:** Pins 2, 3, 4, 5, 6, 7, 8, 9
- **Outputs 9-16:** Pins 21, 20, 19, 18, 17, 16, 15, 14

### 32-Output Version
- **Outputs 1-32:** Extended pin mapping (see developer documentation)

## Configuration

### Output Configuration
Each output can be configured independently:

**Number of Pixels (1-1020)**
- Set the number of LEDs connected to each output
- Maximum 1020 pixels per output due to memory limitations

**Start Universe (0-999)**
- Art-Net universe where this output begins receiving data
- Default spacing: 6 universes per 1020-pixel output

**Start Address (1-510)**
- DMX channel within the start universe where data begins
- Standard DMX universe has 512 channels (510 usable for RGB)

**Brightness (0-255)**
- Individual brightness control per output
- 255 = 100% brightness, 0 = off

### LED Configuration
**LED Type:**
- WS2811
- WS2812/WS2812B  
- WS2813
- SK6812

**Color Order:**
- RGB, GRB, BRG, RBG, GBR, BGR
- WRGB (for RGBW LEDs)

### Network Settings
**IP Address:** Set static IP for the controller
**Subnet Mask:** Network subnet configuration  
**MAC Address:** Unique network identifier

## Test Modes

### Web Interface Test
Use the dropdown menu to select test patterns:
- **Rainbow:** Animated rainbow effect
- **Red:** Solid red color
- **Green:** Solid green color  
- **Blue:** Solid blue color

### Button Control Test
Physical button on pin 30 provides additional control:

**Short Press (< 3 seconds):**
- Cycles through test modes: Rainbow → Red → Green → Blue → Rainbow

**Long Press (3 seconds):**
- Toggles between Art-Net mode and test mode
- LED blinks twice when switching modes

**Network Reset (10 seconds):**
- Resets network settings to default
- LED blinks three times during press

**Full Reset (20 seconds):**
- Resets all settings to factory defaults
- LED stays solid during press

## Usage

### Art-Net Setup
1. Configure your lighting software (MadMapper, Resolume, etc.)
2. Set Art-Net output to controller's IP address
3. Configure universes according to your output mapping
4. Each 1020-pixel output requires 6 universes (510 channels each)

### Universe Calculation
**Formula:** 
- End Universe = Start Universe + ((Pixels × 3 + Start Address - 2) ÷ 510)
- End Address = ((Pixels × 3 + Start Address - 1) % 510) + 1

**Example (1020 pixels, Universe 0, Address 1):**
- Channels needed: 1020 × 3 = 3060
- Universes used: 0, 1, 2, 3, 4, 5 (6 total)
- End address in Universe 5: 510

### Status Indicators

**Built-in LED (Pin 13):**
- **Art-Net Mode:** Slow blink (1 Hz)
- **Test Mode:** Fast blink (4 Hz)  
- **Button Press:** Special patterns during long presses
- **Network Activity:** Brief flashes during data reception

**Web Interface Status:**
- **Test Mode Active:** Green message displayed
- **Configuration Changes:** Automatic restart notification
- **Connection Status:** Real-time updates

## Troubleshooting

### Network Issues

**Cannot Access Web Interface:**
1. Check Ethernet cable connection
2. Verify computer and controller are on same network
3. Try ping test: `ping 10.10.0.42`
4. Check for IP conflicts on network
5. If multiple network interfaces active, disable Wi-Fi temporarily

**Art-Net Not Working:**
1. Verify lighting software Art-Net output is enabled
2. Check universe mapping matches controller configuration
3. Confirm IP address in lighting software
4. Test with simple patterns first
5. Monitor built-in LED for activity

### LED Issues

**No LED Output:**
1. Check power supply to LED strips
2. Verify correct pin connections
3. Test with built-in test modes first
4. Check LED type and color order settings
5. Ensure data line connections are secure

**Incorrect Colors:**
1. Adjust color order setting (RGB, GRB, etc.)
2. Verify LED type matches configuration
3. Check for voltage drops in long LED runs
4. Test with single color modes

**Flickering or Glitches:**
1. Check power supply capacity
2. Add decoupling capacitors near LEDs
3. Reduce brightness to lower current draw
4. Verify data line signal integrity
5. Check for electromagnetic interference

### Universe Conflicts

**Overlapping Universes:**
- Each output should use unique universe ranges
- Default 8-output spacing: 0, 6, 12, 18, 24, 30, 36, 42
- Adjust start universes to avoid conflicts

**Missing Data:**
1. Check universe numbering in lighting software
2. Verify start universe and address settings
3. Ensure lighting software sends all required universes
4. Monitor network traffic with tools like Wireshark

### Memory Issues

**System Restarts Automatically:**
1. Total pixel count may exceed memory limits
2. Reduce pixels per output
3. Use fewer active outputs
4. Check power supply stability

**Configuration Not Saving:**
1. Allow full restart cycle (wait 10 seconds)
2. Check EEPROM memory integrity
3. Reset to factory defaults if corrupted
4. Avoid power interruptions during save

### Performance Issues

**Slow Response:**
1. Check network bandwidth usage
2. Reduce Art-Net frame rate in software
3. Optimize lighting software settings
4. Monitor for packet loss

**Uneven LED Updates:**
1. Verify all required universes are being sent
2. Check for network timing issues
3. Ensure consistent frame rates from software
4. Test with reduced pixel counts

## Factory Reset

### Method 1: Button Reset
1. Hold button for 20 seconds
2. LED will show solid light
3. Release when LED turns off
4. System will restart with default settings

### Method 2: Web Interface
1. Access web interface
2. Set all values to defaults manually
3. Save configuration
4. System will restart

### Default Settings After Reset
- **IP:** 10.10.0.42
- **Subnet:** 255.255.255.0  
- **MAC:** 04:E5:E9:00:69:EC
- **All Outputs:** 200 pixels
- **LED Type:** WS2812/WS2812B
- **Color Order:** GRB
- **Brightness:** 255 (100%)
- **Universes:** 0, 1, 2, 3, 4, 5, 6, 7

## Technical Support

### Diagnostic Information
When reporting issues, please include:
1. Controller version (8/16/32 outputs)
2. Network configuration
3. LED types and quantities
4. Lighting software and version
5. Network topology
6. Error symptoms and timing

### Common Solutions
- **Web interface timeout:** Refresh browser, check network
- **Partial LED operation:** Check universe mapping
- **System instability:** Verify power supply capacity
- **Network conflicts:** Use network scanner to check IP usage

### Advanced Configuration
For complex setups requiring custom universe mapping or special LED configurations, refer to the Developer Documentation for detailed technical information.



