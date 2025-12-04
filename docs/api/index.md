# API Reference

Complete API documentation for the Genius Gateway.

## Overview

The Genius Gateway provides multiple APIs for integration and automation:

- **REST API** - Standard HTTP endpoints for device management and configuration
- **WebSocket API** - Real-time events and packet streaming
- **MQTT Topics** - Home automation integration and status publishing

## API Categories

### Device Management
- Device discovery and registration
- Status monitoring and alarm management  
- Configuration and settings updates

### System Administration
- Gateway settings and network configuration
- User management and authentication
- System monitoring and diagnostics

### Real-time Communication
- WebSocket events for live updates
- Packet visualization and protocol analysis
- MQTT publishing for home automation

## Getting Started

1. **Authentication** - Obtain API credentials through the web interface
2. **Base URL** - All REST endpoints use `http://[gateway-ip]/rest/`
3. **WebSocket** - Connect to `ws://[gateway-ip]/ws/` for real-time events
4. **MQTT** - Subscribe to `genius-gateway/+/+` topic patterns

## Complete API Documentation

- **REST Endpoints** - Complete REST API reference (coming soon)
- **[WebSocket Events](websocket-api.md)** - Real-time event documentation  
- **[MQTT Topics](mqtt-topics.md)** - MQTT integration guide

## Examples and Libraries

Code examples and client libraries are available in the GitHub repository under the `examples/` directory.