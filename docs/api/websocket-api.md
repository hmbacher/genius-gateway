# WebSocket API

This page describes the WebSocket API for real-time communication with the Genius Gateway.

## Overview

The WebSocket API provides real-time, bidirectional communication with the Genius Gateway. This enables instant notifications of device status changes, live data streaming, and interactive control capabilities.

## WebSocket Connection

### Connection URL

Connect to the WebSocket endpoint at:

```
ws://{gateway-ip}/ws/api/v1
```

For secure connections (if SSL is enabled):
```
wss://{gateway-ip}/ws/api/v1
```

### Connection Parameters

**Query Parameters:**
- `api_key` - API key for authentication (if required)
- `client_id` - Unique client identifier for session management
- `subscribe` - Comma-separated list of initial subscriptions

**Example Connection:**
```javascript
const ws = new WebSocket('ws://192.168.1.100/ws/api/v1?client_id=client_001&subscribe=alerts,device_status');
```

### Authentication

*[Content to be added: WebSocket authentication methods]*

**Authentication Options:**
1. **Query Parameter** - Include API key in connection URL
2. **First Message** - Send authentication message after connection
3. **Token-based** - Use JWT tokens for session authentication

## Message Format

All WebSocket messages use JSON format with a standardized structure:

### Outgoing Messages (Client → Gateway)

```json
{
  "id": "msg_001",
  "type": "command",
  "action": "subscribe",
  "payload": {
    "topics": ["device_status", "alerts"]
  },
  "timestamp": "2024-01-15T10:30:00Z"
}
```

### Incoming Messages (Gateway → Client)

```json
{
  "id": "msg_001",
  "type": "response",
  "status": "success",
  "payload": {
    "message": "Subscription successful",
    "subscribed_topics": ["device_status", "alerts"]
  },
  "timestamp": "2024-01-15T10:30:05Z"
}
```

### Message Types

**Client Message Types:**
- `command` - Execute a command or action
- `subscribe` - Subscribe to data streams
- `unsubscribe` - Unsubscribe from data streams
- `request` - Request specific data
- `ping` - Keep-alive ping message

**Server Message Types:**
- `response` - Response to client command/request
- `notification` - Real-time event notification
- `data` - Streamed data update
- `error` - Error message
- `pong` - Response to ping message

## Subscription System

### Available Topics

**Device-Related Topics:**
- `device_status` - Device online/offline status changes
- `device_data` - Real-time device data updates
- `device_discovery` - New device discoveries
- `device_battery` - Battery level changes

**Alert Topics:**
- `alerts` - New alerts and alarm events
- `alert_status` - Alert acknowledgment and resolution
- `alert_history` - Historical alert data updates

**System Topics:**
- `system_status` - System health and status updates
- `system_logs` - Live system log streaming
- `system_stats` - Performance statistics updates
- `rf_activity` - RF communication activity

**Configuration Topics:**
- `config_changes` - Configuration update notifications
- `firmware_updates` - Firmware update progress

### Subscribe to Topics

```json
{
  "id": "sub_001",
  "type": "command",
  "action": "subscribe",
  "payload": {
    "topics": ["device_status", "alerts"],
    "filters": {
      "device_ids": ["12345678", "87654321"],
      "alert_types": ["smoke_detected", "low_battery"]
    }
  }
}
```

**Response:**
```json
{
  "id": "sub_001",
  "type": "response",
  "status": "success",
  "payload": {
    "subscribed_topics": ["device_status", "alerts"],
    "active_subscriptions": 2
  }
}
```

### Unsubscribe from Topics

```json
{
  "id": "unsub_001",
  "type": "command",
  "action": "unsubscribe",
  "payload": {
    "topics": ["system_logs"]
  }
}
```

## Real-Time Notifications

### Device Status Updates

Receive instant notifications when device status changes:

```json
{
  "type": "notification",
  "topic": "device_status",
  "payload": {
    "device_id": "12345678",
    "previous_status": "normal",
    "current_status": "offline",
    "timestamp": "2024-01-15T10:35:00Z",
    "location": "Living Room",
    "last_seen": "2024-01-15T10:30:00Z"
  }
}
```

