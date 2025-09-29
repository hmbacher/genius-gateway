# Smart Home Integration via MQTT

This page describes the smart home integration features via MQTT, including Home Assistant integration and screenshots.

## Overview

The ESP32 Genius Gateway provides comprehensive smart home integration through MQTT (Message Queuing Telemetry Transport) protocol. This integration enables seamless connectivity with popular home automation platforms like Home Assistant, OpenHAB, and custom IoT systems.

## MQTT Integration Architecture

### System Overview

*[Content to be added: Diagram showing MQTT integration architecture]*

The MQTT integration connects:

#### Gateway to MQTT Broker

*[Content to be added: Description of gateway-to-broker connection]*
- **Broker Connection**: Secure connection to MQTT broker
- **Topic Hierarchy**: Organized topic structure for all data
- **Quality of Service**: Reliable message delivery configuration
- **Authentication**: Secure authentication with username/password or certificates

#### MQTT Broker to Home Automation

*[Content to be added: Integration with home automation platforms]*
- **Home Assistant**: Native Home Assistant MQTT discovery
- **OpenHAB**: OpenHAB MQTT binding integration
- **Node-RED**: Flow-based integration with Node-RED
- **Custom Systems**: Integration with custom IoT platforms

## Home Assistant Integration

### Auto-Discovery Configuration

*[Content to be added: Screenshot of Home Assistant auto-discovery setup]*

#### MQTT Discovery Setup

**Configuration Process:**
1. **Enable Discovery**: Activate MQTT discovery in gateway settings
2. **Discovery Topic**: Configure Home Assistant discovery topic prefix
3. **Device Registration**: Automatic device registration in Home Assistant
4. **Entity Creation**: Automatic creation of sensors and controls

#### Device Entities

*[Content to be added: Home Assistant device entities created]*
- **Binary Sensors**: Smoke detection status for each detector
- **Sensors**: Battery level, signal strength, and diagnostic data
- **Device Trackers**: Online/offline status for each smoke detector
- **Buttons**: Test buttons for manual device testing
- **Diagnostics**: System health and performance metrics

### Home Assistant Dashboard

*[Content to be added: Screenshot of Home Assistant dashboard showing Genius Gateway devices]*

#### Lovelace Cards

**Dashboard Elements:**
1. **Status Overview**: System-wide status summary card
2. **Device Grid**: Individual smoke detector status cards
3. **Battery Monitoring**: Battery level monitoring for all devices
4. **Alert History**: Recent alerts and system events
5. **System Diagnostics**: Gateway health and performance metrics

#### Automated Actions

*[Content to be added: Home Assistant automation examples]*
- **Alert Responses**: Automated responses to fire alerts
- **Maintenance Reminders**: Battery replacement and service reminders
- **Integration with Other Systems**: Coordinate with lighting, HVAC, and security
- **Mobile Notifications**: Push notifications to mobile devices

## MQTT Topic Structure

### Topic Hierarchy

*[Content to be added: Complete MQTT topic structure documentation]*

#### Base Topic Structure

```
genius-gateway/
├── status/                    # Gateway status information
├── devices/                   # Individual device data
│   ├── {device-id}/
│   │   ├── state/            # Device state information
│   │   ├── battery/          # Battery status
│   │   ├── signal/           # RF signal quality
│   │   └── commands/         # Device control commands
├── system/                    # System-wide information
│   ├── health/               # System health metrics
│   ├── statistics/           # Communication statistics
│   └── logs/                 # System logs (if enabled)
└── config/                    # Configuration topics
    ├── discovery/            # Auto-discovery information
    └── settings/             # System settings
```

#### Device-Specific Topics

*[Content to be added: Detailed device topic structure]*
- **State Topics**: Current device operational state
- **Attribute Topics**: Device attributes and metadata
- **Command Topics**: Commands sent to devices
- **Status Topics**: Device status and health information

### Message Formats

#### JSON Message Structure

*[Content to be added: Example JSON message formats]*

