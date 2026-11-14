// Host-side unit tests for src/GeniusDeviceRuntime.h (frame classification + RAM-only runtime state).
//
// Build and run (MSVC, from a "x64 Native Tools" prompt or via tests/native/run.ps1):
//   cl /nologo /std:c++17 /EHsc /W3 /I ..\..\src test_runtime.cpp && test_runtime.exe
// or with any C++17 compiler:  g++ -std=c++17 -I../../src test_runtime.cpp -o test_runtime && ./test_runtime

#include <GeniusDeviceRuntime.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace genius_rt;

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                                   \
    do                                                                                \
    {                                                                                 \
        g_checks++;                                                                   \
        if (!(cond))                                                                  \
        {                                                                             \
            g_failed++;                                                               \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);               \
        }                                                                             \
    } while (0)

static std::vector<uint8_t> fromHex(const std::string &h)
{
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back((uint8_t)std::stoul(h.substr(i, 2), nullptr, 16));
    return out;
}

static SupKind kindOf(const std::vector<uint8_t> &d) { return classifySupervision(d.data(), d.size()); }

/// Genius base frame (27 bytes) + type + payload, like the firmware templates.
static std::vector<uint8_t> frame(uint8_t family, uint8_t type, std::vector<uint8_t> payload = {})
{
    std::vector<uint8_t> d(28, 0);
    d[0] = 0x02;
    d[POS_FAMILY] = family;
    d[POS_TYPE] = type;
    d.insert(d.end(), payload.begin(), payload.end());
    return d;
}

// Real captures from the test line a31ad90b (2026-11-14): radio fault and its acknowledgement.
static const char *FAULT = "02cc1803ffffffff0383a7bc5f0383a7bc5fa31ad90b0f8f4800000008000000c88aa282";
static const char *ACK = "02cc1803ffffffff0383a7bc5f0383a7bc5fa31ad90b0f8f4800000010000000c88aa282";

static const uint64_t H = 3600ULL * 1000ULL;

static void testClassification()
{
    CHECK(kindOf(fromHex(FAULT)) == SupKind::RadioFault);
    CHECK(kindOf(fromHex(ACK)) == SupKind::FaultAck);
    auto f = fromHex(FAULT);
    CHECK(le32(f.data(), POS_DETECTOR_SN) == 2191690440u); // serial of the detector "SN Flur"
    CHECK(be32(f.data(), 9) == 0x83a7bc5fu);               // originating radio module

    // fire alarm (code 0x01) and unknown codes are not faults
    auto fire = fromHex(FAULT);
    fire[POS_CODE] = 0x01;
    CHECK(kindOf(fire) == SupKind::None);
    auto other = fromHex(FAULT);
    other[POS_CODE] = 0x20;
    CHECK(kindOf(other) == SupKind::None);
    // a stop flag in the same frame is an undefined combination -> ignored
    auto both = fromHex(FAULT);
    both[POS_STOP_FLAG] = 1;
    CHECK(kindOf(both) == SupKind::None);
    // a 36 byte ConfigCheckProbe response (family 0x55, type 0x08) is not an alarm-family frame
    auto cfg = frame(0x55, 0x08, std::vector<uint8_t>(8, 0));
    cfg[POS_CODE] = CODE_RADIO_FAULT;
    CHECK(kindOf(cfg) == SupKind::None);

    // supervision series and the "module missing" report, distinguished by family and length
    CHECK(kindOf(frame(FAMILY_SUPERVISION, 0x00)) == SupKind::Beacon);
    CHECK(kindOf(frame(0x66, 0x00)) == SupKind::None); // CommissioningProbe Request is not the series
    auto missing = frame(FAMILY_SUPERVISION, 0x02, {0x83, 0xa7, 0xc9, 0x0b});
    CHECK(kindOf(missing) == SupKind::Missing);
    CHECK(be32(missing.data(), POS_MISSING_SN) == 0x83a7c90bu);
    CHECK(be32(missing.data(), POS_MISSING_SN) == 2208811275u);
    CHECK(kindOf(frame(FAMILY_SUPERVISION, 0x02, {1, 2, 3})) == SupKind::None); // wrong length
    CHECK(kindOf(frame(FAMILY_SUPERVISION, 0x01)) == SupKind::None);            // 0x33/01 is not evaluated yet

    // robustness
    CHECK(classifySupervision(nullptr, 40) == SupKind::None);
    CHECK(kindOf(std::vector<uint8_t>(10, 0)) == SupKind::None);
    CHECK(kindOf(std::vector<uint8_t>(27, 0)) == SupKind::None);
}

static void testSeenAndRssi()
{
    RuntimeTable t;
    CHECK(t.find(7) == nullptr);
    CHECK(t.seen(7, 1000, 1700000000) == CHANGE_SEEN);
    CHECK(t.rssi(7, -78, 1000, 1700000000) == CHANGE_RSSI);
    const DeviceRuntime *r = t.find(7);
    CHECK(r && r->hasSeen && r->seenWall == 1700000000 && r->hasRssi && r->rssi == -78);
    t.rssi(7, -91, 2000, 1700000010);
    CHECK(t.find(7)->rssi == -91 && t.find(7)->rssiWall == 1700000010);
    CHECK(t.find(8) == nullptr);
    CHECK(t.all().size() == 1);
    // last seen and RSSI never expire (only faults do)
    t.expire(1000 + 365ULL * 24 * H);
    CHECK(t.find(7)->hasSeen && t.find(7)->hasRssi);
}

