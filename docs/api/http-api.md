# HTTP API Documentation

## Introduction

### Common Response Codes

- **200 OK** - Request successful
- **400 Bad Request** - Invalid JSON or missing required fields
- **401 Unauthorized** - Authentication required or token invalid
- **403 Forbidden** - Insufficient permissions (admin required)
- **500 Internal Server Error** - Server-side error

### Authorization Levels
- **✅ Public** - No authentication required (public endpoints like `/rest/signIn`)
- **🔒 User** - Requires valid JWT token (most read operations)
- **🛡️ Admin** - Requires valid JWT token with admin privileges (configuration changes, system operations)

### Authentication Header Format

For authenticated endpoints, include JWT token:
```
Authorization: Bearer <token>
```

## Genius Gateway Endpoints

### Overview

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/rest/gateway-devices` | GET, POST | 🛡️ | Manage smoke detector devices |
| `/rest/alarm-lines` | GET, POST | 🛡️ | Manage alarm lines (RF groups) |
| `/rest/alarm-lines/do` | POST | 🛡️ | Execute alarm line actions |
| `/rest/gateway-settings` | GET, POST | 🛡️ | Configure gateway behavior |
| `/rest/mqtt-settings` | GET, POST | 🛡️ | Configure Home Assistant MQTT |
| `/rest/end-alarms` | POST | 🛡️ | End all alarms and block new ones |
| `/rest/end-alarmblocking` | POST | 🛡️ | End alarm blocking period |
| `/rest/cc1101/state` | GET | 🔒 | Get radio transceiver status |
| `/rest/cc1101/rx` | POST | 🛡️ | Force radio into RX state |
| `/rest/wslogger` | GET, POST | 🛡️ | Configure WebSocket logger |
| `/rest/packet-visualizer` | GET, POST | 🛡️ | Configure packet visualizer |

### Device Management

#### `/rest/gateway-devices`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Manage registered smoke detector devices

**GET Response:**
```json
{
  "devices": [
    {
      "id": 1,
      "smokeDetector": {
        "sn": 12345678,
        "productionDate": "2024-01-15T00:00:00Z",
        "model": 0
      },
      "radioModule": {
        "sn": 87654321,
        "productionDate": "2024-01-15T00:00:00Z",
        "model": 0
      },
      "location": "Living Room",
      "isAlarming": false,
      "registration": 1,
      "alarms": [
        {
          "startTime": "2025-01-15T10:30:00Z",
          "endTime": "2025-01-15T10:35:00Z",
          "endingReason": 0
        }
      ]
    }
  ]
}
```

**Device Fields:**

- `id` - Unique device identifier (auto-generated)
- `smokeDetector.sn` - Smoke detector serial number
- `radioModule.sn` - Radio module serial number  
- `location` - Human-readable location string
- `isAlarming` - Current alarm state
- `registration` - How device was added (0=built-in, 1=packet, 2=manual)
- `alarms` - History of alarm events
- `endingReason` - How alarm ended (-1=active, 0=detector, 1=manual)

**POST Request:** Same format as GET response

**POST Response:** 200 OK with updated device list

---

### Alarm Line Management

#### `/rest/alarm-lines`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Manage configured alarm lines (RF communication groups)

**GET Response:**
```json
{
  "lines": [
    {
      "id": 123456789,
      "name": "Building A",
      "created": "2025-01-15T10:00:00Z",
      "acquisition": 1
    }
  ]
}
```

**Alarm Line Fields:**

- `id` - Unique 32-bit alarm line ID (0xFFFFFFFF = broadcast)
- `name` - Human-readable alarm line name
- `created` - Creation timestamp
- `acquisition` - How line was discovered (0=built-in, 1=packet, 2=manual)

**POST Request:** Same format as GET response

**POST Response:** 200 OK with updated alarm line list

---

#### `/rest/alarm-lines/do`
- **Method:** POST
- **Auth:** 🛡️ Admin
- **Description:** Execute alarm line action (line test or fire alarm)
- **Request:**
```json
{
  "action": "linetest",
  "alarmLineId": 123456789,
  "duration": 10
}
```

**Actions:**

- `linetest` - Trigger RF line test transmission
- `firealarm` - Trigger RF fire alarm transmission
- `duration` - Duration in seconds

**Response:**
```json
{
  "success": true
}
```

---

### Gateway Settings

#### `/rest/gateway-settings`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Configure gateway behavior and alerts

**GET Response / POST Request:**
```json
{
  "alert_on_unknown_detectors": true,
  "add_alarm_line_from_commissioning_packet": true,
  "add_alarm_line_from_alarm_packet": false,
  "add_alarm_line_from_line_test_packet": false
}
```

**Settings:**

- `alert_on_unknown_detectors` - Generate alerts for unregistered devices
- `add_alarm_line_from_commissioning_packet` - Auto-add lines from commissioning
- `add_alarm_line_from_alarm_packet` - Auto-add lines from alarm packets
- `add_alarm_line_from_line_test_packet` - Auto-add lines from line tests

**POST Response:** 200 OK with updated settings

---

### Gateway MQTT Settings

#### `/rest/mqtt-settings`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Configure Home Assistant MQTT integration

**GET Response / POST Request:**
```json
{
  "haMQTTEnabled": true,
  "haMQTTTopicPrefix": "homeassistant/binary_sensor/genius-",
  "alarmEnabled": true,
  "alarmTopic": "smarthome/genius-gateway/alarm"
}
```

**Settings:**

- `haMQTTEnabled` - Enable Home Assistant MQTT auto-discovery
- `haMQTTTopicPrefix` - Topic prefix for HA device publishing
- `alarmEnabled` - Enable alarm state publishing
- `alarmTopic` - MQTT topic for global alarm state

**POST Response:** 200 OK with updated settings

---

### Alarm Control

#### `/rest/end-alarms`
- **Method:** POST
- **Auth:** 🛡️ Admin
- **Description:** End all active alarms and optionally block new alarms
- **Request:**
```json
{
  "alarmBlockingTime": 300
}
```
- `alarmBlockingTime` - Seconds to block new alarms (0 = no blocking)

**Response:**
```json
{
  "success": true
}
```

---

#### `/rest/end-alarmblocking`
- **Method:** POST
- **Auth:** 🛡️ Admin
- **Description:** Immediately end alarm blocking period

**Request:** Empty body or `{}`

**Response:**
```json
{
  "success": true
}
```

---

### CC1101 Radio Controller

#### `/rest/cc1101/state`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Get CC1101 radio transceiver status
- **Response:**
```json
{
  "success": true,
  "action": false,
  "state": 1,
  "rxMonitorEnabled": true
}
```

- `action` - True if currently receiving/transmitting (GDO0 high)
- `state` - Radio state (0=IDLE, 1=RX, 2=TX, etc.)
- `rxMonitorEnabled` - RX monitoring enabled flag

---

#### `/rest/cc1101/rx`
- **Method:** POST
- **Auth:** 🛡️ Admin
- **Description:** Force CC1101 into RX (receive) state
- **Response:**
```json
{
  "success": true
}
```

---

### WebSocket Logger

#### `/rest/wslogger`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Configure WebSocket logger settings

**GET Response / POST Request:**
```json
{
  "enabled": true
}
```

**POST Response:** 200 OK with updated settings

---

### Packet Visualizer

#### `/rest/packet-visualizer`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Configure packet visualizer display settings

**GET Response / POST Request:**
```json
{
  "showDetails": true,
  "showMetadata": true
}
```

**POST Response:** 200 OK with updated settings

---

## Framework Endpoints (ESP32 SvelteKit)

### Overview

| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/rest/wifiStatus` | GET | 🔒 | Get WiFi connection status |
| `/rest/wifiSettings` | GET, POST | 🔒 | Get/update WiFi configuration |
| `/rest/scanNetworks` | GET | 🔒 | Trigger WiFi network scan |
| `/rest/listNetworks` | GET | 🔒 | Get network scan results |
| `/rest/apStatus` | GET | 🔒 | Get Access Point status |
| `/rest/apSettings` | GET, POST | 🛡️ | Get/update AP configuration |
| `/rest/ntpStatus` | GET | 🔒 | Get NTP sync status |
| `/rest/ntpSettings` | GET, POST | 🛡️ | Get/update NTP settings |
| `/rest/time` | GET, POST | 🛡️ | Get/set system time |
| `/rest/mqttStatus` | GET | 🔒 | Get MQTT connection status |
| `/rest/mqttSettings` | GET, POST | 🛡️ | Get/update MQTT settings |
| `/rest/systemStatus` | GET | 🔒 | Get system information |
| `/rest/restart` | POST | 🛡️ | Restart device |
| `/rest/sleep` | POST | 🔒 | Put device in deep sleep |
| `/rest/factoryReset` | POST | 🛡️ | Reset to factory defaults |
| `/rest/health` | GET | ✅ | Health check |
| `/rest/features` | GET | 🔒 | Get enabled feature flags |
| `/rest/coreDump` | GET | 🛡️ | Get core dump info |
| `/rest/signIn` | POST | ✅ | Authenticate user |
| `/rest/verifyAuthorization` | GET | 🔒 | Verify JWT token |
| `/rest/securitySettings` | GET, POST | 🛡️ | Get/update security settings |
| `/rest/generateToken` | GET | 🛡️ | Generate JWT secret |
| `/rest/uploadFirmware` | POST | 🛡️ | Upload firmware binary |
| `/rest/downloadUpdate` | POST | 🛡️ | Download firmware update |