### Alert Notifications

Receive immediate alert notifications:

```json
{
  "type": "notification",
  "topic": "alerts",
  "payload": {
    "alert_id": "alert_001",
    "device_id": "12345678",
    "alert_type": "smoke_detected",
    "severity": "high",
    "timestamp": "2024-01-15T10:35:00Z",
    "location": "Kitchen",
    "details": {
      "sensor_value": 850,
      "threshold": 500
    }
  }
}
```

### Battery Level Updates

Monitor device battery levels in real-time:

```json
{
  "type": "notification",
  "topic": "device_battery",
  "payload": {
    "device_id": "12345678",
    "battery_level": 15,
    "previous_level": 25,
    "low_battery_warning": true,
    "timestamp": "2024-01-15T10:40:00Z"
  }
}
```

### System Status Updates

Receive system health and status notifications:

```json
{
  "type": "notification",
  "topic": "system_status",
  "payload": {
    "system_health": "warning",
    "previous_health": "good",
    "issues": ["high_cpu_usage", "low_memory"],
    "timestamp": "2024-01-15T10:45:00Z"
  }
}
```

## Data Streaming

### Live Device Data

Stream continuous data from devices:

```json
{
  "id": "stream_001",
  "type": "command",
  "action": "start_stream",
  "payload": {
    "stream_type": "device_data",
    "device_ids": ["12345678"],
    "interval": 5000,
    "data_types": ["signal_strength", "battery_level"]
  }
}
```

**Streamed Data:**
```json
{
  "type": "data",
  "topic": "device_data",
  "payload": {
    "device_id": "12345678",
    "timestamp": "2024-01-15T10:50:00Z",
    "data": {
      "signal_strength": -42,
      "battery_level": 85,
      "temperature": 22.5
    }
  }
}
```

### System Statistics Streaming

Stream real-time system performance data:

```json
{
  "id": "stats_stream",
  "type": "command",
  "action": "start_stream",
  "payload": {
    "stream_type": "system_stats",
    "interval": 10000,
    "metrics": ["cpu_usage", "memory_usage", "rf_activity"]
  }
}
```

### Log Streaming

Stream live system logs:

```json
{
  "type": "data",
  "topic": "system_logs",
  "payload": {
    "timestamp": "2024-01-15T10:55:00Z",
    "level": "info",
    "component": "rf_controller",
    "message": "Device 12345678 communication successful"
  }
}
```

## Device Control

### Send Commands to Devices

Control devices through WebSocket commands:

```json
{
  "id": "cmd_001",
  "type": "command",
  "action": "device_control",
  "payload": {
    "device_id": "12345678",
    "command": "test",
    "parameters": {
      "test_type": "full",
      "timeout": 30
    }
  }
}
```

**Command Response:**
```json
{
  "id": "cmd_001",
  "type": "response",
  "status": "success",
  "payload": {
    "command_id": "cmd_12345",
    "device_id": "12345678",
    "status": "executing",
    "estimated_duration": 30
  }
}
```

### Command Progress Updates

Receive updates on long-running commands:

```json
{
  "type": "notification",
  "topic": "command_progress",
  "payload": {
    "command_id": "cmd_12345",
    "device_id": "12345678",
    "progress": 50,
    "status": "testing_smoke_chamber",
    "timestamp": "2024-01-15T11:00:15Z"
  }
}
```

## System Management

### Configuration Updates

Send configuration updates via WebSocket:

```json
{
  "id": "config_001",
  "type": "command",
  "action": "update_config",
  "payload": {
    "section": "mqtt",
    "config": {
      "enabled": true,
      "broker": "192.168.1.100",
      "port": 1883
    }
  }
}
```

### System Commands

Execute system-level commands:

```json
{
  "id": "sys_001",
  "type": "command",
  "action": "system_command",
  "payload": {
    "command": "restart",
    "delay": 5000
  }
}
```

