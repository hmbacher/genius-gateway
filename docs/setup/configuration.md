# Configuration

This page describes post-installation configuration of the Genius Gateway via the web interface.

## Overview

After successful firmware installation, the Genius Gateway requires initial configuration to begin monitoring your Hekatron Genius Plus X smoke detection system. This configuration is performed through the intuitive web-based interface.

## Initial Setup Process

### First Boot Configuration

*[Content to be added: Screenshot of initial setup wizard]*

#### Wi-Fi Setup

The gateway starts in Access Point (AP) mode for initial configuration:

**Connection Process:**
1. **Connect to Gateway AP** - Look for "Genius-Gateway-XXXXXX" network
2. **Open Configuration Portal** - Navigate to `http://192.168.4.1`
3. **Wi-Fi Network Selection** - Choose your home Wi-Fi network
4. **Enter Credentials** - Provide Wi-Fi password
5. **Apply Settings** - Save and restart gateway

#### Network Configuration

*[Content to be added: Network configuration options]*
- **DHCP Configuration** - Automatic IP address assignment
- **Static IP Setup** - Manual IP address configuration
- **DNS Settings** - Custom DNS server configuration
- **Hostname Configuration** - Set custom hostname for network identification

### Web Interface Access

#### Finding the Gateway

*[Content to be added: Methods to locate gateway on network]*

**Discovery Methods:**
1. **Router Interface** - Check connected devices in router admin
2. **Network Scanner** - Use network discovery tools
3. **mDNS Discovery** - Access via `http://genius-gateway.local`
4. **Serial Console** - View IP address in serial output

#### Initial Login

*[Content to be added: Screenshot of login interface]*
- **Default Credentials** - Initial login information
- **Password Change** - Mandatory password change on first login
- **Security Setup** - Additional security configuration options

## Core System Configuration

### RF Communication Setup

*[Content to be added: Screenshot of RF configuration interface]*

#### CC1101 Transceiver Settings

**RF Parameters:**
1. **Operating Frequency** - Set frequency for your region (868 MHz for Europe)
2. **Power Level** - Adjust transmit power (typically 10-12 dBm)
3. **Data Rate** - Communication speed (usually auto-detected)
4. **Channel Configuration** - RF channel settings

#### Protocol Configuration

*[Content to be added: Protocol-specific settings]*
- **Packet Format** - Hekatron protocol configuration
- **Timing Parameters** - Communication timing settings
- **Error Handling** - Retry and timeout configuration
- **Device Discovery** - Auto-discovery settings

### System Identification

#### Gateway Information

*[Content to be added: Gateway identification settings]*
- **Device Name** - Friendly name for the gateway
- **Location** - Physical location description
- **Time Zone** - Local time zone configuration
- **NTP Configuration** - Network time synchronization

#### Network Services

*[Content to be added: Network service configuration]*
- **Web Server Settings** - HTTP/HTTPS configuration
- **WebSocket Configuration** - Real-time communication settings
- **mDNS Settings** - Network discovery configuration
- **SSH Access** - Remote access configuration (if enabled)

## Device Management Configuration

### Auto-Discovery Settings

*[Content to be added: Screenshot of device discovery configuration]*

#### Discovery Parameters

**Configuration Options:**
1. **Auto-Discovery Mode** - Enable/disable automatic device detection
2. **Discovery Interval** - How often to scan for new devices
3. **Device Timeout** - When to consider devices offline
4. **Discovery Range** - RF range for device discovery

#### Device Registration

*[Content to be added: Device registration settings]*
- **Manual Registration** - Manually add known devices
- **Device Filtering** - Criteria for accepting new devices
- **Device Naming** - Automatic naming conventions
- **Location Assignment** - Default location assignment rules

### Device Organization

#### Location Management

*[Content to be added: Location and zone configuration]*
- **Location List** - Define available locations/rooms
- **Zone Creation** - Create logical groupings of devices
- **Hierarchy Setup** - Organize locations in building hierarchy
- **Custom Attributes** - Define custom device properties

#### Device Categories

*[Content to be added: Device categorization options]*
- **Device Types** - Classify different detector types
- **Priority Levels** - Emergency response priority assignment
- **Maintenance Groups** - Group devices for maintenance scheduling
- **Access Control** - Device access and control permissions

## User Interface Configuration

### Display Preferences

*[Content to be added: Screenshot of UI customization options]*

#### Theme and Layout

**Customization Options:**
1. **Color Theme** - Light/dark mode selection
2. **Dashboard Layout** - Customize main dashboard
3. **Information Density** - Detailed vs. compact views
4. **Language Settings** - Interface language selection

