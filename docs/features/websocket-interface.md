---
icon: tabler/brackets-angle
---

# WebSocket Logging Interface

The WebSocket Logger provides real-time streaming of RF packets from the CC1101 radio to connected clients via WebSocket protocol. This enables external tools and the built-in Packet Visualizer to receive and analyze packets as they are captured.

!!! info "Access Required"
    WebSocket Logger settings require **administrator** privileges to view and modify.

## Overview

The WebSocket Logger acts as a bridge between the gateway's RF packet reception and client applications that need real-time packet data. When enabled, all received packets are immediately streamed to connected WebSocket clients, providing live monitoring and analysis capabilities.

![WebSocket Logger with two connected clients](../assets/images/software/gg-gateway-websocket-logger.png)

## :tabler-plug-connected: Status and Connected Clients {#connected-clients}

The upper part of the WebSocket Logger card shows the state of the logger, the settings follow below. The status is updated every two seconds:

- **Status** - *Active*, or *Disabled* when the logger refuses connections
- **URL** - the address logging tools connect to (only while active)
- **One entry per connected client** - logging tools as well as open [Packet Visualizers](packet-visualizer.md), in this browser or elsewhere (only while active):
    - **Name** - *Packet Visualizer* for the built-in one, the name a tool gave itself (see [Naming a client](#naming-a-client)), otherwise the browser or tool from its User-Agent (e.g. *Chrome on Windows*, *Python/3.13 websockets/13.1*)
    - **Address** - IP address and port of the connection, followed by the browser or tool if the client has a name
    - **Connected for** - how long the connection has been open; the tooltip shows since when
    - **Packets and data** - radio packets sent to the client since it connected, and their data (80 bytes per packet). Every repetition of a packet is counted, as the logger forwards each one

The counters live in the gateway's memory and start at zero with every connection. Without a connected client the entry says **No client connected**:

![WebSocket Logger without a connected client](../assets/images/software/gg-gateway-websocket-logger-no-clients.png)

## :tabler-adjustments: Settings

### Enable WebSocket Logger

**Default:** Enabled

Controls whether the WebSocket logging service is active and accepting client connections.

**When enabled:**

- The WebSocket server accepts connections at `ws://[gateway-ip]/ws/logger`
- All received RF packets are streamed in real-time to connected clients
- The [Packet Visualizer](packet-visualizer.md) can display live packet data
- External tools can connect to receive raw packet streams
- The status shows the WebSocket connection URL and the [connected clients](#connected-clients)

**When disabled:**

- The WebSocket server rejects all connection attempts
- Clients that are still connected are disconnected when the setting is saved
- No packet data is transmitted over WebSocket
- The [Packet Visualizer](packet-visualizer.md) cannot receive live packets (file import still works)
- Reduces system load when real-time monitoring is not needed

!!! warning "No login"
    The logger stream needs no login: while the logger is enabled, any device in the network can connect and read the received radio packets. Switch it off when it is not needed.

!!! warning "Performance Impact"
    Packets are sent to connected clients **synchronously** during packet reception. A slow network connection or multiple connected clients can significantly impact packet processing performance and may lead to packet loss. For best results, use a fast local network connection and limit the number of simultaneous clients. See [Performance Considerations](#performance-considerations) for details.

## WebSocket Connection

When the logger is enabled, clients can connect using:

```
ws://[gateway-ip-address]/ws/logger
```

### Connection Details

- **Protocol:** WebSocket (RFC 6455)
- **Data Format:** Binary (raw packet data structure)
- **Authentication:** none - any device in the network can connect while the logger is enabled
- **Path:** `/ws/logger`

### Naming a client {#naming-a-client}

A client can give itself a name with the query parameter `client`, for example `ws://[gateway-ip-address]/ws/logger?client=my-recorder`. The name is shown in the list of [connected clients](#connected-clients) (up to 32 characters; letters, digits, spaces and punctuation). The built-in Packet Visualizer connects as `packet-visualizer`.

### Data Structure

Each WebSocket message contains the complete `cc1101_packet_t` structure (80 bytes total):

| Offset | Size<br>(incl. padding) | Type | Description |
|--------|------|------|-------------|
| 0 | 8 bytes | uint64_t | Packet reception timestamp (microseconds) |
| 8 | 64 bytes | uint8_t[64] | *RX FIFO buffer*<br>*Byte 8+ 0*: Packet length byte from CC1101 (number of bytes following, including status bytes)<br>*Byte 8+ 1 to byte 8+ length*: Actual packet data, see [Genius Plus X Packets](../reverse-engineering/protocol-analysis.md#genius-plus-x-packets)<br>*Byte 8+ length+1*: RSSI<br>*Byte 8+ length+2*: LQI/CRC   |
| 72 | 4 bytes | uint8_t* | Pointer to packet data within buffer (client-side: ignore/treat as reserved) |
| 76 | 4 bytes | size_t | Packet data length (number of data bytes, excluding status bytes) |

## Use Cases

### Built-in Packet Visualizer

The [Packet Visualizer](packet-visualizer.md) uses the WebSocket Logger internally to display live packet data with interpretation and highlighting. This is the primary use case for most users.

### External Analysis Tools

Developers and researchers can create custom tools that connect to the WebSocket to:

- Capture packet streams for offline analysis
- Implement custom packet filtering or alerting
- Integrate with other monitoring systems
- Perform protocol research and reverse engineering

### API Integration

For detailed information about WebSocket API usage and integration, see [WebSocket API](../api/websocket-api.md#wslogger-packet-stream).

## Performance Considerations

The WebSocket Logger is designed for diagnostic and development purposes. Consider the following when enabling it:

1. **Packet Processing Overhead:** Each received packet triggers a synchronous WebSocket send operation to all connected clients
2. **Network Latency:** Slow clients can block packet processing, causing the RX FIFO to overflow
3. **Multiple Clients:** Each additional client multiplies the synchronous send overhead
4. **Continuous Operation:** Consider disabling the logger when real-time monitoring is not needed

!!! tip "Optimal Usage"
    Enable the WebSocket Logger only when actively using the Packet Visualizer or external monitoring tools. Disable it during normal gateway operation to ensure optimal packet reception performance.

## Related Documentation

- [Packet Visualizer](packet-visualizer.md) - Built-in tool for real-time packet analysis
- [MQTT Topics](../api/mqtt-topics.md) - MQTT integration and message formats
- [Protocol Analysis](../reverse-engineering/protocol-analysis.md) - Packet structure and field definitions
- [Gateway Settings](gateway-settings.md) - Configure packet processing behavior