## Error Handling

### Error Message Format

```json
{
  "id": "msg_001",
  "type": "error",
  "error": {
    "code": "INVALID_DEVICE_ID",
    "message": "The specified device ID does not exist",
    "details": {
      "device_id": "invalid_id",
      "available_devices": ["12345678", "87654321"]
    }
  },
  "timestamp": "2024-01-15T11:05:00Z"
}
```

### Common Error Codes

*[Content to be added: Common WebSocket error codes]*

**Connection Errors:**
- `AUTH_REQUIRED` - Authentication required
- `INVALID_API_KEY` - Invalid or expired API key
- `RATE_LIMIT_EXCEEDED` - Too many messages per second

**Command Errors:**
- `INVALID_COMMAND` - Unknown command type
- `MISSING_PARAMETERS` - Required parameters missing
- `DEVICE_OFFLINE` - Target device is offline
- `COMMAND_TIMEOUT` - Command execution timeout

## Connection Management

### Keep-Alive Mechanism

Send periodic ping messages to maintain connection:

```json
{
  "id": "ping_001",
  "type": "ping",
  "timestamp": "2024-01-15T11:10:00Z"
}
```

**Pong Response:**
```json
{
  "id": "ping_001",
  "type": "pong",
  "timestamp": "2024-01-15T11:10:00Z"
}
```

### Reconnection Strategy

*[Content to be added: Reconnection best practices]*

**Recommended Approach:**
1. Implement exponential backoff for reconnection attempts
2. Store subscription state for restoration after reconnection
3. Handle duplicate messages during reconnection
4. Implement heartbeat monitoring for connection health

### Connection Limits

*[Content to be added: Connection limitations]*
- **Maximum Concurrent Connections**: 50
- **Per-Client Message Rate**: 100 messages/minute
- **Subscription Limit**: 20 topics per connection

## Client Libraries

### JavaScript Example

```javascript
class GatewayWebSocket {
  constructor(gatewayIP, apiKey) {
    this.gatewayIP = gatewayIP;
    this.apiKey = apiKey;
    this.ws = null;
    this.messageId = 0;
    this.callbacks = new Map();
  }

  connect() {
    const url = `ws://${this.gatewayIP}/ws/api/v1?api_key=${this.apiKey}`;
    this.ws = new WebSocket(url);
    
    this.ws.onopen = () => {
      console.log('Connected to Gateway WebSocket');
      this.startHeartbeat();
    };
    
    this.ws.onmessage = (event) => {
      const message = JSON.parse(event.data);
      this.handleMessage(message);
    };
    
    this.ws.onclose = () => {
      console.log('Gateway WebSocket connection closed');
      this.reconnect();
    };
  }

  sendMessage(type, action, payload) {
    const message = {
      id: `msg_${++this.messageId}`,
      type: type,
      action: action,
      payload: payload,
      timestamp: new Date().toISOString()
    };
    
    this.ws.send(JSON.stringify(message));
    return message.id;
  }

  subscribe(topics, callback) {
    const messageId = this.sendMessage('command', 'subscribe', {
      topics: topics
    });
    
    this.callbacks.set(messageId, callback);
  }
}
```

### Python Example

*[Content to be added: Python WebSocket client example]*

## Best Practices

### Performance Optimization

*[Content to be added: WebSocket performance best practices]*

**Recommendations:**
1. **Selective Subscriptions** - Only subscribe to needed topics
2. **Message Batching** - Batch multiple commands when possible
3. **Filtering** - Use server-side filtering to reduce message volume
4. **Compression** - Enable WebSocket compression for large data

### Security Considerations

*[Content to be added: WebSocket security best practices]*

**Security Guidelines:**
1. **Use WSS** - Always use secure WebSocket connections in production
2. **API Key Management** - Rotate API keys regularly
3. **Input Validation** - Validate all incoming message data
4. **Rate Limiting** - Implement client-side rate limiting

---

*The WebSocket API enables powerful real-time integration capabilities for the Genius Gateway system.*