# Firmware Build & Installation

This page describes how to build and install the Genius Gateway firmware, including build configuration options and build flags.

## Overview

The Genius Gateway firmware is built using PlatformIO, a professional development platform for embedded systems. This guide covers the complete process from source code compilation to firmware installation on your hardware.

## Development Environment Setup

### Prerequisites

*[Content to be added: Required software and tools]*

#### Required Software

1. **Visual Studio Code** - Primary development environment
2. **PlatformIO Extension** - Embedded development platform
3. **Git** - Version control for source code management
4. **Python 3.x** - Required by PlatformIO build system

#### Hardware Requirements

*[Content to be added: Hardware needed for development]*
- Genius Gateway hardware
- USB cable for programming and debugging
- Computer with sufficient resources for compilation

### Source Code Repository

#### Clone the Repository

*[Content to be added: Git repository information]*

```bash
git clone https://github.com/hmbacher/genius-gateway.git
cd genius-gateway
```

#### Repository Structure

*[Content to be added: Overview of repository organization]*
- `/src/` - Main firmware source code
- `/lib/` - Custom libraries and dependencies
- `/interface/` - Web interface source code (SvelteKit)
- `/docs/` - Documentation source files
- `/platformio.ini` - PlatformIO configuration file

## Build Configuration

### PlatformIO Configuration

*[Content to be added: Overview of platformio.ini configuration]*

#### Environment Selection

**Available Build Environments:**
1. **esp32-s3-devkitc-1** - Development board configuration
2. **esp32-s3-custom** - Custom hardware configuration
3. **debug** - Debug build with additional logging
4. **release** - Optimized release build

#### Basic Configuration Options

*[Content to be added: Basic configuration parameters]*

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
```

### Build Flags Configuration

#### Core Build Flags

*[Content to be added: Essential build flags and their purposes]*

**Debug Configuration:**
```ini
build_flags = 
    -DCORE_DEBUG_LEVEL=4          # Enable verbose debugging
    -DDEBUG_ESP_PORT=Serial       # Debug output to serial
    -DDEBUG_ESP_WIFI              # Wi-Fi debugging
    -DDEBUG_ESP_HTTP_CLIENT       # HTTP client debugging
```

**RF Configuration:**
```ini
build_flags =
    -DCC1101_FREQUENCY=868420000  # Set RF frequency (Hz)
    -DCC1101_BANDWIDTH=203125     # Set RF bandwidth (Hz)
    -DCC1101_DEVIATION=47607      # Set frequency deviation
    -DCC1101_DATARATE=99970       # Set data rate (bps)
```

#### Hardware-Specific Flags

*[Content to be added: Hardware configuration build flags]*

**GPIO Configuration:**
```ini
build_flags =
    -DCC1101_CS_PIN=5             # CC1101 chip select pin
    -DCC1101_GDO0_PIN=2           # CC1101 GDO0 interrupt pin
    -DCC1101_GDO2_PIN=4           # CC1101 GDO2 pin
    -DSTATUS_LED_PIN=8            # Status LED pin
```

**Memory Configuration:**
```ini
build_flags =
    -DBOARD_HAS_PSRAM             # Enable PSRAM support
    -DARDUINO_USB_CDC_ON_BOOT=1   # Enable USB CDC on boot
```

#### Feature Flags

*[Content to be added: Optional feature configuration]*

**MQTT Configuration:**
```ini
build_flags =
    -DMQTT_ENABLED=1              # Enable MQTT functionality
    -DMQTT_MAX_PACKET_SIZE=1024   # Set maximum MQTT packet size
    -DHOME_ASSISTANT_DISCOVERY=1  # Enable HA auto-discovery
```

**Web Interface Configuration:**
```ini
build_flags =
    -DWEB_SERVER_ENABLED=1        # Enable web server
    -DWEBSOCKET_ENABLED=1         # Enable WebSocket support
    -DOTA_ENABLED=1               # Enable over-the-air updates
```

### Advanced Configuration

#### Custom Build Profiles

*[Content to be added: Creating custom build profiles]*

**Production Build:**
```ini
[env:production]
build_flags =
    -DCORE_DEBUG_LEVEL=0          # Disable debug output
    -Os                           # Optimize for size
    -DNDEBUG                      # Disable debug assertions
    -DMQTT_ENABLED=1
    -DHOME_ASSISTANT_DISCOVERY=1