#### Data Display

*[Content to be added: Data presentation configuration]*
- **Update Intervals** - How often to refresh data
- **Historical Periods** - Data retention and display timeframes
- **Status Indicators** - Customize status symbols and colors
- **Chart Configuration** - Historical data visualization settings

### Notification Configuration

#### Alert Settings

*[Content to be added: Alert and notification configuration]*
- **Alert Levels** - Define what events trigger alerts
- **Visual Notifications** - On-screen alert presentation
- **Audio Alerts** - Sound notification configuration
- **Alert Acknowledgment** - How alerts are acknowledged and cleared

#### External Notifications

*[Content to be added: External notification setup]*
- **Email Notifications** - Email alert configuration
- **SMS Notifications** - Text message alerts (if supported)
- **Webhook Integration** - Custom webhook notifications
- **Mobile Push** - Mobile app notifications

## Integration Configuration

### MQTT Integration Setup

*[Content to be added: Screenshot of MQTT configuration interface]*

#### Broker Configuration

**MQTT Settings:**
1. **Broker Address** - MQTT broker hostname or IP
2. **Port Configuration** - MQTT broker port (1883/8883)
3. **Authentication** - Username and password setup
4. **SSL/TLS Configuration** - Secure connection setup

#### Topic Structure

*[Content to be added: MQTT topic configuration]*
- **Base Topic** - Root topic for all messages
- **Device Topics** - Individual device topic structure
- **Discovery Topics** - Home Assistant discovery configuration
- **Command Topics** - Control command topic setup

### Home Assistant Integration

*[Content to be added: Home Assistant specific configuration]*

#### Auto-Discovery Setup

- **Discovery Prefix** - HA discovery topic configuration
- **Device Registration** - How devices appear in HA
- **Entity Naming** - Naming convention for HA entities
- **Device Classes** - Proper HA device classification

#### Integration Features

*[Content to be added: HA integration features]*
- **Sensor Creation** - What sensors are created in HA
- **Binary Sensors** - Smoke detection status sensors
- **Device Tracking** - Online/offline status tracking
- **Service Calls** - Available HA service calls

## Security Configuration

### Access Control

*[Content to be added: Screenshot of security settings]*

#### Authentication

**Security Options:**
1. **Admin Password** - Administrative password configuration
2. **Session Management** - Login session settings
3. **Password Policy** - Password complexity requirements
4. **Account Lockout** - Failed login attempt handling

#### API Security

*[Content to be added: API security configuration]*
- **API Keys** - Generate and manage API keys
- **Rate Limiting** - Prevent API abuse
- **Access Logging** - Log all access attempts
- **IP Restrictions** - Limit access by IP address

### Data Privacy

#### Data Handling

*[Content to be added: Privacy and data protection settings]*
- **Data Collection** - What data is collected and stored
- **Data Retention** - How long data is kept
- **Data Sharing** - What data is shared externally
- **Anonymization** - Privacy protection options

#### Logging Configuration

*[Content to be added: Logging and audit configuration]*
- **Log Levels** - What information is logged
- **Log Retention** - How long logs are kept
- **Remote Logging** - External log server configuration
- **Audit Trail** - Configuration change logging

## Maintenance and Monitoring

### System Health

*[Content to be added: System monitoring configuration]*

#### Health Checks

**Monitoring Options:**
1. **Performance Monitoring** - CPU and memory usage tracking
2. **Communication Health** - RF communication quality monitoring
3. **Error Tracking** - System error detection and reporting
4. **Diagnostic Data** - System diagnostic information collection

#### Maintenance Schedules

*[Content to be added: Automated maintenance configuration]*
- **Device Testing** - Automated device test schedules
- **System Backups** - Configuration backup scheduling
- **Update Checks** - Automatic firmware update checking
- **Health Reports** - Regular system health reporting

### Backup and Recovery

#### Configuration Backup

*[Content to be added: Backup and recovery options]*
- **Automatic Backups** - Scheduled configuration backups
- **Manual Export** - Manual configuration export
- **Backup Storage** - Where backups are stored
- **Restore Procedures** - How to restore configurations

#### Factory Reset

*[Content to be added: System reset options]*
- **Soft Reset** - Reset to default configuration
- **Factory Reset** - Complete system reset
- **Selective Reset** - Reset specific subsystems
- **Recovery Mode** - Emergency recovery procedures

---

*This configuration guide ensures your Genius Gateway is properly set up and optimized for your specific environment and requirements.*