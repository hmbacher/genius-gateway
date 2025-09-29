# Hardware Setup

This page references the Gateway Hardware section for complete hardware setup information.

## Overview

The hardware setup for the ESP32 Genius Gateway involves several stages, from PCB assembly to final installation in an appropriate enclosure. This page provides a quick overview and directs you to the comprehensive hardware documentation.

## Quick Hardware Setup Guide

### 1. PCB Assembly

For complete PCB assembly information, please refer to the **[Gateway Hardware](../hardware/index.md)** section:

- **[Gateway Overview](../hardware/gateway-overview.md)** - See assembled gateway pictures and specifications
- **[Schematics](../hardware/schematics.md)** - Complete electronic design documentation
- **[PCB Layout](../hardware/pcb-layout.md)** - PCB design files and manufacturing specifications
- **[Bill of Materials](../hardware/bom.md)** - Complete component list and procurement information

### 2. Component Assembly

*[Content to be added: Brief assembly overview with reference to detailed hardware docs]*

#### Required Tools

- Soldering equipment for SMD components
- Anti-static handling equipment
- Flux and solder (lead-free recommended)
- Magnification equipment for small components

#### Assembly Process

Detailed assembly instructions are available in the **[Gateway Hardware](../hardware/index.md)** section. The basic process includes:

1. **SMD Component Placement** - Place all surface-mount components
2. **Reflow Soldering** - Solder all SMD components
3. **Through-Hole Components** - Install any through-hole components
4. **Quality Inspection** - Visual and electrical inspection
5. **Initial Testing** - Basic functionality testing

### 3. Enclosure Installation

For complete enclosure information, see **[Case Selection](../hardware/case.md)**:

- Compatible enclosure models and manufacturers
- Assembly pictures and installation guides
- RF performance considerations for case selection
- Mounting and installation options

## Pre-Installation Checklist

### Hardware Verification

Before proceeding with software installation:

- [ ] All components properly soldered and placed
- [ ] No short circuits or solder bridges
- [ ] Power supply connections verified
- [ ] Antenna properly connected
- [ ] Programming interface accessible

### Required Equipment

*[Content to be added: Equipment needed for initial setup]*

#### Programming Equipment

- USB cable for ESP32 programming
- Computer with PlatformIO or Arduino IDE
- Serial terminal software for debugging

#### Testing Equipment

- Multimeter for voltage verification
- RF analyzer or SDR for RF testing (optional)
- Oscilloscope for debugging (optional)

## Initial Power-On

### Safety Checks

*[Content to be added: Safety verification before first power-on]*

1. **Visual Inspection** - Check for obvious assembly issues
2. **Power Supply Verification** - Verify correct voltage levels
3. **Short Circuit Check** - Ensure no shorts between power rails
4. **Component Orientation** - Verify polarized component orientation

### First Power-On Test

*[Content to be added: Initial power-on procedure]*

1. **Apply Power** - Connect power supply
2. **Check Voltages** - Verify all voltage rails are correct
3. **LED Indicators** - Check for expected LED behavior
4. **Serial Output** - Monitor serial output for boot messages

## Next Steps

After successful hardware setup:

1. **[Firmware Build & Installation](firmware.md)** - Build and install the gateway firmware
2. **[Configuration](configuration.md)** - Configure the gateway through the web interface
3. **[MQTT & Home Assistant](mqtt-homeassistant.md)** - Set up smart home integration

## Troubleshooting Hardware Issues

### Common Assembly Problems

*[Content to be added: Common hardware assembly issues and solutions]*

#### Power Issues

- No power indication
- Incorrect voltage levels
- High current consumption

#### RF Issues

- Poor RF performance
- Antenna connection problems
- RF interference

#### Programming Issues

- Cannot program ESP32
- Serial communication problems
- Boot mode issues

### Getting Help

For detailed hardware troubleshooting:

- Review the **[Hardware Documentation](../hardware/index.md)**
- Check the **[Troubleshooting Guide](../troubleshooting.md)**
- Consult the **[FAQ](../faq.md)** for common issues

---

*This page provides a quick overview of hardware setup. For complete details, please refer to the comprehensive Gateway Hardware documentation.*