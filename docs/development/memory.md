---
icon: tabler/cpu-2
---

# Memory Considerations

Genius Gateway targets ESP32-S3 boards with **8 MB PSRAM** (e.g. Seeed XIAO ESP32-S3). At the design limit of **50 smoke detectors**, the Home Assistant MQTT discovery integration alone creates **700 sub-entities** (14 per detector). The cumulative permanent heap footprint of these objects exhausts the ESP32-S3's internal RAM long before reaching 50 devices.

This page documents the layered memory strategy that keeps the device list, HA framework, and TLS handshake all working under that load, and explains what changes when running on a board without PSRAM.

!!! tip "No PSRAM? It still works"
    The project compiles and runs unchanged on no-PSRAM ESP32-S3 boards (e.g. the `esp32-s3-devkitc-1` environment). With only the ~400 KB of usable internal RAM available, the safe practical ceiling drops to about **10 smoke detectors** — enforced by a `-D GATEWAY_MAX_DEVICES=10` build flag on that environment. See [Running without PSRAM](#running-without-psram) below for the full picture.

## :tabler-target: The problem at scale

The objects that survive between requests — `GeniusDevice` entries in the live list, `HADevice` sub-devices, and the 14 `HAEntityBase` instances per device — are heap-allocated and **never freed** during normal operation. Each `HAEntityBase` carries a handful of `String` members (object id, component type, name, icon, device class, etc.), so the **per-entity cost** is small individually but accumulates fast.

Without intervention the symptoms appear in this order as device count grows:

1. **20+ devices:** `xSemaphoreCreateMutex()` returns `NULL` during boot — internal heap exhausted before service startup completes.
2. **Smaller counts:** the TLS handshake for GitHub release checks fails with `MBEDTLS_ERR_SSL_ALLOC_FAILED (-32512)` because heap fragmentation drops the largest free block below the working-set size needed by mbedTLS, even when total free internal RAM still looks comfortable.

PSRAM is the obvious answer — there's 8 MB of it sitting almost unused — but routing the right things into it requires both a coarse SDK-level mechanism and finer per-collection intent.

## :tabler-layers-subtract: The layered strategy

Three independent mechanisms work together. Each one alone is insufficient; all three combined produce a robust system.

### Layer 1 — SDK-level threshold routing

The `custom_sdkconfig` block in `platformio.ini` (PSRAM environments only) lowers the threshold at which the standard `malloc()` prefers PSRAM over internal RAM:

```ini
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=32
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=8192
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

With `ALWAYSINTERNAL=32`, every allocation **larger than 32 bytes** prefers PSRAM. Only the very small, hot allocations (function-shells, short captures, immediate-mode buffers) stay in fast internal SRAM. This catches almost everything dynamic — `String` content longer than 32 bytes, vector backing stores, struct allocations, JSON document buffers — without any code change.

`TRY_ALLOCATE_WIFI_LWIP=y` additionally pushes WiFi RX/TX and LWIP buffers (~50 KB) into PSRAM, and `RESERVE_INTERNAL=8192` reduces the legacy reservation for in-ISR allocations that this codebase doesn't make.

### Layer 2 — mbedTLS session buffers in PSRAM

The mbedTLS allocator is configured independently from `ALWAYSINTERNAL`. Without explicit routing the TLS working set (~30–50 KB during handshake) competes for internal RAM and fails once the heap is fragmented:

```ini
CONFIG_MBEDTLS_DEFAULT_MEM_ALLOC=n
CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC=n
CONFIG_MBEDTLS_CUSTOM_MEM_ALLOC=n
CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y
```

The four `MBEDTLS_*_MEM_ALLOC` symbols are a Kconfig choice — only one may be active, so the three unused options are explicitly disabled.

### Layer 3 — Explicit PsramAllocator on growth collections

Layer 1 covers everything by size, but it doesn't make design *intent* visible. The header [`lib/framework/PsramAllocator.h`](https://github.com/hmbacher/genius-gateway/blob/main/lib/framework/PsramAllocator.h) provides a standard-compliant `std::allocator` template that allocates explicitly from PSRAM:

```cpp
template <class T> struct PsramAllocator {
    T *allocate(std::size_t n) {
        void *p = psramFound()
            ? heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
            : nullptr;
        if (!p) p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_8BIT); // fallback
        if (!p) abort();
        return static_cast<T *>(p);
    }
    /* ... */
};
```

It is used on the collections whose growth is **driven by device count or runtime usage**:

| Collection | File | Purpose |
| --- | --- | --- |
| `GeniusDevices::devices` | [`src/GeniusDevicesService.h`](https://github.com/hmbacher/genius-gateway/blob/main/src/GeniusDevicesService.h) | The live list of configured smoke detectors |
| `ImportSession::staging` | [`src/GeniusDevicesService.h`](https://github.com/hmbacher/genius-gateway/blob/main/src/GeniusDevicesService.h) | Chunked-import staging buffer |
| `HAService::_subDevices` | ESP32-SvelteKit framework | All `HADevice` instances (one per detector) |
| `HAService::_publishCallbacks` / `_unpublishCallbacks` | ESP32-SvelteKit framework | MQTT connect / disconnect callback lists |
| `HADevice::_entities` | ESP32-SvelteKit framework | The 14 HA entities per smoke detector |

The allocator controls where the vector's **backing storage** lives. The contained objects (`HADevice`, `HAEntityBase` subclasses) are still allocated via `std::make_unique` / `new` — those allocations go through the standard heap and are routed by Layer 1's threshold (every such object is well above 32 bytes).

## :tabler-cpu: Hot-path exception: CC1101 RX task stack

Layer 1's threshold has one notable downside: **task stacks created via plain `xTaskCreatePinnedToCore` also land in PSRAM**, which adds 3–5× per-access latency. For most tasks (HTTP, MQTT, HA) this is acceptable — they're not real-time. The CC1101 RX task is the exception: it wakes from an ISR-triggered task notification and must read the radio FIFO before the next packet arrives.

[`src/GeniusGateway.cpp`](https://github.com/hmbacher/genius-gateway/blob/main/src/GeniusGateway.cpp) creates this task with the caps-aware API so its stack is forced into internal DRAM:

```cpp
xTaskCreatePinnedToCoreWithCaps(
    this->_rx_packetsImpl, RX_TASK_NAME, RX_TASK_STACK_SIZE,
    this, RX_TASK_PRIORITY, &GeniusGateway::xRxTaskHandle,
    RX_TASK_CORE_AFFINITY,
    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
```

### Hot-path data accesses

Forcing the stack into internal RAM solves the per-access latency on locals. The remaining question is what *data* the task touches when it runs. Tracing `_rx_packets()` shows that the every-packet path stays almost entirely out of PSRAM:

| Step | Touches | Location |
| --- | --- | --- |
| ISR → `vTaskNotifyGiveIndexedFromISR` | Task notification slot in FreeRTOS TCB | Internal |
| Task wake, `ulTaskNotifyTakeIndexed` | Task stack locals | Internal (forced) |
| `cc1101_receive_data(&packet)` | SPI registers + stack-resident `packet` | Internal |
| `Utils::xorHash()` over packet bytes | Stack only | Internal |
| `_lastPacketHash` compare (uint32_t member) | `GeniusGateway` object in `.bss` | Internal |
| `_wsLogger.logPacket(&packet)` | WebSocket client list (small) | PSRAM-backed but tiny — typically 1–5 entries |
| `cc1101_check_rx_fifo(true)` | SPI status register | Internal |

The only PSRAM touch on every packet is a brief read over the WebSocket client list — a handful of pointer-sized entries, sub-microsecond at PSRAM bandwidth.

The alarm-handling path is rarer (only on `HPT_ALARM_START` / `HPT_ALARM_STOP`) but *does* access PSRAM-resident data:

- `isSmokeDetectorKnown()` — linear scan of the `devices` vector
- `setAlarm()` / `resetAlarm()` — same scan plus a write to the matched entry
- `mqttPublishDeviceState()` — touches `HADevice::_entities` and allocates a JSON document

At 50 devices the scan reads ~10 KB from PSRAM. At ~80 MB/s QPI bandwidth that's ~125 µs vs. ~25 µs from internal SRAM — about **100 µs of overhead** per alarm event. Total alarm-handler cost is a few hundred µs, well under 5 % of the 10 ms inter-packet budget.

### Why this is safe at 100 Hz

Genius packets repeat with high redundancy — a single physical event typically retransmits **300+ times** as the mesh propagates the announcement. Only the first arrival drives the alarm branch; every subsequent repeat is caught by the `_lastPacketHash` duplicate filter at the top of the RX loop and short-circuits to the cheap log+housekeeping path. Sustained reception therefore runs the heavy alarm path at most once per real event, while the 100 Hz packet stream itself stays on the internal-RAM-only fast path.

### Measuring it

The `GPIO_TEST1` / `GPIO_TEST2` toggles around the packet-handling and WS-logger blocks in `_rx_packets()` are wired up specifically for scoping the RX hot path. Probe them with a logic analyzer to compare before/after numbers if the device count or the heap layout changes meaningfully.

### Future-proofing

The one scaling concern is the **linear scan in `isSmokeDetectorKnown` / `setAlarm` / `resetAlarm`**: cost grows with device count *regardless* of where the vector lives. Replacing it with a hash lookup (`std::unordered_map<uint32_t, GeniusDevice *>` keyed on smoke alarm ID) would matter more than memory placement once device counts climb. At the current 50-device cap it is not a problem.

## :tabler-chart-line: Observed footprint at the design limit

Boot heap with 50 configured smoke detectors, MQTT connected, HA discovery published, WebSocket clients connected, and a successful TLS handshake against the GitHub release endpoint:

| Metric | Value |
| --- | --- |
| Internal RAM free | ~60 KB |
| Internal RAM max alloc block | ~35 KB |
| PSRAM used | ~190 KB (2.3 % of 8 MB) |
| PSRAM free | ~8.2 MB |

The largest internal allocation block stays comfortably above the ~10 KB working set mbedTLS needs for a handshake, and PSRAM has more than 40× the headroom that's actually consumed. The design has substantial margin at the configured limit.

## :tabler-circle-off: Running without PSRAM

The architecture compiles and runs unchanged on no-PSRAM boards. `PsramAllocator::allocate()` checks `psramFound()` at runtime and falls back to regular `heap_caps_malloc(MALLOC_CAP_8BIT)` when PSRAM is absent. No symbol is conditional on PSRAM being available.

What *does* change is the practical device ceiling. With everything competing for the ~400 KB of usable internal RAM, the safe limit drops sharply. The [`env:esp32-s3-devkitc-1`](https://github.com/hmbacher/genius-gateway/blob/main/platformio.ini) environment (no-PSRAM ESP32-S3-DevKitC-1) overrides the compile-time cap:

```ini
build_flags =
    ${env.build_flags}
    ${esp32-s3-devkitc-1.build_flags}
    -D GATEWAY_MAX_DEVICES=10
```

10 devices is a conservative ceiling that leaves headroom for TLS, MQTT, and WebSocket buffers without fragmentation pressure. If you have a no-PSRAM board and never expect to manage more than a handful of detectors, the default settings are otherwise correct — no further reconfiguration is required.

!!! warning "Boards without PSRAM are not the supported target"
    The default environment (`seeed-xiao-esp32s3`) and the `esp32-s3-devkitc-1-n8r2` variant both have PSRAM. The no-PSRAM `esp32-s3-devkitc-1` environment is kept building and running for development boards that lack PSRAM, but the project is designed around the PSRAM-backed memory model and the full 50-device design target requires PSRAM.

## :tabler-bug: Diagnosing memory pressure

The **System Status** page (see [System :material-arrow-right-thin:](../setup/system.md)) shows internal RAM and PSRAM usage in real time. The two numbers to watch are:

- **Memory free** — total free internal RAM. Should stay above ~40 KB at the design limit.
- **Max alloc** — largest free contiguous internal block. If this drops below ~10 KB the next TLS handshake or large JSON parse may fail even when total free still looks healthy.

A boot-time heap snapshot is logged by `GeniusDevicesService::begin()` for the same numbers plus PSRAM equivalents:

```
heap after begin(): devices=50  internal_free=61124 (largest=35344)
                    psram_free=8367104 (largest=8290304)  total_free=8428228
```

If `largest` in internal RAM falls below the working set of whatever's failing (typically TLS at ~10 KB), the issue is fragmentation rather than total free memory — a hint that something growth-bound is still landing in internal RAM and should be inspected.