**Device Status Message Example:**
```json
{
  "device_id": "12345678",
  "timestamp": "2024-01-15T10:30:00Z",
  "state": "normal",
  "battery_level": 85,
  "signal_strength": -45,
  "last_test": "2024-01-14T09:00:00Z",
  "online": true
}
```

**Alert Message Example:**
```json
{
  "device_id": "12345678",
  "alert_type": "smoke_detected",
  "severity": "high",
  "timestamp": "2024-01-15T10:35:00Z",
  "location": "Living Room",
  "acknowledged": false
}
```

## Configuration and Setup

### Gateway MQTT Configuration

*[Content to be added: Screenshot of MQTT configuration interface]*

#### Broker Settings

**Configuration Options:**
1. **Broker Address**: MQTT broker hostname or IP address
2. **Port Configuration**: MQTT broker port (usually 1883 or 8883 for SSL)
3. **Authentication**: Username and password configuration
4. **SSL/TLS Settings**: Secure connection configuration with certificates
5. **Keep-Alive**: Connection keep-alive interval settings

#### Topic Configuration

*[Content to be added: Topic configuration options]*
- **Base Topic**: Root topic prefix for all gateway messages
- **Device Topics**: Topic structure for individual devices
- **Discovery Prefix**: Home Assistant discovery topic prefix
- **Retain Settings**: Message retention configuration

### Quality of Service (QoS) Settings

#### QoS Level Configuration

**QoS Options:**
1. **QoS 0 (At most once)**: Fast, no delivery guarantee
2. **QoS 1 (At least once)**: Guaranteed delivery, possible duplicates
3. **QoS 2 (Exactly once)**: Guaranteed single delivery (highest overhead)

#### Message-Specific QoS

*[Content to be added: QoS configuration for different message types]*
- **Alert Messages**: High QoS for critical alerts
- **Status Updates**: Medium QoS for important status information
- **Diagnostic Data**: Low QoS for non-critical diagnostic information
- **Command Messages**: High QoS for reliable command delivery

## Advanced Integration Features

### Custom MQTT Clients

*[Content to be added: Screenshot of custom MQTT client configuration]*

#### Third-Party Integration

**Integration Possibilities:**
1. **Node-RED Flows**: Visual programming for complex automations
2. **InfluxDB**: Time-series database integration for historical data
3. **Grafana Dashboards**: Advanced visualization and monitoring
4. **Custom Applications**: Integration with custom software applications

#### API-Style MQTT Usage

*[Content to be added: Using MQTT as an API interface]*
- **Command-Response Patterns**: Request-response communication patterns
- **Batch Operations**: Bulk operations through MQTT commands
- **Configuration Management**: Remote configuration through MQTT
- **Firmware Updates**: Over-the-air updates via MQTT (if supported)

### Security and Privacy

#### MQTT Security

*[Content to be added: Security configuration options]*
- **TLS Encryption**: Encrypted communication with MQTT broker
- **Certificate Authentication**: Client certificate authentication
- **Access Control Lists**: Topic-level access control
- **Message Encryption**: Additional payload encryption if required

#### Privacy Considerations

*[Content to be added: Privacy and data protection features]*
- **Data Minimization**: Configure what data is shared via MQTT
- **Anonymous Mode**: Remove identifying information from messages
- **Local Processing**: Keep sensitive data processing local to gateway
- **Retention Policies**: Configure message retention and cleanup

## Troubleshooting MQTT Integration

### Connection Issues

*[Content to be added: Common connection troubleshooting]*
- **Connectivity Testing**: Test MQTT broker connectivity
- **Authentication Debugging**: Diagnose authentication failures
- **Firewall Configuration**: Network firewall configuration requirements
- **SSL Certificate Issues**: Troubleshooting certificate problems

### Message Delivery Issues

*[Content to be added: Message delivery troubleshooting]*
- **QoS Configuration**: Ensure appropriate QoS levels
- **Topic Permissions**: Verify topic access permissions
- **Message Size Limits**: Check for message size restrictions
- **Rate Limiting**: Handle broker rate limiting

---

*The MQTT integration provides powerful and flexible smart home connectivity for the ESP32 Genius Gateway.*