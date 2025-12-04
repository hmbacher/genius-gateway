# WebSocket API Documentation

## Introduction

This document describes the WebSocket endpoints for the Genius Gateway. The gateway provides real-time bidirectional communication through WebSocket connections for event streaming and packet logging.

### Common Connection Requirements

- **Protocol:** WebSocket (ws:// or wss://)
- **Authentication:** Via JWT token (see Authorization Levels below)

### Authorization Levels
- **🔒 User** - Requires valid JWT token (event subscriptions, read operations)
- **🛡️ Admin** - Requires valid JWT token with admin privileges (system logs, configuration)

### Authentication

WebSocket connections can be authenticated by including the JWT token in:

1. **Query parameter:** `?access_token=<token>`
2. **WebSocket subprotocol:** Send token in `Sec-WebSocket-Protocol` header

---

## WebSocket Endpoints

### Overview

| Endpoint | Auth | Description |
|----------|------|-------------|
| `/ws/events` | 🔒 | Real-time event stream for system and application events |
| `/ws/logger` | 🛡️ | Real-time RF packet stream from CC1101 radio |

---

## `/ws/events` - Event Stream

### Connection Details

- **URL:** `/ws/events`
- **Auth:** 🔒 User
- **Description:** Subscribe to real-time system and application events. Clients can subscribe to specific event types and receive JSON-formatted event notifications.

### Connection Flow

1. **Connect** - Establish WebSocket connection with authentication
2. **Subscribe** - Send subscription message for desired events
3. **Receive** - Get real-time event notifications as JSON
4. **Unsubscribe** - Optionally unsubscribe from events
5. **Disconnect** - Close connection when done

### Message Format

#### Subscribe to Event
```json
{
  "event": "subscribe",
  "data": {
    "id": "event-name"
  }
}
```

#### Unsubscribe from Event
```json
{
  "event": "unsubscribe",
  "data": {
    "id": "event-name"
  }
}
```

#### Event Notification (Server → Client)
```json
{
  "event": "event-name",
  "data": {
    // Event-specific data
  }
}
```

---

## Framework Events (ESP32 SvelteKit)

### `rssi`
WiFi signal strength updates

**Update Frequency:** Every 500ms when connected

**Data Format:**
```json
{
  "event": "rssi",
  "data": {
    "rssi": -45
  }
}
```

**Fields:**

- `rssi` - Signal strength in dBm (negative integer)

---

### `reconnect`
WiFi reconnection notifications

**Trigger:** WiFi connection attempt

**Data Format:**
```json
{
  "event": "reconnect",
  "data": {}
}
```

---

### `system_health`
System health metrics and resource usage

**Update Frequency:** Every 5 seconds

**Data Format:**
```json
{
  "event": "system_health",
  "data": {
    "free_heap": 200000,
    "max_alloc_heap": 150000,
    "min_free_heap": 180000,
    "psram_size": 8388608,
    "free_psram": 6000000,
    "cpu_temp": 45.2,
    "uptime": 3600
  }
}
```

**Fields:**

- `free_heap` - Current free heap memory (bytes)
- `max_alloc_heap` - Largest allocatable block (bytes)
- `min_free_heap` - Minimum free heap since boot (bytes)
- `psram_size` - Total PSRAM size (bytes)
- `free_psram` - Available PSRAM (bytes)
- `cpu_temp` - CPU temperature (°C, if available)
- `uptime` - System uptime (seconds)

---

### `notification`
System notifications and alerts

**Trigger:** Various system events

**Data Format:**
```json
{
  "event": "notification",
  "data": {
    "type": "info",
    "message": "Notification message",
    "timestamp": "2025-01-15T10:30:00Z"
  }
}
```

**Fields:**

- `type` - Notification type: `info`, `warning`, `error`, `success`
- `message` - Notification text
- `timestamp` - ISO 8601 timestamp

---

### `features`
Feature flag updates

**Trigger:** Feature configuration changes

**Data Format:**
```json
{
  "event": "features",
  "data": {
    "features": {
      "allow_broadcast": true,
      "cc1101_controller": true,
      "battery_monitoring": false
    }
  }
}
```

**Fields:**

- `features` - Object containing feature name/enabled pairs

---

### `otastatus`
OTA firmware update progress

**Trigger:** During firmware download and installation

**Data Format:**
```json
{
  "event": "otastatus",
  "data": {
    "status": "downloading",
    "progress": 45,
    "message": "Downloading firmware..."
  }
}
```

**Fields:**

- `status` - Update status: `idle`, `downloading`, `installing`, `success`, `error`
- `progress` - Progress percentage (0-100)
- `message` - Human-readable status message

---

### `battery`
Battery status updates (if battery monitoring enabled)

**Update Frequency:** Variable based on configuration

**Data Format:**
```json
{
  "event": "battery",
  "data": {
    "voltage": 3.7,
    "percentage": 85,
    "charging": false
  }
}
```

**Fields:**

- `voltage` - Battery voltage (V)
- `percentage` - Battery level (0-100)
- `charging` - Charging status (boolean)

---

### `analytics`
System analytics data (if analytics enabled)

**Update Frequency:** Periodic based on configuration

**Data Format:**
```json
{
  "event": "analytics",
  "data": {
    // Analytics-specific data
  }
}
```

---

## Genius Gateway Events

### `alarm`
Global alarm state notifications

**Trigger:** Any smoke detector enters or exits alarm state

**Data Format:**
```json
{
  "event": "alarm",
  "data": {
    "isAlarming": true,
    "numAlarmingDevices": 2,
    "alarmingDevices": [
      {
        "smokeDetectorSN": 12345678,
        "location": "Living Room",
        "startTime": "2025-01-15T10:30:00Z"
      }
    ]
  }
}
```

**Fields:**

- `isAlarming` - Global alarm state (boolean)
- `numAlarmingDevices` - Number of devices currently alarming
- `alarmingDevices` - Array of alarming device details
  - `smokeDetectorSN` - Smoke detector serial number
  - `location` - Device location string
  - `startTime` - Alarm start timestamp (ISO 8601)

---

### `alarm-state-change`
Individual device alarm state changes

**Trigger:** Specific smoke detector alarm state changes

**Data Format:**
```json
{
  "event": "alarm-state-change",
  "data": {
    "smokeDetectorSN": 12345678,
    "isAlarming": true,
    "location": "Living Room",
    "startTime": "2025-01-15T10:30:00Z"
  }
}
```

**Fields:**

- `smokeDetectorSN` - Smoke detector serial number
- `isAlarming` - Current alarm state (boolean)
- `location` - Device location
- `startTime` - Alarm start time (ISO 8601, if alarming)
- `endTime` - Alarm end time (ISO 8601, if ended)
- `endingReason` - How alarm ended (0=detector, 1=manual)

---

### `genius-device-added-from-packet`
New smoke detector auto-discovered

**Trigger:** New device detected from RF packet

**Data Format:**
```json
{
  "event": "genius-device-added-from-packet",
  "data": {
    "smokeDetectorSN": 12345678,
    "radioModuleSN": 87654321,
    "location": "Unknown location"
  }
}
```

**Fields:**

- `smokeDetectorSN` - Smoke detector serial number
- `radioModuleSN` - Radio module serial number
- `location` - Initial location (typically "Unknown location")

---

### `new-alarm-line`
New alarm line discovered

**Trigger:** New alarm line detected from RF communication

**Data Format:**
```json
{
  "event": "new-alarm-line",
  "data": {
    "id": 123456789,
    "name": "Alarm Line 123456789",
    "created": "2025-01-15T10:30:00Z",
    "acquisition": 1
  }
}
```

**Fields:**

- `id` - Unique 32-bit alarm line ID
- `name` - Auto-generated or user-defined name
- `created` - Discovery timestamp (ISO 8601)
- `acquisition` - How line was added (0=built-in, 1=packet, 2=manual)

---

### `alarm-line-action-finished`
Alarm line action completion

**Trigger:** Line test or fire alarm transmission completed

**Data Format:**
```json
{
  "event": "alarm-line-action-finished",
  "data": {
    "alarmLineId": 123456789,
    "action": "linetest",
    "success": true,
    "transmissionTime": 10.5
  }
}
```

**Fields:**

- `alarmLineId` - Alarm line ID
- `action` - Action type: `linetest`, `firealarm`
- `success` - Action completion status (boolean)
- `transmissionTime` - Duration in seconds

---

### `rem-alarm-block-time`
Remaining alarm blocking time updates

**Trigger:** Alarm blocking active, updates every second

**Data Format:**
```json
{
  "event": "rem-alarm-block-time",
  "data": {
    "isBlocked": true,
    "remainingBlockingTimeS": 285
  }
}
```

**Fields:**

- `isBlocked` - Current blocking state (boolean)
- `remainingBlockingTimeS` - Seconds remaining until blocking ends

---

### Error Handling

**Connection Errors:**
- **401 Unauthorized** - Invalid or missing JWT token
- **403 Forbidden** - Insufficient user privileges

**Event Subscription Errors:**
- Invalid event names are silently ignored
- Invalid subscription message format is silently ignored
- Subscribe only to valid event names listed in the Framework Events and Genius Gateway Events sections

---

### Best Practices

1. **Selective Subscription** - Subscribe only to events you need to minimize bandwidth usage
2. **High-Frequency Events** - Be prepared to handle frequent updates from `rssi` (every 500ms) and `system_health` (every 5s)
3. **Reconnection Strategy** - Implement automatic reconnection with exponential backoff (start at 1s, max 30s)
4. **Resubscribe on Reconnect** - Store subscriptions and resubscribe after reconnection
5. **Message Buffering** - Consider buffering rapid events like `rssi` for display updates
6. **Resource Cleanup** - Unsubscribe from events before closing connection
7. **Error Recovery** - Handle JSON parse errors gracefully in case of malformed messages

---

### Connection Examples

#### JavaScript (Browser)
```javascript
const eventsWs = new WebSocket('ws://gateway.local/ws/events?access_token=' + token);

eventsWs.onopen = () => {
  console.log('Connected to events stream');
  
  // Subscribe to multiple events
  ['alarm', 'alarm-state-change', 'system_health'].forEach(eventName => {
    eventsWs.send(JSON.stringify({
      event: 'subscribe',
      data: { id: eventName }
    }));
  });
};

eventsWs.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  console.log('Event:', msg.event, 'Data:', msg.data);
  
  // Handle specific events
  switch(msg.event) {
    case 'alarm':
      handleAlarm(msg.data);
      break;
    case 'system_health':
      updateHealthDisplay(msg.data);
      break;
  }
};

eventsWs.onerror = (error) => {
  console.error('WebSocket error:', error);
};

eventsWs.onclose = () => {
  console.log('Disconnected, reconnecting...');
  setTimeout(() => connectEvents(), 5000);
};
```

#### Python
```python
import websocket
import json
import time

class EventsClient:
    def __init__(self, token):
        self.token = token
        self.subscriptions = ['alarm', 'alarm-state-change', 'system_health']
        self.ws = None
        
    def on_open(self, ws):
        print('Connected to events stream')
        # Subscribe to events
        for event_name in self.subscriptions:
            ws.send(json.dumps({
                'event': 'subscribe',
                'data': {'id': event_name}
            }))
    
    def on_message(self, ws, message):
        msg = json.loads(message)
        print(f"Event: {msg['event']}, Data: {msg['data']}")
        
        # Handle specific events
        if msg['event'] == 'alarm':
            self.handle_alarm(msg['data'])
        elif msg['event'] == 'system_health':
            self.update_health(msg['data'])
    
    def on_error(self, ws, error):
        print(f'Error: {error}')
    
    def on_close(self, ws, close_status_code, close_msg):
        print('Disconnected, reconnecting...')
        time.sleep(5)
        self.connect()
    
    def connect(self):
        self.ws = websocket.WebSocketApp(
            f"ws://gateway.local/ws/events?access_token={self.token}",
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close
        )
        self.ws.run_forever()
    
    def handle_alarm(self, data):
        if data.get('isAlarming'):
            print(f"ALARM! {data['numAlarmingDevices']} device(s) alarming")
    
    def update_health(self, data):
        print(f"Free heap: {data['free_heap']} bytes")

# Usage
client = EventsClient(token='your_jwt_token')
client.connect()
```

#### Node.js
```javascript
const WebSocket = require('ws');

class EventsClient {
  constructor(token) {
    this.token = token;
    this.subscriptions = ['alarm', 'alarm-state-change', 'system_health'];
    this.reconnectDelay = 1000;
    this.maxReconnectDelay = 30000;
  }
  
  connect() {
    this.ws = new WebSocket(`ws://gateway.local/ws/events?access_token=${this.token}`);
    
    this.ws.on('open', () => {
      console.log('Connected to events stream');
      this.reconnectDelay = 1000; // Reset backoff
      
      // Subscribe to events
      this.subscriptions.forEach(eventName => {
        this.ws.send(JSON.stringify({
          event: 'subscribe',
          data: { id: eventName }
        }));
      });
    });
    
    this.ws.on('message', (data) => {
      const msg = JSON.parse(data);
      console.log('Event:', msg.event, 'Data:', msg.data);
    });
    
    this.ws.on('close', () => {
      console.log(`Disconnected, reconnecting in ${this.reconnectDelay}ms...`);
      setTimeout(() => this.connect(), this.reconnectDelay);
      this.reconnectDelay = Math.min(this.reconnectDelay * 2, this.maxReconnectDelay);
    });
    
    this.ws.on('error', (error) => {
      console.error('WebSocket error:', error);
    });
  }
}

