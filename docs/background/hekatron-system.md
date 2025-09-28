# Hekatron Genius Plus X System

The Genius Gateway interfaces with Hekatron's Genius Plus X smoke detection ecosystem through reverse-engineered RF communication.

## System Overview

The Genius Plus X is a professional wireless smoke detection system designed for commercial and residential applications. Key characteristics include:

- **Wireless Communication**: 868 MHz RF using FM Basis Radio Modules
- **Distributed Intelligence**: Each detector operates independently while communicating with the network
- **Professional Grade**: Designed to meet stringent fire safety standards
- **Proprietary Protocol**: Uses custom communication protocols for device coordination

## FM Basis Radio Module

The wireless communication is handled by integrated FM Basis Radio Modules:

**Technical Specifications**
- Frequency: 868.35 MHz (European SRD band)
- Modulation: FSK (Frequency Shift Keying)
- Output Power: ≤ 25mW ERP
- Communication Range: Typically 30-100m (depending on environment)
- Protocol: Proprietary Hekatron format

**Integration**
- Factory-integrated into smoke detectors
- Handles device discovery, alarm propagation, and status reporting
- Manages mesh-like communication between devices
- Implements timing and collision avoidance algorithms

## Device Types

The Genius Plus X ecosystem includes various detector types:

**Smoke Detectors**
- Optical smoke detection with integrated radio
- Battery-powered with 10+ year lithium cells
- Local sounder and visual indicators
- Status LED and test button

**Accessories** 
- Remote indicators and sounders
- Isolation modules for system segmentation
- Programming and testing tools

## Communication Patterns

The system implements several communication patterns that the gateway monitors:

**Alarm Propagation**
- Immediate alarm transmission when smoke is detected
- Cascade activation of connected detectors
- Acknowledgment and status confirmation messages

**System Monitoring**
- Periodic status heartbeats
- Battery level reporting
- Communication quality metrics

**Commissioning**
- Device discovery and network joining
- Configuration parameter distribution
- Test mode activation and results

## Official Resources

For complete technical specifications and official documentation:

- **[Hekatron Official Website](https://www.hekatron.de/)**  
  Main product information and specifications

- **[Genius Plus X Product Page](https://www.hekatron.de/genius-plus-x)**  
  Detailed product lineup and technical data

- **[Technical Documentation](https://www.hekatron.de/downloads)**  
  Official installation guides and specifications

- **[Hekatron Partner Portal](https://partner.hekatron.de/)**  
  Professional resources and support materials

## Gateway Integration

!!! info "Reverse Engineering Approach"
    The Genius Gateway does not use official Hekatron interfaces or APIs. Instead, it monitors and decodes the RF communications between devices using reverse-engineered protocol knowledge.

The gateway provides:

**Passive Monitoring**
- Non-intrusive monitoring of device communications
- Real-time alarm detection and status updates  
- Historical logging of all network activity

**Protocol Translation**
- Conversion of proprietary formats to standard MQTT messages
- Web-based visualization of device states and communications
- Integration with home automation systems

**Enhanced Functionality**
- Device discovery and automatic configuration
- Custom alarm filtering and notification rules
- Advanced packet analysis and protocol debugging

## Compliance & Safety

!!! warning "Important Safety Information"
    The Genius Gateway is designed for monitoring purposes only and does not replace official Hekatron controllers or interfaces. For life safety applications, always follow official Hekatron installation and maintenance guidelines.

**Regulatory Considerations**
- The gateway operates as a receive-only monitor (no RF transmission)
- Complies with European 868 MHz SRD regulations for monitoring devices
- Does not interfere with normal Hekatron system operation

**Installation Guidelines**  
- Position gateway within range of Hekatron devices (typically 10-50m)
- Ensure clear line of sight when possible for optimal reception
- Avoid RF interference sources (WiFi routers, microwave ovens, etc.)

## Next Steps

- **[Protocol Analysis](../reverse-engineering/protocol-analysis.md)** - Learn how the communication protocol was reverse-engineered
- **[Hardware Design](../hardware/index.md)** - Understand the gateway's RF reception capabilities
- **[Device Management](../features/device-management.md)** - See how discovered devices are managed in the gateway