---

### WiFi Management

#### `/rest/wifiStatus`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Get current WiFi connection status
- **Response:**
```json
{
  "status": 0-5,
  "local_ip": "192.168.1.100",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "rssi": -45,
  "ssid": "MyNetwork",
  "bssid": "AA:BB:CC:DD:EE:FF",
  "channel": 6,
  "subnet_mask": "255.255.255.0",
  "gateway_ip": "192.168.1.1",
  "dns_ip_1": "8.8.8.8",
  "dns_ip_2": "8.8.4.4"
}
```

---

#### `/rest/wifiSettings`
- **Methods:** GET, POST
- **Auth:** 🔒 User
- **Description:** Get or update WiFi configuration

**GET Response / POST Request:**
```json
{
  "wifi_networks": [
    {
      "ssid": "NetworkName",
      "password": "password123",
      "static_ip_config": false,
      "local_ip": "192.168.1.100",
      "gateway_ip": "192.168.1.1", 
      "subnet_mask": "255.255.255.0",
      "dns_ip_1": "8.8.8.8",
      "dns_ip_2": "8.8.4.4"
    }
  ],
  "hostname": "genius-gateway",
  "connection_mode": 0
}
```

**POST Response:** 200 OK with updated settings

---

#### `/rest/scanNetworks`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Trigger WiFi network scan

