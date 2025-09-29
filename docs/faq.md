# Frequently Asked Questions

Answers to common questions about the ESP32 Genius Gateway.

## General Questions

??? question "What is the ESP32 Genius Gateway?"
    The ESP32 Genius Gateway is a reverse-engineered interface for Hekatron Genius Plus X smoke detection systems. It monitors RF communications between smoke detectors and provides integration with modern home automation systems through MQTT, web interfaces, and REST APIs.

??? question "Is this an official Hekatron product?"
    No, this is an independent reverse-engineering project not affiliated with Hekatron. It monitors the RF communications between Hekatron devices without interfering with their normal operation.

??? question "Is it legal to use this gateway?"
    The gateway operates as a passive RF monitor and does not transmit or interfere with the smoke detection system. However, users should ensure compliance with local regulations regarding RF monitoring and fire safety systems.

??? question "Will this void my smoke detector warranty?"  
    The gateway does not modify or interfere with the smoke detectors themselves. It only monitors their RF communications passively. However, consult your specific warranty terms and local regulations.

## Compatibility Questions

??? question "Which Hekatron devices are supported?"
    The gateway supports Hekatron Genius Plus X smoke detectors equipped with FM Basis Radio Modules. This includes most modern Hekatron wireless smoke detectors operating at 868.35 MHz.

??? question "How many devices can one gateway monitor?"
    A single gateway can monitor up to 50 smoke detectors simultaneously. Performance remains optimal with typical residential and small commercial installations of 5-20 devices.

??? question "What is the range between gateway and smoke detectors?"
    Typical range is 30-100 meters depending on environmental conditions, building construction, and RF interference. Indoor range is usually 20-50 meters through walls and obstacles.

??? question "Can I use multiple gateways in the same location?"
    Yes, multiple gateways can operate in the same area without interference. Each gateway operates as an independent monitor. This can provide redundancy or extended coverage area.

## Technical Questions

??? question "Do I need special hardware or can I use standard ESP32 boards?"
    You can use standard ESP32 development boards with a CC1101 radio module. However, the custom PCB provides optimized RF performance and integrated design. See the [Hardware Guide](hardware/index.md) for wiring details.

??? question "What WiFi networks are supported?"
    The gateway supports 802.11 b/g/n networks on 2.4 GHz. It does not support 5 GHz networks. WPA2 and WPA3 security are supported.

??? question "Can the gateway work without internet connection?"
    Yes, the gateway operates independently on the local network. Internet is only required for initial setup, firmware updates, and cloud-based MQTT brokers. Local MQTT brokers work without internet.

??? question "What happens if the gateway loses power?"
    The gateway will restart automatically when power is restored. It will reconnect to WiFi and MQTT, and resume monitoring. Device configurations and alarm history are preserved in flash memory.

## Setup and Configuration

??? question "How do I find the gateway's IP address?"
    Check your router's DHCP client list for a device named "genius-gateway", use a network scanner app, or connect to the gateway's initial WiFi access point at 192.168.4.1 to configure network settings.

??? question "The gateway isn't discovering my smoke detectors automatically. What should I do?"
    Try triggering a test alarm on the detectors to generate RF traffic. Ensure detectors have FM Basis Radio Module installed and are within range. You can also add devices manually using their serial numbers.

??? question "How do I integrate with Home Assistant?"
    Configure MQTT settings in the gateway and enable Home Assistant discovery. Devices will automatically appear in Home Assistant. See the [Smart Home Integration Guide](features/smart-home-integration.md) for detailed setup instructions.

??? question "Can I access the gateway remotely over the internet?"
    For security reasons, the gateway does not expose services directly to the internet. Use VPN, reverse proxy, or Home Assistant's remote access features for secure remote monitoring.

## MQTT and Integration

??? question "What MQTT broker should I use?"
    Any standard MQTT broker works, including Mosquitto, Home Assistant's built-in broker, or cloud services like AWS IoT or HiveMQ. Local brokers provide better performance and privacy.

??? question "What MQTT topics does the gateway publish to?"
    The gateway publishes device states to `genius-gateway/devices/[device-id]/state` and alarm information to configurable topics. See the [MQTT Topics Reference](api/mqtt-topics.md) for complete topic structure.

??? question "How often does the gateway publish MQTT updates?"
    Updates are published immediately when device states change (alarms, battery levels, etc.). Periodic status updates can be configured, typically every 15 minutes or when devices transmit.

??? question "Can I customize the MQTT topics and payloads?"
    Basic topic structure can be customized through settings. For advanced customization, modify the source code or use MQTT bridges/transformers to reshape messages.

## Troubleshooting