```

**Development Build:**
```ini
[env:development]
build_flags =
    -DCORE_DEBUG_LEVEL=4          # Maximum debug output
    -Og                           # Optimize for debugging
    -DDEBUG_BUILD=1               # Enable debug features
    -DPACKET_VISUALIZER=1         # Enable packet visualization
```

## Building the Firmware

### Command Line Build

*[Content to be added: Building from command line]*

#### Basic Build Commands

```bash
# Build default environment
pio run

# Build specific environment
pio run -e esp32-s3-devkitc-1

# Clean build
pio run -t clean

# Build and upload
pio run -t upload
```

#### Build Verification

*[Content to be added: Verifying successful build]*
- Check for compilation errors
- Verify memory usage statistics
- Validate build artifacts

### Visual Studio Code Build

*[Content to be added: Building using VS Code PlatformIO extension]*

#### Using PlatformIO Toolbar

1. **Build Project** - Compile the firmware
2. **Upload** - Program the device
3. **Monitor** - View serial output
4. **Clean** - Clean build artifacts

#### Build Tasks

*[Content to be added: VS Code build tasks and shortcuts]*
- Keyboard shortcuts for common operations
- Custom build tasks configuration
- Integrated terminal usage

## Firmware Installation

### Initial Installation

*[Content to be added: First-time firmware installation]*

#### Preparation Steps

1. **Connect Hardware** - Connect ESP32 to computer via USB
2. **Enter Download Mode** - Put ESP32 into programming mode
3. **Select Port** - Choose correct serial port
4. **Upload Firmware** - Program the device

#### Upload Process

```bash
# Upload firmware to device
pio run -t upload

# Upload and monitor serial output
pio run -t upload -t monitor
```

### Over-the-Air (OTA) Updates

*[Content to be added: OTA update process]*

#### OTA Configuration

**Enable OTA in Build:**
```ini
build_flags =
    -DOTA_ENABLED=1               # Enable OTA functionality
    -DOTA_PASSWORD="your_password" # Set OTA password
```

#### OTA Update Process

1. **Build Firmware** - Compile updated firmware
2. **Connect to Network** - Ensure gateway is connected to Wi-Fi
3. **Upload OTA** - Use network upload instead of serial
4. **Verify Update** - Confirm successful update

### Firmware Verification

#### Post-Installation Checks

*[Content to be added: Verifying successful firmware installation]*

1. **Serial Output** - Check boot messages
2. **Web Interface** - Verify web interface accessibility
3. **RF Communication** - Test RF functionality
4. **Feature Testing** - Verify all enabled features work

#### Version Information

*[Content to be added: Checking firmware version and build information]*
- View version in web interface
- Serial console version output
- Build date and configuration verification

## Troubleshooting Build Issues

### Common Build Problems

*[Content to be added: Common compilation issues and solutions]*

#### Dependency Issues

- Missing libraries or dependencies
- Version compatibility problems
- Platform-specific build issues

#### Configuration Errors

- Invalid build flag syntax
- Conflicting configuration options
- Memory allocation issues

#### Hardware-Specific Issues

- Board definition problems
- Pin assignment conflicts
- Clock configuration issues

### Debug Build Configuration

*[Content to be added: Setting up debug builds for troubleshooting]*

#### Debug Flags

```ini
build_flags =
    -DCORE_DEBUG_LEVEL=5          # Maximum debug output
    -DDEBUG_ESP_PORT=Serial       # Debug to serial port
    -DDEBUG_ESP_WIFI              # Wi-Fi debug messages
    -DDEBUG_ESP_HTTP_CLIENT       # HTTP debug messages
    -DDEBUG_MQTT_CLIENT           # MQTT debug messages
```

#### Debugging Tools

*[Content to be added: Available debugging tools and techniques]*
- Serial monitor debugging
- Remote debugging via telnet
- Log file analysis
- Memory usage analysis

---

*This guide provides complete instructions for building and installing the Genius Gateway firmware with all available configuration options.*