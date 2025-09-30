# REST Endpoints

This page lists and describes all available REST API endpoints for programmatic access to the Genius Gateway.

## Overview

The Genius Gateway provides a comprehensive REST API for programmatic access to all system functions. This API enables integration with custom applications, automation systems, and third-party software.

## API Base URL

All API endpoints are relative to the gateway's base URL:

```
http://{gateway-ip}/api/v1/
```

For secure connections (if SSL is enabled):
```
https://{gateway-ip}/api/v1/
```

## Authentication

### API Key Authentication

*[Content to be added: API key authentication system]*

**Authentication Methods:**
1. **Header-based** - `X-API-Key: your-api-key`
2. **Query Parameter** - `?api_key=your-api-key`
3. **Bearer Token** - `Authorization: Bearer your-token`

### API Key Management

*[Content to be added: API key creation and management]*
- Generate API keys through web interface
- Set expiration dates for security
- Revoke compromised keys
- Role-based access control

## System Information Endpoints

### GET /system/info

Get general system information and status.

**Response:**
```json
{
  "status": "success",
  "data": {
    "gateway_id": "ESP32-12345678",
    "firmware_version": "1.2.3",
    "build_date": "2024-01-15T10:30:00Z",
    "uptime": 3600,
    "free_heap": 180000,
    "wifi_status": "connected",
    "rf_status": "active"
  }
}
```

### GET /system/status

Get detailed system status and health information.

**Response:**
```json
{
  "status": "success",
  "data": {
    "system_health": "good",
    "cpu_usage": 25.5,
    "memory_usage": 65.2,
    "rf_quality": 85,
    "devices_online": 12,
    "devices_total": 15,
    "last_communication": "2024-01-15T10:29:45Z"
  }
}
```

### GET /system/statistics

Get communication and performance statistics.

*[Content to be added: Statistics endpoint response format]*

## Device Management Endpoints

### GET /devices

Get list of all discovered smoke detectors.

**Query Parameters:**
- `status` - Filter by device status (online, offline, alert)
- `location` - Filter by assigned location
- `limit` - Limit number of results
- `offset` - Pagination offset

**Response:**
```json
{
  "status": "success",
  "data": {
    "devices": [
      {
        "id": "12345678",
        "name": "Living Room Detector",
        "location": "Living Room",
        "status": "normal",
        "battery_level": 85,
        "signal_strength": -45,
        "last_seen": "2024-01-15T10:29:00Z",
        "online": true
      }
    ],
    "total": 15,
    "online": 12
  }
}
```

### GET /devices/{device_id}

Get detailed information for a specific device.

**Path Parameters:**
- `device_id` - Unique device identifier

**Response:**
```json
{
  "status": "success",
  "data": {
    "id": "12345678",
    "name": "Living Room Detector",
    "location": "Living Room",
    "status": "normal",
    "battery_level": 85,
    "signal_strength": -45,
    "last_seen": "2024-01-15T10:29:00Z",
    "online": true,
    "firmware_version": "2.1.0",
    "device_type": "smoke_detector",
    "installation_date": "2024-01-01T00:00:00Z",
    "test_history": [
      {
        "timestamp": "2024-01-14T09:00:00Z",
        "result": "pass"
      }
    ]
  }
}
```

### PUT /devices/{device_id}

Update device configuration or information.

**Request Body:**
```json
{
  "name": "Updated Device Name",
  "location": "New Location",
  "custom_attributes": {
    "maintenance_schedule": "quarterly"
  }
}
```

### POST /devices/{device_id}/test

Initiate a test sequence for a specific device.

*[Content to be added: Device test endpoint details]*

### DELETE /devices/{device_id}

Remove a device from the system.

*[Content to be added: Device removal endpoint]*

## Device Control Endpoints

### POST /devices/{device_id}/commands

Send control commands to a specific device.

**Request Body:**
```json
{
  "command": "test",
  "parameters": {
    "test_type": "full",
    "timeout": 30
  }
}
```

### GET /devices/{device_id}/history

Get historical data for a device.

**Query Parameters:**
- `start_date` - Start date for historical data
- `end_date` - End date for historical data
- `data_type` - Type of data (status, battery, signal)

*[Content to be added: Historical data response format]*