static void testFaultLifecycle()
{
    RuntimeTable t;
    const uint32_t id = 1;

    // warning (0x33/0x02) first, as observed on 2026-11-13 (9.5 h before the fault)
    CHECK(t.linkMissing(id, 0x83a7c90b, 0, 100) == CHANGE_FAULT);
    CHECK(t.find(id)->fault.state == FaultState::Warning);
    CHECK(t.find(id)->fault.causeSn == 0x83a7c90bu);
    CHECK(t.find(id)->fault.sinceWall == 100);
    // same report again: nothing to publish, but the expiry is extended
    CHECK(t.linkMissing(id, 0x83a7c90b, 1 * H, 200) == CHANGE_NONE);
    CHECK(t.find(id)->fault.lastMs == 1 * H);
    CHECK(t.find(id)->fault.sinceWall == 100);

    // fault frame turns the warning into an active fault and keeps the cause and the start
    CHECK(t.fault(id, 10 * H, 5000) == CHANGE_FAULT);
    CHECK(t.find(id)->fault.state == FaultState::Active);
    CHECK(t.find(id)->fault.causeSn == 0x83a7c90bu);
    CHECK(t.find(id)->fault.sinceWall == 100);
    CHECK(t.find(id)->fault.activeWall == 5000);
    // repeated fault frame: no state change
    CHECK(t.fault(id, 11 * H, 5100) == CHANGE_NONE);

    // acknowledgement
    CHECK(t.ack(id, 14 * H, 9000) == CHANGE_FAULT);
    CHECK(t.find(id)->fault.state == FaultState::Acked);
    CHECK(t.find(id)->fault.ackWall == 9000);
    CHECK(t.find(id)->fault.startUnknown == false);
    // repeated acknowledgement keeps the first acknowledgement time
    CHECK(t.ack(id, 14 * H + 1000, 9050) == CHANGE_NONE);
    CHECK(t.find(id)->fault.ackWall == 9000);
    // an incoming warning does not downgrade an acknowledged fault
    CHECK(t.linkMissing(id, 0x83a7c90b, 15 * H, 9100) == CHANGE_NONE);
    CHECK(t.find(id)->fault.state == FaultState::Acked);

    // the fault is signalled again after the acknowledgement -> active again, acknowledgement cleared
    CHECK(t.fault(id, 20 * H, 12000) == CHANGE_FAULT);
    CHECK(t.find(id)->fault.state == FaultState::Active);
    CHECK(t.find(id)->fault.ackWall == 0);
}

static void testAckWithoutStart()
{
    RuntimeTable t;
    CHECK(t.ack(3, 5000, 777) == CHANGE_FAULT); // e.g. after a reboot of the gateway
    const Fault &f = t.find(3)->fault;
    CHECK(f.state == FaultState::Acked);
    CHECK(f.startUnknown);
    CHECK(f.ackWall == 777);
    CHECK(f.causeSn == 0);
    // a fault frame without any earlier warning
    RuntimeTable u;
    CHECK(u.fault(4, 0, 50) == CHANGE_FAULT);
    CHECK(u.find(4)->fault.state == FaultState::Active && u.find(4)->fault.causeSn == 0);
    CHECK(u.find(4)->fault.sinceWall == 50 && u.find(4)->fault.activeWall == 50);
}

static void testExpiry()
{
    RuntimeTable t;
    t.fault(1, 0, 10);
    t.linkMissing(2, 0x83a7c90b, 5 * H, 10);
    t.seen(3, 0, 10);

    // exactly 48 h after the last frame is still within the time limit
    CHECK(t.expire(FAULT_TTL_MS).empty());
    CHECK(t.find(1)->fault.state == FaultState::Active);

    // one millisecond later device 1 loses its fault; device 2 (last frame 5 h later) keeps its warning
    auto ch = t.expire(FAULT_TTL_MS + 1);
    CHECK(ch.size() == 1 && ch[0] == 1);
    CHECK(t.find(1)->fault.state == FaultState::None);
    CHECK(t.find(1)->fault.causeSn == 0);
    CHECK(t.find(2)->fault.state == FaultState::Warning);

    // a repeated frame restarts the 48 h
    t.fault(2, 40 * H, 20);
    CHECK(t.expire(5 * H + FAULT_TTL_MS + 10).empty());
    CHECK(t.find(2)->fault.state == FaultState::Active);
    CHECK(t.expire(40 * H + FAULT_TTL_MS + 1).size() == 1);
    CHECK(t.find(2)->fault.state == FaultState::None);

    // a device without fault is never reported, a clock that went backwards does not expire anything
    CHECK(t.expire(FAULT_TTL_MS * 10).empty());
    t.fault(5, 1000 * H, 1);
    CHECK(t.expire(10).empty());
    CHECK(t.find(5)->fault.state == FaultState::Active);

    // the same episode can start again after the expiry
    CHECK(t.fault(1, FAULT_TTL_MS * 3, 99) == CHANGE_FAULT);
    CHECK(t.find(1)->fault.sinceWall == 99);
}

static void testRemove()
{
    RuntimeTable t;
    t.seen(1, 0, 1);
    t.seen(2, 0, 1);
    t.remove(1);
    CHECK(t.find(1) == nullptr && t.find(2) != nullptr && t.all().size() == 1);
    t.remove(42); // unknown id is harmless
    t.clear();
    CHECK(t.all().empty());
}

int main()
{
    testClassification();
    testSeenAndRssi();
    testFaultLifecycle();
    testAckWithoutStart();
    testExpiry();
    testRemove();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