const client = new EventsClient('your_jwt_token');
client.connect();
```

---

## `/ws/logger` - Packet Stream

### Connection Details

- **URL:** `/ws/logger`
- **Auth:** 🛡️ Admin
- **Description:** Real-time binary stream of RF packets received by the CC1101 radio transceiver. Provides raw packet data for debugging, analysis, and visualization.

### Connection Flow

1. **Connect** - Establish WebSocket connection with admin authentication
2. **Receive ID** - Server sends unique client ID
3. **Stream** - Receive binary packet data continuously
4. **Disconnect** - Close connection when done

### Message Format

#### Client ID (Server → Client, on connect)
```json
{
  "clientId": "wslogger:12345"
}
```

#### Packet Data (Server → Client, binary)

**Format:** Binary data structure (`cc1101_packet_t`)

**Structure:**
```c
struct cc1101_packet {
    uint64_t timestamp;           // Packet reception timestamp (µs since boot)
    uint8_t buffer[64];           // Raw CC1101 RX FIFO content
    uint8_t* data;                // Pointer to actual packet data
    size_t length;                // Packet data length (bytes)
}
```

**Buffer Contents:**

- First byte: Packet length
- Next N (Packt length) bytes: Packet data
- Last 2 bytes: Status (RSSI, LQI / CRC)

**Binary Stream Details:**

- **Type:** `HTTPD_WS_TYPE_BINARY`
- **Size:** 80 bytes per packet (fixed structure size)
- **Frequency:** Real-time as packets are received

For further details, also see [WebSocket Logging Interface](../features/websocket-interface.md).

### Usage Notes

- Only available when WebSocket logger is enabled (via `/rest/wslogger` or web frontend)
- Connection filter checks admin authentication
- Multiple clients can connect simultaneously
- Each client receives all packets (broadcast)
- Binary format requires client-side parsing
- Timestamp is in microseconds since device boot

---

### Error Handling

**Connection Errors:**
- **401 Unauthorized** - Invalid or missing JWT token
- **403 Forbidden** - Insufficient admin privileges
- **503 Service Unavailable** - WebSocket logger disabled via `/rest/wslogger` settings

**Stream Errors:**
- Connection drops if logger is disabled during active session
- Binary data parsing errors if client expects wrong packet format
- Verify logger is enabled before connecting: `GET /rest/wslogger`

---

### Best Practices

1. **Check Logger Status** - Verify logger is enabled via `/rest/wslogger` before connecting
2. **Binary Type Setting** - Always set `ws.binaryType = 'arraybuffer'` or `'blob'` for proper binary handling
3. **Efficient Parsing** - Use DataView for efficient binary data access
4. **High Data Rate** - Prepare for high packet rates during active RF communication
5. **Buffer Management** - Implement packet buffering if processing is slower than arrival rate
6. **Reconnection Strategy** - Implement exponential backoff reconnection (start at 1s, max 30s)
7. **Resource Management** - Close connection when not actively monitoring to reduce server load
8. **Timestamp Handling** - Timestamps are microseconds since boot, convert to absolute time if needed

---

### Connection Examples

#### JavaScript (Browser)
```javascript
const loggerWs = new WebSocket('ws://gateway.local/ws/logger?access_token=' + token);
loggerWs.binaryType = 'arraybuffer';

