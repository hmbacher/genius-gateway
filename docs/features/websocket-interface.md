# WebSocket Logging Interface

This page describes the WebSocket logging interface and its usage for real-time system monitoring with screenshots.

## Overview

The WebSocket Logging Interface provides real-time access to system logs, debug information, and operational data from the ESP32 Genius Gateway. This powerful tool enables developers, system administrators, and advanced users to monitor system behavior in real-time.

## Main Logging Interface

### Real-Time Log Display

*[Content to be added: Screenshot of the main WebSocket logging interface showing live log stream]*

The main interface features:

#### Live Log Stream

*[Content to be added: Description of real-time log display]*
- **Continuous Updates**: Live streaming of log messages as they occur
- **Timestamp Display**: Precise timestamps for each log entry
- **Log Level Indicators**: Visual indicators for different log severity levels
- **Message Formatting**: Structured display of log message content
- **Auto-Scroll**: Automatic scrolling to show latest log entries

#### Log Level Classification

**Log Level Hierarchy:**
1. **ERROR**: Critical errors requiring immediate attention
2. **WARN**: Warning conditions that should be monitored
3. **INFO**: General information about system operation
4. **DEBUG**: Detailed debugging information for development
5. **TRACE**: Very detailed trace information for deep debugging

## Log Filtering and Search

### Filter Configuration

*[Content to be added: Screenshot of log filtering interface]*

#### Basic Filters

**Available Filter Options:**
1. **Log Level**: Filter by minimum log level (Error, Warn, Info, Debug, Trace)
2. **Component**: Filter by specific system components or modules
3. **Time Range**: Show logs within specific time windows
4. **Message Content**: Filter based on message text content
5. **Device ID**: Show logs related to specific smoke detectors

#### Advanced Filtering

*[Content to be added: Advanced filter configuration]*
- **Regular Expressions**: Use regex patterns for complex filtering
- **Multiple Criteria**: Combine multiple filter criteria with AND/OR logic
- **Custom Patterns**: Save and reuse custom filter configurations
- **Exclusion Filters**: Exclude specific types of log messages

### Search Functionality

*[Content to be added: Log search capabilities]*
- **Real-Time Search**: Search through live log stream
- **Historical Search**: Search through stored log history
- **Pattern Recognition**: Identify recurring log patterns
- **Context Display**: Show surrounding log entries for search results

## System Component Logging

### RF Communication Logs

*[Content to be added: Screenshot of RF communication logging]*

#### CC1101 Transceiver Logs

**RF-Specific Log Categories:**
1. **Packet Transmission**: Outgoing packet transmission details
2. **Packet Reception**: Incoming packet reception information
3. **RF Configuration**: Transceiver configuration changes
4. **Signal Quality**: RSSI, SNR, and link quality measurements
5. **Error Conditions**: RF communication errors and recovery

#### Protocol Processing

*[Content to be added: Protocol-level logging information]*
- **Packet Parsing**: Details about packet interpretation and parsing
- **Protocol State**: Communication protocol state machine transitions
- **Device Discovery**: Logs related to device discovery and registration
- **Command Processing**: Logs for command transmission and responses

### Web Server Logs

*[Content to be added: Web server and HTTP logging]*
- **HTTP Requests**: Incoming web requests and responses
- **WebSocket Connections**: WebSocket connection events and data
- **Authentication**: User authentication attempts and results
- **API Calls**: REST API endpoint access and usage

### System Operation Logs

#### Device Management

*[Content to be added: Device management logging]*
- **Device Registration**: New device discovery and registration
- **Status Updates**: Device status changes and updates
- **Configuration Changes**: Device configuration modifications
- **Health Monitoring**: Device health checks and diagnostics

#### MQTT Integration

*[Content to be added: MQTT communication logging]*
- **Connection Status**: MQTT broker connection events
- **Message Publishing**: Outgoing MQTT message publishing
- **Message Reception**: Incoming MQTT command messages
- **Integration Events**: Home Assistant and other integration events

## Log Export and Storage

### Export Options

*[Content to be added: Screenshot of log export interface]*

#### Export Formats

**Available Export Formats:**
1. **Plain Text**: Simple text format for basic analysis
2. **JSON**: Structured JSON format for programmatic analysis
3. **CSV**: Comma-separated values for spreadsheet analysis
4. **Syslog**: Standard syslog format for external log servers

#### Export Configuration

*[Content to be added: Export customization options]*
- **Time Range Selection**: Export specific time periods
- **Filter Application**: Export only filtered log data
- **Compression**: Compress exported files for storage efficiency
- **Scheduling**: Automated periodic log exports

### Log Storage Management

#### Local Storage

*[Content to be added: Local log storage configuration]*
- **Storage Limits**: Configure maximum log file sizes
- **Retention Policies**: Automatic cleanup of old log files
- **Rotation Settings**: Log file rotation configuration
- **Performance Impact**: Balance logging detail with system performance

#### Remote Logging

*[Content to be added: Remote log server configuration]*
- **Syslog Servers**: Send logs to external syslog servers
- **Cloud Logging**: Integration with cloud-based logging services
- **Real-Time Streaming**: Stream logs to external monitoring systems
- **Backup Logging**: Redundant logging for critical systems

## Development and Debugging

### Debug Mode Configuration

*[Content to be added: Screenshot of debug configuration interface]*

#### Debug Level Control

**Debug Configuration Options:**
1. **Component-Specific Levels**: Set different debug levels per component
2. **Runtime Adjustment**: Change debug levels without system restart
3. **Performance Monitoring**: Monitor performance impact of debug logging
4. **Selective Enabling**: Enable debugging only for specific operations

#### Development Tools

*[Content to be added: Developer-focused logging tools]*
- **Function Tracing**: Trace function entry and exit
- **Memory Usage**: Monitor memory allocation and usage
- **Performance Metrics**: Track system performance and bottlenecks
- **Error Tracking**: Detailed error tracking and stack traces

### Remote Debugging Support

*[Content to be added: Remote debugging capabilities]*
- **Remote Log Access**: Access logs from multiple gateway instances
- **Distributed Logging**: Aggregate logs from multiple devices
- **Real-Time Monitoring**: Monitor multiple gateways simultaneously
- **Centralized Analysis**: Centralized analysis of distributed log data

## Integration with External Tools

### Log Analysis Tools

*[Content to be added: Integration with external log analysis]*
- **ELK Stack**: Integration with Elasticsearch, Logstash, and Kibana
- **Splunk**: Splunk integration for enterprise log analysis
- **Grafana**: Visualization of log-based metrics and trends
- **Custom Dashboards**: Create custom monitoring dashboards

### Monitoring and Alerting

*[Content to be added: Monitoring system integration]*
- **Alert Triggers**: Generate alerts based on log patterns
- **Threshold Monitoring**: Alert on log rate or error thresholds
- **Automated Responses**: Trigger automated responses to log events
- **Notification Integration**: Send alerts via email, SMS, or messaging

---

*The WebSocket Logging Interface provides powerful real-time monitoring and debugging capabilities for the ESP32 Genius Gateway.*