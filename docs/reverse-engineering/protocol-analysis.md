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
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                   |
```

#### Detailed explanation

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| 0 | 1 | `0x02` | | Unknown, seems to be constant |
| 1-2 | 2 | `CC18 - 0000`<br>(`6.348 - 0`)<br>*little endian* | Rem-TX | **Remaining transmission time.** The radio module repeats every packet for a fixed time window; this field holds the remaining window time in module clock ticks (little-endian), reloaded on each repetition as `remaining = window - elapsed` and counted down toward ~0, at which point the module stops repeating. It is *not* a packet sequence number or counter - see [Repetition](#repetition). |
| 3 | 1 | `0x00` / `0xFF` | | Address mode: `0x00` in normal (line-addressed) packets; `0xFF` marks a wildcard/broadcast packet such as the [ConfigCheckProbe request](#configcheckprobe). |
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
| 23 | 1 | `XX` | | Sequence number - changes from one originated packet to the next. |
| 24 | 1 | `XX` | Flags | Bit-fields: `(type & 3) << 5 \| (relay & 1) << 4 \| (fwd_class & 3) << 2 \| (flag01 & 3)`.<br>Only the **forward-class** (bits 2-3) and **flag01** (bits 0-1, `1` for commissioning and `0` for every other frame) vary; the **type** (bits 5-6) and **relay** (bit 4) fields carry a constant `0x40` (type = 2, relay = 0) in these frames. Observed values: **Alarm Line Commissioning `0x45`**, Line Test / Alarming `0x48`, ConfigCheckProbe `0x40`.<br>The frames of the [radio link supervision](#radio-link-supervision) show that the type bits do vary: Supervision Series `0x20` (forward class 0), Module Missing `0x24` (forward class 1). |
| 25 | 1 | `0x00` | | Unknown, seems to be constant |
| 26 | 1 | `XX` | | First byte of the message body - a **family marker**: `0x55` = ConfigCheckProbe, `0x66` = CommissioningProbe / Line test, `0x00` = status change (alarm, faults), `0x33` = radio link supervision. The [message‑type](#message-type-byte) subtype follows at offset 27. |

</div>

### Message Type Byte

The message body starts at **offset 26** with the **message family**, followed at
**offset 27** by the **message type** within that family. Family and type together (with
the packet length as a check) identify the packet type. The type values repeat across the
families - type `0x00` exists in five of them - so the type byte alone is not enough. In
the per-packet detail tables below the type is the offset-27 row (previously annotated
*"Unknown, seems to be constant"*).

<div class="pckt-table" markdown>

| Family<br>`data[26]` | Type<br>`data[27]` | Length | Packet type |
|:----------:|:----------:|:------:|:------------|
| `0x00` | `0x00` | 36 B | [Status Change (Alarming, Battery Low, Faults)](#status-change) |
| `0x66` | `0x00` | 28 B | [CommissioningProbe Request](#commissioningprobe-request) |
| `0x66` | `0x01` | 32 B | [CommissioningProbe Response](#commissioningprobe-response) |
| `0x66` | `0x03` | 37 B | [Alarm Line Commissioning](#alarm-line-commissioning) |
| `0x66` | `0x04` | 29 B | [Line Test / Network Test (Start/Stop)](#line-test-startstop) |
| `0x55` | `0x05` | 29 B | ConfigCheckProbe probe *(tentative; not yet observed in a capture)* |
| `0x55` | `0x06` | 28 B | [ConfigCheckProbe **request**](#configcheckprobe) |
| `0x55` | `0x08` | 36 B | [ConfigCheckProbe **response**](#configcheckprobe) |
| `0x33` | `0x00` | 28 B | [Supervision Series](#supervision-series) (radio link supervision, FM Pro X) |
| `0x33` | `0x02` | 32 B | [Module Missing](#module-missing) (radio link supervision, FM Pro X) |
| `0x33` | `0x01` | 28 B | Seen on supervised lines - [function unknown](#other-telegrams-on-supervised-lines) |
| `0x11` | `0x00`, `0x01` | 30 B, 28 B | Seen on supervised lines - [function unknown](#other-telegrams-on-supervised-lines) |
| `0x22` | `0x00` | 33 B | Seen on supervised lines - [function unknown](#other-telegrams-on-supervised-lines) |

</div>

!!! note "Why family and type matter for classification"
    Packet length alone is ambiguous: a **36-byte** frame is either a **status change
    (`0x00/0x00`)** or a **ConfigCheckProbe response (`0x55/0x08`)** - classifying purely by
    length can misread a ConfigCheckProbe response as an alarm-stop. The type byte alone is
    ambiguous as well: type `0x00` is the **Status Change** in family `0x00`, the
    **CommissioningProbe Request** in family `0x66` and the **daily supervision series** in
    family `0x33`, the latter two with the same length (28 B). The gateway therefore
    classifies on family and type with the length as a validator (type since v1.5.0, family
    since v1.6.0); packets of a family or type it does not know are shown as unknown.

    The ConfigCheckProbe request (`0x06`), response (`0x08`) and the `0x55` family marker at
    offset 26 were observed in an over-the-air capture of a *Linienabschlusstest*.

### Forwarding (Repeater Function)

Radio modules act as **repeaters**: a module that receives a packet may re-broadcast it so
that packets can reach modules outside the originator's direct range. Forwarding is visible
on the air through two fields of the [common part](#base-packet-structure):

- **`Hops` (offset 22)** starts at `0x0F` on the original transmission and is decremented by
  one on each retransmission, limiting how far a packet propagates.
- **`Fwd-SN` (offset 14–17)** is rewritten to the serial number of the forwarding module,
  while **`Org-SN` (offset 9–12)** keeps the original sender. A forwarded copy can therefore
  be told apart from the original: `Fwd-SN != Org-SN` and `Hops < 0x0F`.

A packet is only forwarded when it targets the module's own line or the broadcast line
(`Line-ID` matches, or is `FF FF FF FF`) and the hop limit is not yet exhausted. Not every
packet type is relayed: the [ConfigCheckProbe](#configcheckprobe)
and [CommissioningProbe](#commissioningprobe-request) exchanges are **direct-range only** (never forwarded,
always `Hops = 0`), whereas status changes (alarms, battery warnings, faults), line tests and commissioning propagate across the line.

Whether a packet may be forwarded at all is decided by its originator: the **forward class** in bits 2-3 of the
`Flags` byte (offset 24). Class 0 is never forwarded, class 1 only within the own line, class 2 within the own line and -
under further conditions - beyond it. This matches every
capture so far: the [Supervision Series](#supervision-series) carries class 0 and was never seen relayed, a status change
carries class 2.

### Repetition

All packets are transmitted multiple times by the radio module. The packets are sent with a period of approximately 10 ms. The repeated packets are identical except for the `Rem-TX` (*Remaining transmission time*) field (see [Base Packet Structure](#base-packet-structure)). This field starts at an individual initial value in the first packet and is decremented with each repetition until it reaches ~0, when transmission stops.

The number of repetitions, the exact period for packet repetitions, as well as the initial value and decrementation slightly differ depending on the packet type:

| Packet Type | Repetitions<br>$N$ | Period Time<br>$T$ | Initial *Rem-TX*<br>$RT_{Start}$ | *Rem-TX* Decrement<br>$\Delta=\frac{RT_{Start}}{N - 1}$ |
|:------------|------------:|-------:|----------------:|:---------:|
| [Alarm Line Commissioning](#alarm-line-commissioning) | 309 x | ~10.06 ms | 6.348 (CC 18) | ~20,6 |
| [Status Change (Alarming, Battery Low, Faults)](#status-change) | 315 x | ~9.85 ms | 6.348 (CC 18) | ~20,2 |
| [Line Test (Start/Stop)](#line-test-startstop) | 370 x | ~8,40 ms | 6.348 (CC 18) | ~17,2 |
| [CommissioningProbe Request](#commissioningprobe-request) | 26 x | ~8,19 ms | 427 (AB 01) | ~17,1 |
| [CommissioningProbe Response](#commissioningprobe-response) | 24 x| ~9,02 ms | 427 (AB 01) | ~18,6 |
| [Supervision Series](#supervision-series) | 26 x | not measured | 427 (AB 01) | ~17,1 *(calculated)* |
| [Module Missing](#module-missing) | 10 - 20 x *(received)* | not measured | 427 (AB 01) | - |

!!! note "Rem-TX decrement"
    It can be observed that the decrement is not constant across packet repetitions but exhibits a certain amount of jitter. This is caused by the higher-resolution underlying timer (the radio module's ~2 kHz clock-tick counter): the remaining transmission time is sampled each iteration and rounded to an unsigned integer when stored in the `Rem-TX` field.

    Furthermore, it is notable that the average decrement approximately equals twice the period time $T$, which corresponds to the count value of a 2 kHz clock. The initial value $RT_{Start}$ thereby corresponds to twice the duration of the entire transmission process (across all repetitions) in milliseconds.

### Specific Packets

The following section describes the structure of specific packets that were received and analyzed in connection with various Genius Plus X functions.

#### Alarm Line Commissioning

The packet described here occurs when functions for commissioning or establishing an alarm line are used, such as initial commissioning, when devices are subsequently added to the alarm line, and possibly other scenarios.

##### Observations

When commissioning is initiated via the radio module of a smoke detector that is already part of an alarm line, the stored alarm line ID is used in both the `Line-ID` and `New Line-ID` fields. This method allows new smoke detectors (i.e., corresponding radio modules that have not yet stored an alarm line ID) to be added to the existing alarm line. For this purpose, commissioning packets are responded to by all radio modules *without* an assigned alarm line ID, as well as *only* by smoke detectors (i.e. their radio modules) that have the same alarm line ID stored (i.e., are part of the initiating alarm line).

When commissioning is initiated via a new radio module (i.e., not yet assigned to an alarm line or assignment deleted), the `Line-ID` field contains the value `00 00 00 00` and a generated alarm line ID is entered in the `New Line-ID` field. All radio modules not yet assigned (i.e., radio modules without a stored alarm line ID) signal their readiness for commissioning and, upon confirmation, store the alarm line ID proposed in `New Line-ID`.

The principle by which new alarm line IDs are generated is not fully understood.

!!! note "Commissioning eligibility is line-ID-state based, not group/line - firmware-confirmed"
    Verified against the FM Basis X firmware reverse-engineering, the decision to join a line
    during commissioning is made **solely from stored line-ID state**:

    - an **unassigned** module (`Line-ID == 0`) adopts the broadcast `New Line-ID` regardless of
      its own group/line - the scope test is `our_line == 0 || rx_line == our_line || rx_line == 0xFFFFFFFF`;
    - an **assigned** module reacts only to frames on its own line-ID.

    The device's **group/line is metadata only**: it is collected by the radio module, carried in
    the `0x03` payload, and written into the peer node
    record - but it is **never compared to gate membership**. Consequence: several uncommissioned
    modules with *different* group/line labels (e.g. `C.3` and `D.2`) commissioned together all
    land on the **same** line-ID. Group/line therefore must **not** be used as a line key - always
    key on `Line-ID`.

##### Basic properties

- Overall packet length: **37 bytes**
- Repititions: **309** 
- Forwarded within network: **yes**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |    28-31    | 32 | 33 | 34 | 35 | 36 |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | 45 | 00 | 66 | 03 | XX XX XX XX | XX | XX | XX | 00 | 00 |
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                        | New Line-ID | Ho | Mi | Se |         |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x03` | Type | Message type - see [Message Type Byte](#message-type-byte). |
| 28-31 | 4 | `XX XX XX XX`<br>*big endian* | New Line-ID | The alarm line ID to be assigned during commissioning.<br><ul><li>When initiated by an already commissioned device: Contains the existing alarm line ID (same as `Line-ID`).</li><li>When initiated by a new device: Contains a newly generated alarm line ID (while `Line-ID` is `00 00 00 00`).</li></ul>See [Observations](#observations) for more details. |
| 32 | 1 | `XX` | Ho(urs) | Hour of the smoke detector's current time in format `0-23`h |
| 33 | 1 | `XX` | Mi(nutes) | Minute of the smoke detector's current time in format `0-59`min |
| 34 | 1 | `XX` | Se(conds) | Second of the smoke detector's current time in format `0-59`s |
| 35 | 1 | `0x00` | | Unknown, seems to be constant |
| 36 | 1 | `0x00` | | Unknown, seems to be constant |

</div>

#### Status Change (Alarming, Battery Low, Faults) {#status-change}

The packets described here announce a change of a smoke detector's **status flags** to all other smoke detectors of an alarm line. The most prominent case is the detection of smoke, which makes all smoke detectors of the alarm line signal it - the exact signaling behavior can be found in the operating instructions. The same packet also announces a low battery, device faults, radio link faults and the dismounting of a smoke detector.

The packet carries the change as a pair of masks over the same 16-bit flag field: the flags newly **set** and the flags newly **cleared**. The flag field is the *ErrorValue* of Hekatron's FM.GW interface - the identical `set/cleared/serial` triple is forwarded 1:1 by the FM.GW serial protocol (`IndNwkStateChange`), which confirms the field semantics and supplies the flag names:

<div class="pckt-table" markdown>

| Flag | Name | Meaning |
|:----:|:-----|:--------|
| `0x0001` | Alarm | Smoke detected - alarm start (set) / alarm stop or silencing (cleared) |
| `0x0002` | Error | Device fault |
| `0x0004` | BattLow | Battery low |
| `0x0008` | Radio | Radio link fault |
| `0x0010` | Mute | A radio link or dismount fault was muted (informational) |
| `0x0020` | Dismount | Smoke detector dismounted from its base |

</div>

The flags are **edge-triggered**: a radio module announces a flag once when it changes (computed as `set = ~current & new`, `cleared = current & ~new`) and never repeats it periodically. A gateway that misses the announcement - e.g. because it was offline - does not learn about the flag until the next change. The FM Basis X measures the battery itself: it is powered from the smoke detector's battery, so its own supply voltage is the detector battery.

##### Observations (alarming)

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

`Start` and `Stop` are the low bytes of the set and cleared masks - a Start Alarming packet is a status change setting the alarm flag `0x0001`, a Stop Alarming packet one clearing it.

##### Observations (battery low and faults)

- A battery warning was captured as a status change with `set = 0x0004` (BattLow), `cleared = 0x0000` and the serial of the affected smoke detector in `Detector` - mesh-forwarded like an alarm, with `Org-SN` naming the radio module of that smoke detector.
- Device faults (`0x0002`), radio link faults (`0x0008`) and dismounting (`0x0020`) use the same packet with their own flag; a flag that recovers is announced in the cleared mask.

##### Basic properties

- Overall packet length: **36 bytes**
- Repititions: **315** 
- Forwarded within network: **yes**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |  28-29  |  30-31  |    32-35    |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | 00 | 00 |  XX XX  |  XX XX  | XX XX XX XX |
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                        |   Set   | Cleared |  Detector   |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x00` | Type | Message type - see [Message Type Byte](#message-type-byte). |
| 28-29 | 2 | `XX XX`<br><span style="color: var(--md-typeset-a-color)">:material-alert-circle-outline: *little endian*</span> | Set | Status flags newly **set** (see flag table above).<br><br>Low byte = former *Start* field: `0x01` starts a fire alarm. |
| 30-31 | 2 | `XX XX`<br><span style="color: var(--md-typeset-a-color)">:material-alert-circle-outline: *little endian*</span> | Cleared | Status flags newly **cleared** (see flag table above).<br><br>Low byte = former *Stop* field: `0x01` stops or silences alarming. |
| 32-35 | 4 | `XX XX XX XX`<br><span style="color: var(--md-typeset-a-color)">:material-alert-circle-outline: *little endian*</span> | Detector | Serial number of the smoke detector (not its radio module!) whose status changed - e.g. the one that (initially) detected smoke. |

</div>

!!! example "Captured battery warning"
    ```
    02 AE 0A 00 FF FF FF FF 00 83 5F E1 16 00 83 5F E1 15 B7 B7 AA 11 0E 26 48 00 00 00 04 00 00 00 3B F6 A5 82
    ```
    Type `0x00`, set `0x0004` (BattLow), cleared `0x0000`, detector `0x82A5F63B` (2191914555), originated by radio module `0x835FE116` and forwarded by `0x835FE115`.

#### Line Test / Network Test (Start/Stop) {#line-test-startstop}

The line test function can be used to verify or confirm the membership of smoke detectors to an alarm line. When the line test is initiated, all smoke detectors on the same alarm line as the initiating smoke detector signal their presence. The same packet starts a **network test**, which the manufacturer describes as the test across the whole network; the two differ in the test mode byte and in how long the detectors keep signalling.
For more information on performing the line test, please refer to the smoke detector's operating instructions.

The `Line-ID` field contains the ID of the alarm line for which the line test should be started or stopped.

Line tests can be initiated or stopped from the Genius Gateway for known alarm lines. Further details can be found in [Alarm Lines Management](../features/alarm-lines-management.md).

##### Test mode byte - how line test and network test were told apart

The radio modules send exactly three values in byte 28:

| Value | Sent when | Duration of the test |
|:-----:|:----------|:----------------------|
| `0x04` | a test is started in the first of two test states | 15 minutes - as long as the commissioning mode |
| `0x06` | a test is started in the second test state | 30 minutes |
| `0x00` | either test state is left | - |

The manufacturer states that a line test lasts about 15 minutes and a network test about 30 minutes, and the serial protocol between the Genius Port and its radio module knows exactly the test modes *no test*, *line test* and *network test*. The two durations match, which gives the assignment above.

- **Confirmed by captures:** message family `0x66`, type `0x04`, 29 bytes, and the test mode values `0x04` and `0x06` on air.
- **Derived, not confirmed on air:** which of the two values is the line test. The assignment follows from the durations, not from a named field.

Up to v1.5.0 Genius Gateway treated byte 28 as a start/stop flag with `0x06` as the start, and its **Start Line Test** action sent `0x06` - by the assignment above a network test. Since v1.6.0 it sends `0x04`.

!!! example "Captured line test start"
    ```
    02 CC 18 00 FF FF FF FF 00 ss ss ss ss 00 ss ss ss ss ll ll ll ll 0F 5B 48 00 66 04 04
    ```
    Family `0x66`, type `0x04`, test mode `0x04` (line test), flags `0x48` (forward class 2). The serial number of the radio module (`ss`) and the alarm line ID (`ll`) are left out.

##### Basic properties

- Overall packet length: **29 bytes**
- Repititions: **370** 
- Forwarded within network: **yes**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |     28     |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | XX | 04 |     XX     |
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                        | Start/Stop |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x04` | Type | Message type - see [Message Type Byte](#message-type-byte). |
| 28 | 1 | `XX` | Test mode | `0x04`: Start a **line test** (about 15 minutes)<br>`0x06`: Start a **network test** (about 30 minutes)<br>`0x00`: Stop the running line or network test |

</div>

#### CommissioningProbe Request

This exchange runs **during commissioning**. After the initiating module broadcasts the [Alarm Line Commissioning](#alarm-line-commissioning) packet (the line assignment), it - and other involved modules - broadcast a **CommissioningProbe Request**, and every **directly reachable** module replies with a back-addressed [CommissioningProbe Response](#commissioningprobe-response) (`Req-SN` = the requester). It is a **direct-range reachability enumeration** (not forwarded, `Hops = 0`) - a broadcast *"who is reachable?"* - rather than a targeted request/response. (The name reflects this observed role.)

##### Observations

These packets occur in connection with commissioning. Once the initiating radio module has sent the [Alarm Line Commissioning Packet](#alarm-line-commissioning), these CommissioningProbe Requests are *sometimes* (the exact conditions are not known) additionally transmitted. In my experiments, these requests were answered with [CommissioningProbe Response Packets](#commissioningprobe-response). The radio module serial numbers contained in them did not belong to my own radio modules and were unknown to me.

Since many residential units in my apartment complex are equipped with smoke detectors from the [Hekatron Genius Plus X :material-open-in-new:](https://www.hekatron-brandschutz.de/produkte/rauchmelder/produkte/genius-plus-x){ target=_blank} system, my current assumption is that the [CommissioningProbe Response Packets](#commissioningprobe-response) could originate from smoke detectors (i.e. their radio modules) belonging to my neighbors. It may also specifically be their *unnetworked* smoke detectors, i.e., those that are installed in the base but for which commissioning (to register them in an alarm line) has not been performed.

Furthermore, it appears that each radio module (in its own smoke detector network) that either receives the CommissioningProbe Request or is involved in the ongoing commissioning also sends the CommissioningProbe Request itself (not forwarding it, but initiating it with its own serial number). Each radio module that sent a CommissioningProbe Request itself responded to the other requests with its own [CommissioningProbe Response](#commissioningprobe-response).

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
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x00` | Type | Message type - see [Message Type Byte](#message-type-byte). |

</div>

#### CommissioningProbe Response

Like [CommissioningProbe Requests](#commissioningprobe-request) it is not confirmed that this packet is actually the response to a previous request for discovering smoke detectors. Rather, this designation was chosen based on conclusions drawn from the observed packets and protocol behavior.

##### Observations

Radio modules seem to respond with this packet to a previous [CommissioningProbe Request](#commissioningprobe-request). The `Org-SN` and `Fwd-SN` fields are populated with the serial number of the responding radio module. Additionally, the `Req-SN` field contains the serial number of the requesting radio module. This makes it possible to identify which requesting radio module the response is intended for.

Further details are described in the [CommissioningProbe Request](#commissioningprobe-request) section.

##### Basic properties

- Overall packet length: **32 bytes**
- Repititions: **24** 
- Forwarded within network: **no**

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |    28-31    |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | XX | 00 | XX | 01 | XX XX XX XX |
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                        |   Req-SN    |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown> 

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* |
| 27 | 1 | `0x01` | Type | Message type - see [Message Type Byte](#message-type-byte). |
| 28-31 | 4 | `XX XX XX XX`<br>*big endian* | Req-SN | Serial number of the radio module that sent the original [CommissioningProbe Request](#commissioningprobe-request).<br><br>This field allows the requesting module to identify that this response is directed to its specific request, enabling proper association between requests and responses in environments with multiple simultaneous discovery operations. |

</div>

#### ConfigCheckProbe

A silent, direct-range reachability exchange, observed over the air during a
*Linienabschlusstest*. Unlike the [CommissioningProbe Request/Response](#commissioningprobe-request) the
request is **wildcard-addressed** (byte 3 = `0xFF` instead of the usual `0x00`, `Line-ID` =
`FF FF FF FF`) and the exchange is **not forwarded** - every frame stays at `Hops = 0`.

##### Observations

- The request is broadcast repeatedly; each directly reachable detector answers once per
  sweep with a 36-byte response carrying its **own** `Org-SN`/`Line-ID` and a small
  status + group/line payload - it is *not* back-addressed to the requester like the
  [CommissioningProbe Response](#commissioningprobe-response).
- In one capture a single request sweep drew responses from ~15 distinct radio modules,
  including modules on other `Line-ID`s (neighbouring installations in direct range).
- Both request and response share the constant family marker `0x55` at offset 26, followed
  by the [message type](#message-type-byte) at offset 27 (`0x06` request, `0x08` response).

##### Request

- Overall packet length: **28 bytes**
- Forwarded within network: **no**

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |
Value (hex) | 02 | XX XX | FF | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | FF FF FF FF |  0F  | XX | XX | 00 | 55 | 06 |
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                   | Ty |
```

<div class="pckt-table" markdown>

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure). Note byte 3 is `0xFF` (wildcard, not the usual `0x00`), `Line-ID` is broadcast, and byte 26 is the `0x55` family marker.* |
| 27 | 1 | `0x06` | Type | [Message type](#message-type-byte) - ConfigCheckProbe request |

</div>

##### Response

- Overall packet length: **36 bytes**
- Forwarded within network: **no**

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 | 32 | 33 | 34 | 35 |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  0F  | XX | XX | 00 | 55 | 08 | 00 | XX | XX | 00 | 60 | 00 | 00 | XX |
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |                   | Ty |    | St | GL |    |    |    |    | Ck |
```

<div class="pckt-table" markdown>

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-26* | *27* | | | *See [Base Packet Structure](#base-packet-structure). Byte 26 is the `0x55` family marker; `Org-SN` / `Line-ID` are the **responder's own**.* |
| 27 | 1 | `0x08` | Type | [Message type](#message-type-byte) - ConfigCheckProbe response |
| 28 | 1 | `0x00` | | Constant |
| 29 | 1 | `XX` | Status | Per-device status/capability flag (`0x00` or `0x40` observed) |
| 30 | 1 | `XX` | Group/Line | Responder group/line label: high nibble = group `A–H`, low nibble = line `0–9`. **Display metadata only - not the line identity.** The authoritative line is the 32-bit `Line-ID` (offset 18–21); group/line is not commissioning-gating (see [line addressing](#line-addressing-fm-basis-x-vs-old-fm-basis)) and can differ between modules on the same line. |
| 31 | 1 | `0x00` | | Constant |
| 32 | 1 | `0x60` | | Constant |
| 33-34 | 2 | `00 00` | | Constant |
| 35 | 1 | `XX` | | Trailing byte, varies per frame (checksum or metric - not confirmed) |

</div>

#### Radio Link Supervision (FM Pro X) {#radio-link-supervision}

FM Pro X radio modules can supervise their line: with DIP switch 5 (*Funkstreckenüberwachung*, off by default) switched
on, the modules of a line check once a day whether all of them are still there. The manufacturer describes it like this
(product information *Funkmodule Basis X und Pro X für Genius Plus X*, 7050611b, issue 26.06.2020):

> Jedes Funkmodul besitzt eine eindeutige Seriennummer. Bei der Inbetriebnahme speichert jedes Funkmodul die Seriennummer
> der Funkmodule, die zu einer Linie zusammengefasst wurden (max. 20 Funk-Teilnehmer pro Linie). [...] Die
> Funkstreckenüberwachung überprüft anhand der Seriennummer täglich, ob alle miteinander vernetzten Funk-Rauchwarnmelder
> noch vorhanden sind. Ist die Linie unvollständig, wird dies über ein akustisches Signal ausgegeben.

The FM Basis X has no DIP switches; according to the same document all these functions are fixed to *off* there.

On the air the supervision shows up as message family `0x33`. Two of its telegrams are understood and evaluated by
Genius Gateway: the daily [Supervision Series](#supervision-series) and the [Module Missing](#module-missing) report.
Both were captured and analysed by [danielrdt :material-open-in-new:](https://github.com/danielrdt){ target=_blank } on a
test line with two FM Pro X modules over 14 days
([raw captures :material-open-in-new:](https://github.com/danielrdt/genius-log/blob/main/docs/captures-radio-link-supervision.md){ target=_blank },
[telegram overview :material-open-in-new:](https://github.com/danielrdt/genius-log/blob/main/docs/genius-telegramme.md){ target=_blank }).

##### What happens when a module goes missing

Timeline of the one complete failure that was captured (one of the two modules unscrewed from its base):

| Time after the first missed series | Telegram | Remark |
|:--|:--|:--|
| 0 | - | The daily series of the removed module does not come |
| about 32 min | `0x11/0x00` | function unknown |
| + 90 s | `0x22/0x00` | function unknown; carries a clock time and a day counter |
| + 60 s | `0x33/0x01` | function unknown |
| about 50 min later | [`0x33/0x02` Module Missing](#module-missing) | names the serial number of the missing module |
| roughly 10.5 h after the first missed series | [`0x33/0x02` Module Missing](#module-missing) again, 2 min later `0x11/0x01`, 2 s later a [Status Change](#status-change) with set mask `0x0008` | radio link fault - the detector starts to beep |
| later, by hand | [Status Change](#status-change), set mask `0x0010` | the fault was muted on the detector |

A line does not recover by itself. After the module was screwed back in, both modules stayed silent - no series, no
"fault cleared" status change. Only a new commissioning brought the daily series back. This matches the manufacturer's
instruction for this fault: *"Netzwerk überprüfen und Funk-Inbetriebnahme neu durchführen."*

##### How far this is verified

| Statement | Source |
|:--|:--|
| Supervision is a function of the FM Pro X only | Manufacturer (DIP switch only on the Pro X) |
| The modules check each other daily, by serial number | Manufacturer. Captures: one series per module per day; the report names a serial number |
| The series is not forwarded | Captures (never relayed in 10 days). Its forward class is 0, which is never relayed |
| `0x33/0x02` names the missing module | Captures: three reports, each with the serial number of the module that had been removed |
| `0x33/0x02` is forwarded within the line | **Not observed** (the test line had two modules). Its forward class 1 says so |
| A radio link fault follows as status flag `0x0008`, muting as `0x0010` | Captures. The host protocol of the Genius Port names exactly these two flags (*Funkstreckenfehler*, *Mute eines Funkstrecken- oder Demontage-Fehlers*) |
| 26 repetitions | Captures. The initial `Rem-TX` of 427 is the one of the other packets with 26 repetitions |

Not verified: everything about *why* a module sends these
telegrams is derived from captures of one line with two modules. Lines with three or more modules, with FM Basis X
modules among them, or with several missing modules have not been observed.

#### Supervision Series {#supervision-series}

The daily sign of life of a module with radio link supervision switched on. Every module of the line sends its own
series; there is no reply. The modules apparently only check whether they hear the series of the others.

##### Observations

- One series per module and day, at 00:03 of a clock the line keeps for itself (it starts at the commissioning and is
  not the local time). A series consists of about 6 - 8 bursts roughly 2 minutes apart, each burst being one packet
  repeated 26 times.
- The start moves about 8 s earlier per day, so the period is slightly below 24 hours.
- The packet is never forwarded and never answered. At the edge of the range single bursts are missed.
- Same length and same type byte as the [CommissioningProbe Request](#commissioningprobe-request) - only the family
  byte tells them apart. Up to v1.5.0 Genius Gateway showed it as a CommissioningProbe Request.
- For Genius Gateway the series is what makes the supervision useful beyond the fault itself: a module that sends it
  was heard today.

##### Basic properties

- Overall packet length: **28 bytes**
- Repetitions: **26** per burst
- Forwarded within network: **no** (forward class 0)

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  0F  | XX | 20 | 00 | 33 | 00 |
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |    |Flags|   |Fam.|Type|
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown>

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-25* | *26* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* `Org-SN` and `Fwd-SN` are always the sending module, `Hops` is always `0x0F`. |
| 24 | 1 | `0x20` | Flags | Forward class 0 - never forwarded. |
| 26 | 1 | `0x33` | Family | Radio link supervision - see [Message Type Byte](#message-type-byte). |
| 27 | 1 | `0x00` | Type | Supervision series. |

</div>

!!! example "Captured series packet"
    ```
    02 AB 01 03 FF FF FF FF 03 83 A7 C9 0B 03 83 A7 C9 0B A3 1A D9 0B 0F D5 20 00 33 00
    ```
    Module `0x83A7C90B` on line `0xA31AD90B`, flags `0x20`. Capture by danielrdt.

#### Module Missing {#module-missing}

A module reports that it no longer hears another module of its line, and names it by its serial number.

##### Observations

- Seen three times, always after a module had been removed, and always with the serial number of that module.
- It is not the fault itself: in the one complete sequence the first report came well over an hour after the first
  missed series, while the radio link fault ([Status Change](#status-change) with set mask `0x0008`) followed hours
  later, 2 minutes after another report. The report can therefore serve as an early warning - and it is the only
  telegram that says *which* module is missing. The status change carries the serial number of the detector that
  reports the fault, not of the one that is gone.
- 10 - 20 repetitions were received per burst. The initial `Rem-TX` of 427 suggests 26, as for the series; the capture
  was taken at the edge of the range.
- Whether it is forwarded could not be observed on a line with two modules. Its forward class 1 says: within the own
  line, yes.

##### Basic properties

- Overall packet length: **32 bytes**
- Repetitions: **10 - 20** received (26 expected)
- Forwarded within network: **presumably yes**, within the own line (forward class 1) - not observed

##### Compact view (complete)

```
Byte offset |  0 |  1-2  |  3 |  4 |  5 |  6 |  7 |  8 |    9-12     | 13 |    14-17    |    18-21    |  22  | 23 | 24 | 25 | 26 | 27 |    28-31    |
Value (hex) | 02 | XX XX | 00 | FF | FF | FF | FF | 00 | XX XX XX XX | 00 | XX XX XX XX | XX XX XX XX |  XX  | XX | 24 | 00 | 33 | 02 | XX XX XX XX |
Field            | Rem-TX |                             |   Org-SN    |    |   Fwd-SN    |   Line-ID   | Hops |    |Flags|   |Fam.|Type| Missing-SN  |
```

##### Detailed explanation (delta)

<div class="pckt-table" markdown>

| Offset | Length<br>(bytes) | Value | Field | Purpose/Description |
|:------:|:-----------------:|:-----:|:------|:--------------------|
| *0-25* | *26* | | | *See [Base Packet Structure](#base-packet-structure) for details on the common packet part.* `Org-SN` is the module that reports. |
| 24 | 1 | `0x24` | Flags | Forward class 1 - forwarded within the own line only. |
| 26 | 1 | `0x33` | Family | Radio link supervision - see [Message Type Byte](#message-type-byte). |
| 27 | 1 | `0x02` | Type | Module missing. |
| 28-31 | 4 | `XX XX XX XX`<br>*big endian* | Missing-SN | Serial number of the radio module that is missing - same notation as `Org-SN`. |

</div>

!!! example "Captured report"
    ```
    02 AB 01 03 FF FF FF FF 03 83 A7 C9 0B 03 83 A7 C9 0B A3 1A D9 0B 0F A8 24 00 33 02 83 A7 BC 5F
    ```
    Module `0x83A7C90B` reports module `0x83A7BC5F` as missing, flags `0x24`. Capture by danielrdt.

##### What Genius Gateway does with it

- The detector with the reporting module gets an entry in its [activity log](../features/device-management.md#viewing-the-activity-log).
- The detector with the missing module - if it is registered - gets the status **Reported missing**. It counts as a
  fault, like the radio link fault that the reporting detector announces later.
- As a line does not report a recovery, the state ends when any packet that the missing module originated or
  transmitted is received again - in practice after the new commissioning - or when the status is reset by hand.

#### Other telegrams on supervised lines

Four more telegrams were captured on the supervised test line. Their function is unknown; Genius Gateway shows them
as *Unknown* in the packet visualizer and only takes them as a sign of life of the sending module.

<div class="pckt-table" markdown>

| Family / Type | Length | Flags | Repetitions | Seen |
|:-------------:|:------:|:-----:|:-----------:|:-----|
| `0x33` / `0x01` | 28 B | `0x04` | 26 | After a commissioning and after a module failure, following `0x11` and `0x22`. Forwarded. |
| `0x11` / `0x00` | 30 B | `0x48` | about 350 | Long burst after a commissioning and on a module failure; the two payload bytes were always `B6 08`. Forwarded. |
| `0x11` / `0x01` | 28 B | `0x28` | 11 | Twice, 2 s before the radio link fault. |
| `0x22` / `0x00` | 33 B | `0x08` | 23 | About 90 s after every `0x11/0x00`. Payload: hour, minute and second of the line's own clock and a 16-bit day counter. Forwarded. |

</div>

The family `0x11` is not specific to the FM Pro X: FM Basis X modules can send both of its telegrams as well
(`0x11/0x00` with a 16-bit value, `0x11/0x01` without payload). What they are for is open there, too.

## Line Addressing: FM Basis X vs. old FM Basis

The `Line-ID` field (offset 18–21) means different things depending on the **generation** of the radio module that produced it. This matters because the Genius Gateway was reverse-engineered around the **new** generation (FM Basis X), but old **FM Basis** modules behave fundamentally differently - and this is a common source of interop confusion.

!!! warning "Two incompatible line-identity schemes"
    A 32-bit `Line-ID` from an FM Basis **X** is a *commissioning-assigned random code*. A `Line-ID` seen from an *old* FM Basis is (almost certainly) a *deterministic function of the rotary switch position*. They are not interchangeable and are generated by completely different mechanisms.

### New generation - FM Basis X (and Pro X)

- The line is identified by a **32-bit Identifikationscode** that is assigned **automatically and uniquely during Funk-Inbetriebnahme (commissioning)** and stored by every module of that line. After commissioning, a module only reacts to packets carrying *both* the same line and the same ID; neighbouring systems are ignored.[^3]
- The code is effectively **random** (see [New Line-ID generation](#alarm-line-commissioning) - "the principle by which new alarm line IDs are generated is not fully understood").
- FM Basis X has **no rotary switch** - it is permanently fixed to the label "A.0" ex factory. The displayed "A.0" is therefore *not* the network identity; the 32-bit ID is.
- The **group/line label** (e.g. `C.3`) is **device metadata, not a line key**: the firmware never uses it to gate commissioning (see the note under [Alarm-line commissioning](#observations)), neighbouring installations reuse the same labels, and modules on one line can carry different labels. Use it for display only; key lines on `Line-ID`.
- Over smartsonic [acoustic readout](acoustic-readout.md), bytes 25–28 carry this commissioned 32-bit `lineId` (non-zero once commissioned; `0` = unassigned).

### Old generation - FM Basis

- The line is set **physically via two rotary switches** (major `A–G`/`H`, minor `0–9`; up to ~70 lines). Factory default is A.0 but it **is changeable** (unlike Basis X).
- There is **no Identifikationscode** and **no commissioning-assigned ID** - the rotary position *is* the entire line identity.
- Over smartsonic, an old FM Basis transmits **only its type and serial number** - *not* the radio dataset. Consequently the acoustic `lineId` (bytes 25–28) reads **`0`**.
- Because the rotary is the module's *only* source of line identity, the 32-bit `Line-ID` it emits on the 868 MHz radio **must be a deterministic function of the rotary setting** (there is no other entropy source). I.e. a fixed rotary→Line-ID mapping, hardcoded in the module firmware.

### Working hypothesis & test

Observation: an old FM Basis set to **A.0** was seen emitting `Line-ID = 0xD7F75240` (3623309888). A commissioned FM Basis **X** also labelled "A.0" emitted a *different*, random `0xA9B22DF3` - confirming the two schemes are unrelated.

**Hypothesis:** for old FM Basis, `Line-ID = f(major, minor)` is deterministic, so *every* old FM Basis at A.0 emits `0xD7F75240`.

Falsifiable experiment:

| Step | Action | Expected result if hypothesis holds |
|------|--------|--------------------------------------|
| 1 | Sniff a second old FM Basis set to A.0 | Same `Line-ID` = `0xD7F75240` |
| 2 | Sweep one module through rotary A.0 → A.1 → B.0 → … and log the `Line-ID` each time | Reveals whether `f` is a flat lookup table or an algorithm over `major`/`minor` (e.g. CRC/scramble of `major*10+minor`) |

| Rotary (major.minor) | Observed `Line-ID` (hex) | Observed (dec) |
|:--------------------:|:------------------------:|:--------------:|
| A.0 | `0xD7F75240` | 3623309888 |
| *…* | *(to be filled)* | |

If confirmed, the gateway can map old-module rotary lines to their fixed `Line-ID`s (lookup or formula) and address them the same way it addresses commissioned FM Basis X lines.

[^1]: See [Packet Fragmentation](#packet-fragmentation) section for details on the 64-byte total packet size limit that constrains the maximum payload to 59 bytes in practice.
[^2]: Genius Gateway uses FF FF FF FE (4294967294) as serial number when it sends packets. See [Alarm Lines Management](../features/alarm-lines-management.md) for more information.
[^3]: Source: Hekatron *Bedienungsanleitung Funkmodul Basis X / Pro X* (7003145), section "Identifikationscode", and *Genius Funksysteme im Mischbetrieb* whitepaper (7050644).
 