loggerWs.onopen = () => {
  console.log('Connected to packet logger');
};

loggerWs.onmessage = (event) => {
  if (typeof event.data === 'string') {
    // Client ID message
    const msg = JSON.parse(event.data);
    console.log('Client ID:', msg.clientId);
  } else {
    // Binary packet data (cc1101_packet_t structure)
    const view = new DataView(event.data);
    
    // Parse structure fields
    const timestamp = view.getBigUint64(0, true); // Little endian
    
    // Buffer starts at offset 8, first byte is length
    const length = view.getUint8(8);
    
    // Extract packet data (starts at buffer[1])
    const packetData = new Uint8Array(event.data, 9, length);
    
    // Last 2 bytes of buffer are RSSI and LQI
    const rssi = view.getInt8(8 + length + 1);
    const lqi = view.getUint8(8 + length + 2);
    
    console.log('Packet:', {
      timestamp: timestamp,
      length: length,
      data: Array.from(packetData).map(b => b.toString(16).padStart(2, '0')).join(' '),
      rssi: rssi,
      lqi: lqi & 0x7F,
      crc_ok: (lqi & 0x80) !== 0
    });
  }
};

loggerWs.onerror = (error) => {
  console.error('WebSocket error:', error);
};

loggerWs.onclose = () => {
  console.log('Logger disconnected');
};
```

#### Python
```python
import websocket
import struct
import json