??? question "The web interface is slow or unresponsive. What should I do?"
    This is usually due to weak WiFi signal or network congestion. Check WiFi signal strength, reduce the number of devices being monitored, or disable the packet visualizer if running continuously.

??? question "I'm getting frequent disconnections from MQTT. How can I fix this?"
    Check network connectivity to the MQTT broker, verify credentials and connection settings, and ensure the broker can handle the message frequency. Consider increasing keepalive intervals.

??? question "The packet visualizer shows no activity even though devices are nearby."
    Verify the CC1101 module is properly connected and detected, check antenna connections, ensure devices are within range, and try triggering a test alarm to generate RF traffic.

??? question "Some devices appear offline even though they're working normally."
    Genius Plus X devices transmit infrequently during normal operation. They may appear offline if they haven't transmitted recently. Triggering a test or waiting for the next status transmission should update their status.

## Maintenance and Updates

??? question "How do I update the gateway firmware?"
    Firmware updates can be performed over-the-air (OTA) through the web interface or by connecting via USB and using PlatformIO/Arduino IDE. Always backup configuration before updating.

??? question "How do I backup and restore my configuration?"
    Configuration settings are stored in JSON files that can be downloaded through the web interface. System backups include device configurations, user settings, and alarm history.

??? question "What maintenance does the gateway require?"
    The gateway requires minimal maintenance. Periodically check for firmware updates, verify all devices are still detected, and ensure MQTT connectivity is stable. See the [Troubleshooting Guide](troubleshooting.md) for maintenance schedules.

??? question "How long do configuration settings persist?"
    All settings are stored in non-volatile flash memory and persist through power cycles and firmware updates. Settings are only lost during factory resets or flash memory corruption.

## Safety and Compliance

??? question "Does the gateway interfere with smoke detector operation?"
    No, the gateway is a passive monitoring device that only receives RF signals. It does not transmit or interfere with normal smoke detector operation or safety functionality.

??? question "Can the gateway detect all alarms reliably?"
    The gateway detects RF-transmitted alarms with very high reliability (>99.5%). However, it should not be considered a replacement for proper fire alarm systems or professional monitoring services.

??? question "What happens if the gateway fails during an emergency?"
    The smoke detectors continue to operate independently. The gateway monitors communications but does not control or disable the smoke detectors' local alarms and safety functions.

??? question "Is the gateway suitable for commercial fire safety applications?"
    The gateway is primarily designed for monitoring and integration purposes. For life safety applications, always consult with fire safety professionals and ensure compliance with local fire codes and regulations.

## Development and Customization  

??? question "Can I modify the gateway software for custom features?"
    Yes, the gateway is open source. The software can be modified, extended, and customized. The source code is available on GitHub for modification and extension.

??? question "Can I integrate the gateway with other home automation systems besides Home Assistant?"
    Yes, the gateway provides standard MQTT, REST API, and WebSocket interfaces that work with most automation systems including OpenHAB, Domoticz, Node-RED, and custom applications.

??? question "Are there plans for additional features or device support?"
    Development is ongoing with community contributions. Feature requests and bug reports can be submitted through GitHub. Support for additional Hekatron devices and protocols may be added based on demand and availability of devices for analysis.

??? question "How can I contribute to the project?"
    Contributions are welcome through GitHub including code contributions, documentation improvements, testing, and sharing experiences. Visit the GitHub repository for contribution guidelines.

## Commercial and Professional Use

??? question "Can I use this gateway in commercial installations?"
    Yes, but ensure compliance with local regulations, fire codes, and safety standards. Consider professional support for critical installations and proper documentation for regulatory compliance.

??? question "Is professional support available?"
    Professional support, custom development, and commercial licensing options may be available. Contact the project maintainer through GitHub for commercial inquiries.

??? question "Can I sell products based on this design?"
    The hardware design is licensed under CC BY-SA 4.0 and software under LGPL v3/MIT. Commercial use is permitted following the respective license terms including attribution and sharing improvements.

## Still Need Help?

If you can't find the answer to your question here:

1. **Check the Documentation**: Browse the complete documentation sections for detailed information
2. **Search GitHub Issues**: Look for similar questions or issues in the project repository  
3. **Join Discussions**: Participate in GitHub Discussions for community help
4. **Report Issues**: Create a new GitHub issue for bugs or feature requests

**Useful Links:**
- [GitHub Repository](https://github.com/hmbacher/genius-gateway)
- [Issue Tracker](https://github.com/hmbacher/genius-gateway/issues)
- [Community Discussions](https://github.com/hmbacher/genius-gateway/discussions)
- [Documentation Home](index.md)