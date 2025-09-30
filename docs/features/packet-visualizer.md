# Packet Visualizer

This page describes the packet visualizer tool and its usage for analyzing RF communication with screenshots.

## Overview

The Packet Visualizer is a powerful diagnostic tool that provides real-time visualization and analysis of RF communication packets between the Genius Gateway and Hekatron Genius Plus X smoke detectors. This tool is essential for troubleshooting communication issues and understanding protocol behavior.

## Main Visualizer Interface

### Live Packet Stream

*[Content to be added: Screenshot of the main packet visualizer interface showing live packet capture]*

The main interface displays:

#### Real-Time Packet List

*[Content to be added: Description of packet list display]*
- **Timestamp**: Precise time when packet was received/transmitted
- **Direction**: Incoming (from devices) or outgoing (to devices) indicators
- **Source/Destination**: Device addresses or identifiers
- **Packet Type**: Classification of packet content
- **Size**: Packet size in bytes
- **Status**: Success, error, or retry indicators

#### Packet Content Preview

*[Content to be added: Hex and ASCII packet content display]*
- **Raw Data View**: Hexadecimal representation of packet contents
- **ASCII Interpretation**: Human-readable ASCII interpretation where applicable
- **Field Highlighting**: Color-coded packet fields and structures
- **Protocol Parsing**: Automatic parsing of known packet structures

## Detailed Packet Analysis

### Packet Detail View

*[Content to be added: Screenshot of detailed packet analysis interface]*

Selecting any packet opens the detailed analysis view:

#### Packet Header Analysis

*[Content to be added: Packet header breakdown]*
- **Protocol Version**: Communication protocol version identification
- **Packet Type**: Detailed packet type classification
- **Sequence Numbers**: Packet sequencing and ordering information
- **Address Fields**: Source and destination device addressing
- **Control Flags**: Protocol control and status flags

#### Payload Decoding

*[Content to be added: Payload content analysis]*
- **Data Structure**: Organized display of packet payload
- **Field Interpretation**: Meaning and significance of each data field
- **Value Conversion**: Conversion of raw values to engineering units
- **Status Information**: Device status information contained in packet

#### Error Detection and Correction

*[Content to be added: Packet integrity analysis]*
- **Checksum Verification**: CRC or checksum validation status
- **Error Detection**: Identification of corrupted or invalid packets
- **Correction Information**: Error correction capabilities and results
- **Retry Analysis**: Analysis of packet retransmission attempts

## Filtering and Search

### Filter Options

*[Content to be added: Screenshot of packet filtering interface]*

#### Basic Filters

**Available Filter Criteria:**
1. **Device Address**: Show packets from/to specific devices
2. **Packet Type**: Filter by specific packet types
3. **Time Range**: Filter packets within specific time windows
4. **Direction**: Show only incoming or outgoing packets
5. **Status**: Filter by success, error, or retry status

#### Advanced Filters

*[Content to be added: Advanced filtering capabilities]*
- **Protocol Field Values**: Filter based on specific field contents
- **Data Pattern Matching**: Search for specific data patterns
- **Signal Quality**: Filter based on RF signal quality metrics
- **Complex Logic**: Combine multiple filter criteria with AND/OR logic

### Search Functionality

*[Content to be added: Packet search capabilities]*
- **Content Search**: Search packet contents for specific data
- **Pattern Recognition**: Identify recurring communication patterns
- **Anomaly Detection**: Highlight unusual or unexpected packets
- **Bookmark System**: Save and recall interesting packets

## Statistics and Analysis

### Communication Statistics

*[Content to be added: Screenshot of statistics dashboard]*

#### Traffic Analysis

**Statistical Displays:**
1. **Packet Rate**: Packets per second over time
2. **Data Volume**: Bytes transmitted/received over time
3. **Error Rate**: Communication error statistics
4. **Device Activity**: Per-device communication activity levels

#### Protocol Analysis

*[Content to be added: Protocol-level statistics]*
- **Packet Type Distribution**: Frequency of different packet types
- **Response Times**: Communication latency measurements
- **Retry Statistics**: Analysis of communication reliability
- **Protocol Efficiency**: Overhead and efficiency metrics

### Signal Quality Metrics

*[Content to be added: RF performance analysis]*
- **RSSI Trends**: Received signal strength over time
- **SNR Analysis**: Signal-to-noise ratio measurements
- **Link Quality**: Overall RF link quality assessment
- **Interference Detection**: RF interference identification and analysis

## Export and Logging

### Data Export Options

*[Content to be added: Screenshot of export interface]*

#### Export Formats

**Available Export Formats:**
1. **CSV Format**: Comma-separated values for spreadsheet analysis
2. **JSON Export**: Structured data format for programmatic analysis
3. **PCAP Format**: Standard packet capture format for Wireshark analysis
4. **Plain Text**: Human-readable text format

#### Export Customization

*[Content to be added: Export customization options]*
- **Field Selection**: Choose which packet fields to include
- **Time Range Selection**: Export specific time periods
- **Filter Application**: Export only filtered packet data
- **Format Options**: Customize output format details

### Continuous Logging

*[Content to be added: Automated logging configuration]*
- **Log File Management**: Automatic log file creation and rotation
- **Storage Limits**: Disk space management for packet logs
- **Trigger-Based Logging**: Start/stop logging based on specific events
- **Remote Logging**: Send packet data to external logging systems

## Troubleshooting with the Visualizer

### Common Issues and Diagnosis

#### Communication Problems

**Diagnostic Approach:**
1. **Check Packet Flow**: Verify bidirectional communication
2. **Analyze Error Rates**: Identify patterns in communication failures
3. **Signal Quality Assessment**: Evaluate RF link quality
4. **Protocol Compliance**: Verify proper protocol implementation

#### Device-Specific Issues

*[Content to be added: Device-specific troubleshooting guidance]*
- **Device Discovery Problems**: Diagnose device detection issues
- **Configuration Issues**: Identify device configuration problems
- **Performance Issues**: Analyze device response and performance
- **Maintenance Alerts**: Understand device maintenance requirements

### Advanced Analysis Techniques

*[Content to be added: Advanced diagnostic techniques]*
- **Pattern Analysis**: Identify communication patterns and anomalies
- **Timing Analysis**: Evaluate communication timing and synchronization
- **Correlation Analysis**: Correlate packet data with system events
- **Predictive Analysis**: Identify potential future communication issues

---

*The Packet Visualizer provides essential insights into the RF communication system for effective troubleshooting and optimization.*