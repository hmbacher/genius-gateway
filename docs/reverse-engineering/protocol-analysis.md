# Protocol Analysis

Deep dive into the Hekatron Genius Plus X communication protocol structure and behavior.

## CC1101 Packet Mode Structure

When the CC1101 operates in packet mode, all CC1101 packets are structured as follows. The packet format is determined by the configuration settings analyzed in the [FM Basis X - RF](fm-basis-x-rf.md) section.

### Basic Packet Structure

Based on the CC1101 configuration, each packet contains the following elements:

| Field | Byte Offset | Size | Content | Description | Configuration Source |
|-------|-------------|------|---------|-------------|---------------------|
| **Preamble** | 0-3 | 4 bytes | 0xAAAAAAAA | Synchronization sequence for clock recovery | MDMCFG1[6:4] = 010 |
| **Sync Word** | 4-5 | 2 bytes | 0xD391 | Packet detection pattern | SYNC1=0xD3, SYNC0=0x91 |
| **Length** | 6 | 1 byte | 0x01-0x3D | Payload length (N bytes, max 59)[^1] | Variable length mode (PKTCTRL0[1:0]=01) |
| **Payload** | 7 to N+6 | 1-59[^1] bytes | Data (whitened) | Application data with whitening applied. This section contains the [Genius Plus X Packets](#genius-plus-x-packets) | Data whitening ON (PKTCTRL0[6]=1) |
| **CRC** | N+7 to N+8 | 2 bytes | CRC-16 | Error detection checksum over length + payload | CRC enabled (PKTCTRL0[2]=1) |
| **Status** | N+9 to N+10 | 2 bytes | RSSI + LQI | Signal quality metrics appended by receiver | Status append (PKTCTRL1[2]=1) |

*Where N = payload length (1-59 bytes in practice)*

#### Packet Fragmentation

The Genius Plus X system does **not utilize packet fragmentation**. All communication fits within the 59-byte payload limit of a single physical packet, keeping the protocol simple and energy-efficient.

!!! tip "Packet limit and fragmentation"
    While the CC1101 supports variable packet lengths up to 255 bytes in theory, the physical packet size is limited to **64 bytes total**, including length, payload, CRC, and status bytes (but excluding preamble and sync word).

    **Max. application payload =** 64 bytes (max. physical payload) - 1 byte (length) - 2 bytes (CRC) - 2 bytes (status) = **59 bytes**

### Packet Timing

The CC1101 transmits at 38.383 kBaud with GFSK modulation:

- **Bit duration**: 26.045 μs per bit
- **Byte duration**: 208.36 μs per byte  
- **Minimum physical packet**: ~2.50 ms (12 bytes: 4+2+1+1+2+2 = preamble + sync + length + 1 byte payload + CRC + status)
- **Maximum physical packet**: ~14.58 ms (70 bytes: 4+2+1+59+2+2 = preamble + sync + length + 59 bytes payload + CRC + status)
- **Typical Genius Plus packet**: ~3-6 ms (15-30 total bytes including all overhead)

### Data Encoding

The payload undergoes several processing steps:

1. **Data Whitening**: Applied to payload (not preamble/sync) for DC balance
2. **CRC Calculation**: 16-bit CRC-CCITT over length + payload
3. **GFSK Modulation**: ±47.6 kHz deviation around 868.035 MHz carrier

## Genius Plus X Packets

Using the parameters described in [FM Basis X - RF](fm-basis-x-rf.md), it is possible to build a receiver that captures and stores packets from FM Basis X modules.

The packets documented below were received during specific functional scenarios and can be categorized accordingly.

!!! note "No Encryption"
    The Genius Plus communication appears to be unencrypted. This can be observed from the serial numbers that serve as identifiers within the packets and for packet routing.

    Although not all bytes of the packet payload have been identified and understood, the evidence suggests that communication is fundamentally unencrypted, which aligns with the goal of energy-efficient communication that minimizes processing overhead.

### Base Packet Structure

The following table shows the 27 bytes large part of a packet that was recognizable in all examined packets and appears to be part of all packets. Each packet begins with this common part, and depending on the packet type, additional information may be appended.

#### Compact view

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | XX |
Field            | Pkt-# |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                   |
```

#### Detailed explanation

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| 0 | 1 | `0x02` | | Unknown, seems to be constant |
| 1-2 | 2 | `CC18 - 0000`<br>(`6.348 - 0`)<br>*little endian* | Pkt-# | A *packet repetition counter* that decreases with each packet repetition. It is not entirely clear how the decrementation occurs (see [Repetition](#repetition) for details). |
| 3 | 1 | `0x00` | | Unknown, seems to be constant |
| 4 | 1 | `0xFF` | | Unknown, seems to be constant |
| 5 | 1 | `0xFF` | | Unknown, seems to be constant |
| 6 | 1 | `0xFF` | | Unknown, seems to be constant |
| 7 | 1 | `0xFF` | | Unknown, seems to be constant |
| 8 | 1 | `0x00` | | Unknown, seems to be constant |
| 9-12 | 4 | `XX XX XX XX` | Org-SN| Serial number[^2] of the FM Basis X radio module that originally sent this packet<br><br>It is assumed that this field does not change when the packet is forwarded, but this could not be confirmed as no multi-hop routing scenarios were observed. |
| 13 | 1 | `0x00` | | Unknown, seems to be constant |
| 14-17 | 4 | `XX XX XX XX`<br>*big endian* | Fwd-SN | Serial number[^2] of the FM Basis X radio module *forwarding* this packet.<br>The *first* packet has identical Org-SN and Fwd-SN fields. If other FM Basis X radio modules forward the packet (acting as routers), they insert their own serial number.
| 18-21 | 4 | `XX XX XX XX`<br>*big endian* | Line-ID | The alarm line ID that this packet should affect.<br>Special IDs:<br>- `00 00 00 00 (0)`: The radio module is not (yet) assigned to any alarm line.<br>- `FF FF FF FF` (4294967295): Broadcast (all alarm lines) |
| 22 | 1 | `0x0F - 0x00`<br>(`15 - 0`) | Hops | This appears to be a hop counter for forwarded packets, tracking the number of times the packet has been retransmitted. The original packet always contains `0x0F`, which is reduced to `0x0E` after the first forwarding. This suggests that the field is decremented by one with each subsequent forwarding. |
| 23 | 1 | `XX` | | Unknown |
| 24 | 1 | `XX` | | Unknown |
| 25 | 1 | `0x00` | | Unknown, seems to be constant |
| 26 | 1 | `XX` | | Unknown |

</div>

### Repetition

All packets are transmitted multiple times by the radio module. The packets are sent with a period of approximately 10 ms. The repeated packets are identical except for the `Pkt-#` field (see [Base Packet Structure](#base-packet-structure)). The `Pkt-#` field starts with an individual initial value in the first packet, which is decremented with each repetition.

The number of repetitions, the exact period for packet repetitions, as well as the initial value and decrementation slightly differ depending on the packet type:

| Packet Type | Repetitions<br>$N$ | Period Time<br>$T$ | Initial *Pkt-#*<br>$PC_{Start}$ | *Pkt-#* Decrement<br>$\Delta=\frac{PC_{Start}}{N - 1}$ |
|:------------|------------:|-------:|----------------:|:---------:|
| [Alarm Line Commissioning](#alarm-line-commissioning) | 309 x | ~10.06 ms | 6.348 (CC 18) | ~20,6 |
| [Alarming (Start/Stop)](#alarming-startstop) | 315 x | ~9.85 ms | 6.348 (CC 18) | ~20,2 |
| [Line Test (Start/Stop)](#line-test-startstop) | 370 x | ~8,40 ms | 6.348 (CC 18) | ~17,2 |
| [Discovery Request](#discovery-request) | 26 x | ~8,19 ms | 427 (AB 01) | ~17,1 |
| [Discovery Response](#discovery-response) | 24 x| ~9,02 ms | 427 (AB 01) | ~18,6 |

!!! note "Pkt-# decrement"
    It can be observed that the decrement is not constant across packet repetitions but exhibits a certain amount of jitter. This is likely caused by a higher-resolution underlying counter whose value may vary slightly with each iteration and is then rounded and stored as an unsigned integer in the `Pkt-#` field.

    Furthermore, it is notable that the average decrement approximately equals twice the period time $T$, which corresponds to the count value of a 2 kHz clock. The initial value $PC_{Start}$ thereby corresponds to twice the duration of the entire transmission process (across all repetitions) in milliseconds.

### Specific Packets

The following section describes the structure of specific packets that were received and analyzed in connection with various Genius Plus X functions.

#### Alarm Line Commissioning

The packet described here occurs when functions for commissioning or establishing an alarm line are used, such as initial commissioning, when devices are subsequently added to the alarm line, and possibly other scenarios.

##### Observations

When commissioning is initiated via the radio module of a smoke detector that is already part of an alarm line, the stored alarm line ID is used in both the `Line-ID` and `New Line-ID` fields. This method allows new smoke detectors (i.e., corresponding radio modules that have not yet stored an alarm line ID) to be added to the existing alarm line. For this purpose, commissioning packets are responded to by all radio modules *without* an assigned alarm line ID, as well as *only* by smoke detectors (i.e. their radio modules) that have the same alarm line ID stored (i.e., are part of the initiating alarm line).

When commissioning is initiated via a new radio module (i.e., not yet assigned to an alarm line or assignment deleted), the `Line-ID` field contains the value `00 00 00 00` and a generated alarm line ID is entered in the `New Line-ID` field. All radio modules not yet assigned (i.e., radio modules without a stored alarm line ID) signal their readiness for commissioning and, upon confirmation, store the alarm line ID proposed in `New Line-ID`.

The principle by which new alarm line IDs are generated is not fully understood.

##### Basic properties

- Overall packet length: **37 bytes**
- Repititions: **309** 
- Forwarded within network: **yes**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |    28-31    | 32 | 33 | 34 | 35 | 36 |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | XX | 03 | XX XX XX XX | XX | XX | XX | 00 | 00 |
Field            | Pkt-# |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                        | New Line-ID | Ho | Mi | Se |         |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x03` | | Unknown, seems to be constant |
| 28-31 | 4 | `XX XX XX XX`<br>*big endian* | New Line-ID | The alarm line ID to be assigned during commissioning.<br><ul><li>When initiated by an already commissioned device: Contains the existing alarm line ID (same as `Line-ID`).</li><li>When initiated by a new device: Contains a newly generated alarm line ID (while `Line-ID` is `00 00 00 00`).</li></ul>See [Observations](#observations) for more details. |
| 32 | 1 | `XX` | Ho(urs) | Hour of the smoke detector's current time in format `0-23`h |
| 33 | 1 | `XX` | Mi(nutes) | Minute of the smoke detector's current time in format `0-59`min |
| 34 | 1 | `XX` | Se(conds) | Second of the smoke detector's current time in format `0-59`s |
| 35 | 1 | `0x00` | | Unknown, seems to be constant |
| 36 | 1 | `0x00` | | Unknown, seems to be constant |

</div>

#### Alarming (Start/Stop)

The packets described here serve to signal the detection of smoke by an individual smoke detector to all other smoke detectors of an alarm line, so that they also signal the detected smoke. The exact signaling behavior of the smoke detectors can be found in the operating instructions.

##### Observations

- Each smoke detector whose smoke sensor detects smoke sends a Start Alarming packet:
    - The `Org-SN` and `Fwd-SN` fields contain the serial number of the **radio module** of the detecting smoke detector
    - The `Start` field contains the value `0x01`
    - The `Stop` field contains the value `0x00`
    - The `Detector` field contains the serial number of the detecting **smoke detector** (not its radio module!)

- Each smoke detector that receives a Start Alarming packet signals the smoke itself (acoustically and optically) and forwards the Start Alarming packet as follows:
    - The serial number of the forwarding radio module is entered in the `Fwd-SN` field
    - The value of the `Hops` field is decremented by `1`

- If the smoke sensor of a detecting smoke detector no longer detects smoke, it sends a Stop Alarming packet as follows:
    - The `Org-SN` and `Fwd-SN` fields contain the serial number of the radio module of the no-longer-detecting smoke detector
    - The `Stop` field contains the value `0x01`
    - The `Start` field contains the value `0x00`
    - The `Detector` field contains the serial number of the *no-longer-detecting* smoke detector (not its radio module!)

- Each smoke detector that receives a Stop Alarming packet stops its own signaling and forwards the Stop Alarming packet as follows:
    - The serial number of the forwarding radio module is entered in the `Fwd-SN` field
    - The value of the `Hops` field is decremented by `1`

        !!! question
            It is not known whether non-detecting but signaling smoke detectors individually store all detecting smoke detectors whose Start Alarming packets they have already received and forwarded, and only stop their own signaling when a Stop Alarming packet has been received from *all* detecting smoke detectors.

- If the test button is pressed on a *signaling* but non-detecting smoke detector, the radio module generates a Stop Alarming packet that silences *all non-detecting* smoke detectors:
    - The `Org-SN` and `Fwd-SN` fields contain the serial number of the radio module of the smoke detector on which the test button was pressed
    - The `Stop` field contains the value `0x01`
    - The `Start` field contains the value `0x00`
    - The `Detector` field contains the serial number of the smoke detector (not its radio module!) on which the test button was pressed
        
        !!! tip
            Because `Detector` does not contain the serial number of a *detecting* smoke detector, another radio module receiving the Stop Alarming packet could evaluate whether it originates from a detecting or "merely" signaling smoke detector.

- If the test button is pressed on a *detecting* smoke detector, the radio module generates a Stop Alarming packet that silences *all* smoke detectors:
    - The `Org-SN` and `Fwd-SN` fields contain the serial number of the radio module of the smoke detector on which the test button was pressed
    - The `Stop` field contains the value `0x01`
    - The `Start` field contains the value `0x00`
    - The `Detector` field contains the serial number of the smoke detector (<span style="color: var(--md-typeset-a-color)">:material-alert-circle-outline: not its radio module!</span>) on which the test button was pressed

!!! danger "Start/Stop Mutual Exclusion"
    The behavior of smoke detectors when both the `Start` field and the `Stop` field are set to `0x01` in a packet is not known.

##### Basic properties

- Overall packet length: **36 bytes**
- Repititions: **315** 
- Forwarded within network: **yes**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |   28  | 29 |  30  | 31 |    32-35    |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | 00 | 00 |   XX  | 00 |  XX  | 00 | XX XX XX XX |
Field            | Pkt-# |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                        | Start |    | Stop |    |  Detector   |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x00` | | Unknown, seems to be constant |
| 28 | 1 | `XX` | Start | Start flag<br><br>`0x01`: Starting fire alarm<br>`0x00`: Otherwise |
| 29 | 1 | `0x00` | | Unknown, seems to be constant |
| 30 | 1 | `XX` | Stop | Stop flag<br><br>`0x01`: Stopping or silencing alarming<br>`0x00`: Otherwise |
| 31 | 1 | `0x00` | | Unknown, seems to be constant |
| 32-35 | 4 | `XX XX XX XX`<br><span style="color: var(--md-typeset-a-color)">:material-alert-circle-outline: *little endian*</span> | Detector | Serial number of the smoke detector (not its radio module!) that (initially) detected smoke. |

</div>

#### Line Test (Start/Stop)

The line test function can be used to verify or confirm the membership of smoke detectors to an alarm line. When the line test is initiated, all smoke detectors on the same alarm line as the initiating smoke detector signal their presence.
For more information on performing the line test, please refer to the smoke detector's operating instructions.

The `Line-ID` field contains the ID of the alarm line for which the line test should be started or stopped.

Line tests can be initiated or stopped from the Genius Gateway for known alarm lines. Further details can be found in [Alarm Lines Management](../features/alarm-lines-management.md).

##### Basic properties

- Overall packet length: **29 bytes**
- Repititions: **370** 
- Forwarded within network: **yes**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |     28     |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | XX | 04 |     XX     |
Field            | Pkt-# |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                        | Start/Stop |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x04` | | Unknown, seems to be constant |
| 28 | 1 | `XX` | Start/Stop | `0x06`: Start/Perform line test<br>`0x00`: Stop line test |

</div>

#### Discovery Request

It is not confirmed that this packet is actually a request for discovering smoke detectors. Rather, this designation was chosen based on conclusions drawn from the observed packets and protocol behavior.

##### Observations

These packets occur in connection with commissioning. Once the initiating radio module has sent the [Alarm Line Commissioning Packet](#alarm-line-commissioning), these Discovery Requests are *sometimes* (the exact conditions are not known) additionally transmitted. In my experiments, these requests were answered with [Discovery Response Packets](#discovery-response). The radio module serial numbers contained in them did not belong to my own radio modules and were unknown to me.

Since many residential units in my apartment complex are equipped with smoke detectors from the [Hekatron Genius Plus X :material-open-in-new:](https://www.hekatron-brandschutz.de/produkte/rauchmelder/produkte/genius-plus-x){ target=_blank} system, my current assumption is that the [Discovery Response Packets](#discovery-response) could originate from smoke detectors (i.e. their radio modules) belonging to my neighbors. It may also specifically be their *unnetworked* smoke detectors, i.e., those that are installed in the base but for which commissioning (to register them in an alarm line) has not been performed.

Furthermore, it appears that each radio module (in its own smoke detector network) that either receives the Discovery Request or is involved in the ongoing commissioning also sends the Discovery Request itself (not forwarding it, but initiating it with its own serial number). Each radio module that sent a Discovery Request itself responded to the other requests with its own [Discovery Response](#discovery-response).

!!! tip "Hypothesis"
    Since this packet type is not forwarded by other radio modules, only smoke detectors within direct transmission/reception range of the initiating module can respond. Thus, this part of the protocol could serve to allow smoke detectors to detect all other directly reachable smoke detectors in their vicinity.

    I do not know whether the [Hekatron Genius Plus X :material-open-in-new:](https://www.hekatron-brandschutz.de/produkte/rauchmelder/produkte/genius-plus-x){ target=_blank} system implements **routing** in any way, since I have not found any indication of an *addressed recipient* in the packet structure so far, but such a discovery function could serve to establish **routes** in such a case.

    Perhaps the mechanism is also used to prevent forwarding cycles. There is also no definitive evidence or investigation for this either.

##### Basic properties

- Overall packet length: **28 bytes**
- Repititions: **26** 
- Forwarded within network: **no**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | XX | 00 |
Field            | Pkt-# |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x00` | | Unknown, seems to be constant |

</div>

#### Discovery Response

Like [Discovery Requests](#discovery-request) it is not confirmed that this packet is actually the response to a previous request for discovering smoke detectors. Rather, this designation was chosen based on conclusions drawn from the observed packets and protocol behavior.

##### Observations

Radio modules seem to respond with this packet to a previous [Discovery Request](#discovery-request). The `Org-SN` and `Fwd-SN` fields are populated with the serial number of the responding radio module. Additionally, the `Req-SN` field contains the serial number of the requesting radio module. This makes it possible to identify which requesting radio module the response is intended for.

Further details are described in the [Discovery Request](#discovery-request) section.

##### Basic properties

- Overall packet length: **32 bytes**
- Repititions: **24** 
- Forwarded within network: **no**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |    28-31    |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | XX | 01 | XX XX XX XX |
Field            | Pkt-# |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                        |   Req-SN    |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x01` | | Unknown, seems to be constant |
| 28-31 | 4 | `XX XX XX XX`<br>*big endian* | Req-SN | Serial number of the radio module that sent the original [Discovery Request](#discovery-request).<br><br>This field allows the requesting module to identify that this response is directed to its specific request, enabling proper association between requests and responses in environments with multiple simultaneous discovery operations. |

</div>

[^1]: See [Packet Fragmentation](#packet-fragmentation) section for details on the 64-byte total packet size limit that constrains the maximum payload to 59 bytes in practice.
[^2]: Genius Gateway uses FF FF FF FE (4294967294) as serial number when it sends packets. See [Alarm Lines Management](../features/alarm-lines-management.md) for more information.
 