## Alarm and Alert Endpoints

### GET /alerts

Get current active alerts.

**Response:**
```json
{
  "status": "success",
  "data": {
    "active_alerts": [
      {
        "id": "alert_001",
        "device_id": "12345678",
        "alert_type": "smoke_detected",
        "severity": "high",
        "timestamp": "2024-01-15T10:35:00Z",
        "location": "Living Room",
        "acknowledged": false
      }
    ],
    "total_active": 1
  }
}
```

### POST /alerts/{alert_id}/acknowledge

Acknowledge an active alert.

**Request Body:**
```json
{
  "acknowledged_by": "user_name",
  "notes": "False alarm - cooking smoke"
}
```

### GET /alerts/history

Get historical alert data.

*[Content to be added: Alert history endpoint]*

## Configuration Endpoints

### GET /config

Get current gateway configuration.

*[Content to be added: Configuration retrieval endpoint]*

### PUT /config

Update gateway configuration.

**Request Body:**
```json
{
  "rf": {
    "frequency": 868420000,
    "power_level": 10
  },
  "mqtt": {
    "enabled": true,
    "broker": "192.168.1.100",
    "port": 1883
  },
  "notifications": {
    "email_enabled": true,
    "email_recipients": ["admin@example.com"]
  }
}
```

### GET /config/backup

Export complete configuration as backup.

**Response:**
```json
{
  "status": "success",
  "data": {
    "backup_date": "2024-01-15T10:40:00Z",
    "config_version": "1.0",
    "configuration": {
      // Complete configuration data
    }
  }
}
```

### POST /config/restore

Restore configuration from backup.

*[Content to be added: Configuration restore endpoint]*

## Network and Connectivity Endpoints

### GET /network/wifi

Get Wi-Fi connection information.

*[Content to be added: Wi-Fi status endpoint]*

### GET /network/scan

Scan for available Wi-Fi networks.

*[Content to be added: Wi-Fi scanning endpoint]*

### POST /network/wifi

Configure Wi-Fi connection.

*[Content to be added: Wi-Fi configuration endpoint]*

## Diagnostic Endpoints

### GET /diagnostics

Get comprehensive diagnostic information.

*[Content to be added: Diagnostics endpoint]*

### GET /diagnostics/logs

Get system logs via REST API.

**Query Parameters:**
- `level` - Log level filter (error, warn, info, debug)
- `limit` - Number of log entries to return
- `since` - Return logs since timestamp

### POST /diagnostics/test

Run system diagnostic tests.

*[Content to be added: Diagnostic test endpoint]*

## Firmware Management Endpoints

### GET /firmware/info

Get current firmware information and available updates.

*[Content to be added: Firmware info endpoint]*

### POST /firmware/update

Initiate firmware update process.

*[Content to be added: Firmware update endpoint]*

## Error Handling

### Standard Error Response Format

All API endpoints return errors in a consistent format:

```json
{
  "status": "error",
  "error": {
    "code": "INVALID_DEVICE_ID",
    "message": "The specified device ID does not exist",
    "details": {
      "device_id": "invalid_id",
      "available_devices": ["12345678", "87654321"]
    }
  }
}
```

### HTTP Status Codes

*[Content to be added: Standard HTTP status codes used]*

**Common Status Codes:**
- `200 OK` - Request successful
- `201 Created` - Resource created successfully
- `400 Bad Request` - Invalid request parameters
- `401 Unauthorized` - Authentication required
- `403 Forbidden` - Insufficient permissions
- `404 Not Found` - Resource not found
- `429 Too Many Requests` - Rate limit exceeded
- `500 Internal Server Error` - Server error

## Rate Limiting

### Request Limits

*[Content to be added: API rate limiting information]*
- **Default Limit**: 100 requests per minute per API key
- **Burst Limit**: 20 requests per 10 seconds
- **Rate Limit Headers**: `X-RateLimit-Limit`, `X-RateLimit-Remaining`

## API Versioning

### Version Management

*[Content to be added: API versioning strategy]*
- Current version: v1
- Version specified in URL path
- Backward compatibility policy
- Deprecation notices

---

*This REST API provides comprehensive programmatic access to all Genius Gateway functions for integration and automation purposes.*