**Response:**
```json
{
  "networks": [
    {
      "rssi": -45,
      "ssid": "NetworkName",
      "bssid": "AA:BB:CC:DD:EE:FF",
      "channel": 6,
      "encryption_type": 3
    }
  ]
}
```

---

#### `/rest/listNetworks`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Get results from last network scan

**Response:** Same format as `/rest/scanNetworks`

---

### Access Point Management

#### `/rest/apStatus`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Get Access Point status
- **Response:**
```json
{
  "status": 0-1,
  "ip_address": "192.168.4.1",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "station_num": 2
}
```

---

#### `/rest/apSettings`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Get or update Access Point configuration

**GET Response / POST Request:**
```json
{
  "provision_mode": 0-2,
  "ssid": "Genius-Gateway-AP",
  "password": "password123",
  "channel": 1,
  "ssid_hidden": false,
  "max_clients": 4,
  "local_ip": "192.168.4.1",
  "gateway_ip": "192.168.4.1",
  "subnet_mask": "255.255.255.0"
}
```

**POST Response:** 200 OK with updated settings

---

### NTP (Time) Management

#### `/rest/ntpStatus`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Get NTP synchronization status
- **Response:**
```json
{
  "status": 0-2,
  "time": "2025-01-15T10:30:00Z",
  "uptime": 3600
}
```

---

#### `/rest/ntpSettings`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Get or update NTP settings

**GET Response / POST Request:**
```json
{
  "enabled": true,
  "server": "pool.ntp.org",
  "tz_label": "Europe/Berlin",
  "tz_format": "CET-1CEST,M3.5.0,M10.5.0/3"
}
```

**POST Response:** 200 OK with updated settings

---

#### `/rest/time`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Get or manually set system time

**GET Response / POST Request:**
```json
{
  "time": "2025-01-15T10:30:00Z"
}
```

**POST Response:** 200 OK with updated time

---

### MQTT Management

#### `/rest/mqttStatus`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Get MQTT client connection status
- **Response:**
```json
{
  "enabled": true,
  "connected": true,
  "client_id": "genius-gateway",
  "disconnect_reason": 0
}
```