class PacketLoggerClient:
    def __init__(self, token):
        self.token = token
        self.ws = None
    
    def on_open(self, ws):
        print('Connected to packet logger')
    
    def on_message(self, ws, message):
        if isinstance(message, str):
            # Client ID message
            msg = json.loads(message)
            print(f"Client ID: {msg['clientId']}")
        else:
            # Binary packet data
            # Parse cc1101_packet_t structure
            timestamp = struct.unpack('<Q', message[0:8])[0]  # uint64_t little endian
            
            # Buffer starts at offset 8
            length = message[8]  # First byte is length
            packet_data = message[9:9+length]
            
            # RSSI and LQI are last 2 bytes
            rssi = struct.unpack('b', bytes([message[8 + length + 1]]))[0]  # signed
            lqi_byte = message[8 + length + 2]
            lqi = lqi_byte & 0x7F
            crc_ok = (lqi_byte & 0x80) != 0
            
            print(f"Packet: timestamp={timestamp}µs, len={length}, "
                  f"data={packet_data.hex()}, rssi={rssi}dBm, "
                  f"lqi={lqi}, crc={crc_ok}")
    
    def on_error(self, ws, error):
        print(f'Error: {error}')
    
    def on_close(self, ws, close_status_code, close_msg):
        print('Logger disconnected')
    
    def connect(self):
        self.ws = websocket.WebSocketApp(
            f"ws://gateway.local/ws/logger?access_token={self.token}",
            on_open=self.on_open,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close
        )
        self.ws.run_forever()

