# Genius Gateway

> **Reverse-engineered gateway for Hekatron Genius Plus X smoke detection systems**

A sophisticated IoT gateway that enables monitoring and integration of Hekatron Genius Plus X smoke detectors through reverse-engineered RF communication protocols. Built on ESP32 with modern web technologies.

[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://hmbacher.github.io/genius-gateway/)
[![License: Hardware](https://img.shields.io/badge/License-CC%20BY--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-sa/4.0/)
[![License: Software](https://img.shields.io/badge/License-GPL%20v3%2FMIT-green.svg)](LICENSE)
[![License: Docs](https://img.shields.io/badge/License-CC%20BY%204.0-blue.svg)](https://creativecommons.org/licenses/by/4.0/)

## 🔥 What is the Genius Gateway?

The **Genius Gateway** is a reverse-engineered interface for Hekatron Genius Plus X smoke detection systems. It enables real-time monitoring, MQTT integration, and web-based management of Genius smoke detectors without requiring proprietary Hekatron gateways.

### Key Features

- **🔍 Protocol Reverse Engineering** - Complete analysis of proprietary Genius communication protocol
- **📡 RF Communication** - CC1101-based 868 MHz radio interface compatible with FM Basis Radio Modules  
- **🌐 Modern Web Interface** - Responsive Svelte-based web UI for device management and monitoring
- **📊 Real-time Monitoring** - Live packet visualization, alarm states, and device status monitoring
- **🏠 Home Automation** - MQTT integration with Home Assistant and other automation platforms
- **⚙️ Open Source** - Complete hardware and software designs available under open licenses

## 🚀 Quick Start

1. **Hardware Setup** - Assemble or order the gateway hardware (ESP32-S3 + CC1101)
2. **Firmware Installation** - Flash the firmware to your ESP32
3. **Configuration** - Configure WiFi and basic settings via web interface
4. **MQTT Integration** - Connect to Home Assistant or other automation systems

## 📡 System Architecture

```
┌─────────────────────┐    868MHz RF    ┌──────────────────┐    WiFi/MQTT    ┌─────────────────┐
│ Hekatron Genius     │ ◄─────────────► │ ESP32 Gateway    │ ◄──────────────► │ Home Automation │
│ Smoke Detectors     │                 │ (CC1101 Radio)   │                 │ Systems         │
└─────────────────────┘                 └──────────────────┘                 └─────────────────┘
```

The gateway monitors RF communications between Hekatron devices and translates them into standard protocols (MQTT, HTTP, WebSocket) for integration with modern home automation systems.

## 🛠️ Technical Specifications

- **Hardware**: ESP32-S3 microcontroller with CC1101 sub-GHz transceiver
- **Frequency**: 868.35 MHz (European SRD band)
- **Protocol**: Reverse-engineered Hekatron Genius Plus X communication
- **Range**: Typically 30-100m depending on environment
- **Connectivity**: WiFi, MQTT, HTTP REST API, WebSocket
- **Interface**: Modern responsive web UI built with SvelteKit

## 📖 Documentation

**Complete documentation is available at: [https://hmbacher.github.io/genius-gateway/](https://hmbacher.github.io/genius-gateway/)**

The documentation includes:

- **[Quick Start Guide](https://hmbacher.github.io/genius-gateway/quick-start/)** - Get up and running in 30 minutes
- **[Background Information](https://hmbacher.github.io/genius-gateway/background/)** - Understanding Hekatron systems and ESP32 SvelteKit
- **[Reverse Engineering](https://hmbacher.github.io/genius-gateway/reverse-engineering/)** - Protocol analysis and RF communication details  
- **[Hardware Design](https://hmbacher.github.io/genius-gateway/hardware/)** - Complete schematics, PCB layout, and assembly guide
- **[Gateway Features](https://hmbacher.github.io/genius-gateway/features/)** - Web interface, device management, and packet visualization
- **[Installation & Setup](https://hmbacher.github.io/genius-gateway/setup/)** - Hardware setup, firmware building, and configuration
- **[API Reference](https://hmbacher.github.io/genius-gateway/api/)** - REST endpoints, WebSocket events, and MQTT topics

## 🔧 Development

The project is built using:

- **Backend**: C++ with Arduino framework and PlatformIO
- **Frontend**: SvelteKit with TypeScript and Vite  
- **Hardware**: KiCad for PCB design and schematics
- **Documentation**: MkDocs with Material theme

### Project Structure

```
genius-gateway/
├── src/                 # C++ backend source code
├── interface/           # SvelteKit frontend application  
├── lib/                 # Arduino libraries and dependencies
├── docs/               # Documentation source (MkDocs)
├── cae/                # Hardware design files (KiCad)
├── scripts/            # Build and utility scripts
└── data/               # Web assets and configuration files
```

## ⚠️ Important Notes

- **Reverse Engineering**: This is a reverse-engineered implementation not affiliated with Hekatron
- **Safety**: Use at your own risk and ensure compliance with local regulations
- **RF Regulations**: Operates as receive-only monitor compliant with European 868 MHz SRD regulations
- **Fire Safety**: Does not replace official Hekatron controllers or compromise detector safety functions

## 📄 License

This project uses multiple licenses for different components:

- **Hardware Design**: [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) - PCB, schematics, CAD files
- **Backend Software**: [GPL v3](LICENSE) - ESP32 firmware, protocol implementation  
- **Frontend Software**: [MIT](LICENSE) - Web interface and frontend components
- **Documentation**: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) - All documentation and guides

See the [LICENSE](LICENSE) file for complete details and rationale for each license choice.

## 🤝 Contributing

Contributions are welcome! Please see the [documentation](https://hmbacher.github.io/genius-gateway/) for development guidelines.

- **GitHub Repository**: [hmbacher/genius-gateway](https://github.com/hmbacher/genius-gateway)
- **Issues & Bug Reports**: [GitHub Issues](https://github.com/hmbacher/genius-gateway/issues)
- **Discussions**: [GitHub Discussions](https://github.com/hmbacher/genius-gateway/discussions)

---

**🔗 For complete documentation, visit: [https://hmbacher.github.io/genius-gateway/](https://hmbacher.github.io/genius-gateway/)**