---

#### `/rest/mqttSettings`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Get or update MQTT client settings

**GET Response / POST Request:**
```json
{
  "enabled": true,
  "host": "192.168.1.10",
  "port": 1883,
  "username": "mqtt_user",
  "password": "mqtt_pass",
  "client_id": "genius-gateway",
  "keep_alive": 60,
  "clean_session": true,
  "max_topic_length": 128,
  "uri": "mqtt://192.168.1.10:1883"
}
```

**POST Response:** 200 OK with updated settings

---

### System Management

#### `/rest/systemStatus`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Get system information
- **Response:**
```json
{
  "esp_platform": "ESP32-S3",
  "max_alloc_heap": 4194304,
  "psram_size": 8388608,
  "free_psram": 6000000,
  "cpu_freq_mhz": 240,
  "free_heap": 200000,
  "sketch_size": 1500000,
  "free_sketch_space": 2500000,
  "flash_chip_size": 4194304,
  "sdk_version": "v4.4.6",
  "core_version": "2.0.14"
}
```

---

#### `/rest/restart`
- **Method:** POST
- **Auth:** 🛡️ Admin
- **Description:** Restart the ESP32 device

**Request:** Empty body or `{}`

**Response:** 200 OK (device restarts immediately)

---

#### `/rest/sleep`
- **Method:** POST
- **Auth:** 🔒 User
- **Description:** Put device into deep sleep mode
- **Request:**
```json
{
  "duration": 3600,
  "wakeup_pin": 0,
  "wakeup_signal": 1
}
```

**Response:** 200 OK (device enters sleep mode)

---

#### `/rest/factoryReset`
- **Method:** POST
- **Auth:** 🛡️ Admin
- **Description:** Reset all settings to factory defaults

**Request:** Empty body or `{}`

**Response:** 200 OK (device resets)

---

#### `/rest/health`
- **Method:** GET
- **Auth:** ✅ Public
- **Description:** Health check endpoint for monitoring

**Response:**
```json
{
  "status": "pass"
}
```

---

#### `/rest/features`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Get enabled feature flags
- **Response:**
```json
{
  "features": {
    "allow_broadcast": true,
    "cc1101_controller": true
  }
}
```

---

#### `/rest/coreDump`
- **Method:** GET
- **Auth:** 🛡️ Admin
- **Description:** Retrieve core dump information from last crash

**Response:** Binary core dump data or JSON with error if no dump available

---

### Security & Authentication

#### `/rest/signIn`
- **Method:** POST
- **Auth:** ✅ Public
- **Description:** Authenticate and receive JWT token
- **Request:**
```json
{
  "username": "admin",
  "password": "admin_password"
}
```
- **Response:**
```json
{
  "access_token": "eyJhbGc...",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

---

#### `/rest/verifyAuthorization`
- **Method:** GET
- **Auth:** 🔒 User
- **Description:** Verify current JWT token is valid

**Response:** 
```json
{
  "valid": true
}
```
or 401 Unauthorized if invalid

---

#### `/rest/securitySettings`
- **Methods:** GET, POST
- **Auth:** 🛡️ Admin
- **Description:** Get or update security settings (users, JWT secret)

**GET Response / POST Request:**
```json
{
  "users": [
    {
      "username": "admin",
      "password": "hashed_password",
      "admin": true
    }
  ],
  "jwt_secret": "random_secret_key"
}
```

**POST Response:** 200 OK with updated settings

---

#### `/rest/generateToken`
- **Method:** GET
- **Auth:** 🛡️ Admin
- **Description:** Generate a new JWT secret token

**Response:**
```json
{
  "token": "newly_generated_token_string"
}
```

---

### Firmware Management

#### `/rest/uploadFirmware`
- **Method:** POST (multipart/form-data)
- **Auth:** 🛡️ Admin
- **Description:** Upload new firmware binary for OTA update

**Request:** Multipart form-data with firmware file in `firmware` field

**Response:**
```json
{
  "success": true
}
```

---

#### `/rest/downloadUpdate`
- **Method:** POST
- **Auth:** 🛡️ Admin
- **Description:** Download and install firmware update from URL

**Request:**
```json
{
  "url": "https://example.com/firmware.bin",
  "md5": "abc123..."
}
```

**Response:**
```json
{
  "success": true
}
```