# Usage
client = PacketLoggerClient(token='your_jwt_token')
client.connect()
```

#### Node.js
```javascript
const WebSocket = require('ws');

class PacketLoggerClient {
  constructor(token) {
    this.token = token;
  }
  
  connect() {
    this.ws = new WebSocket(`ws://gateway.local/ws/logger?access_token=${this.token}`);
    this.ws.binaryType = 'arraybuffer';
    
    this.ws.on('open', () => {
      console.log('Connected to packet logger');
    });
    
    this.ws.on('message', (data) => {
      if (typeof data === 'string') {
        // Client ID message
        const msg = JSON.parse(data);
        console.log('Client ID:', msg.clientId);
      } else {
        // Binary packet data
        const buffer = Buffer.from(data);
        
        // Parse cc1101_packet_t structure
        const timestamp = buffer.readBigUInt64LE(0);
        const length = buffer.readUInt8(8);
        const packetData = buffer.slice(9, 9 + length);
        const rssi = buffer.readInt8(8 + length + 1);
        const lqiByte = buffer.readUInt8(8 + length + 2);
        const lqi = lqiByte & 0x7F;
        const crcOk = (lqiByte & 0x80) !== 0;
        
        console.log('Packet:', {
          timestamp: timestamp.toString(),
          length: length,
          data: packetData.toString('hex'),
          rssi: rssi,
          lqi: lqi,
          crc_ok: crcOk
        });
      }
    });
    
    this.ws.on('error', (error) => {
      console.error('WebSocket error:', error);
    });
    
    this.ws.on('close', () => {
      console.log('Logger disconnected');
    });
  }
}

const client = new PacketLoggerClient('your_jwt_token');
client.connect();
```
