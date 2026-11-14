// Host-side unit tests for src/GeniusMqttTree.h (generic MQTT value tree: values, change detection, throttling).
// Build and run: tests/native/run.ps1

#include <GeniusMqttTree.h>

#include <cstdio>
#include <map>

using namespace genius_tree;
using genius_rt::FaultState;

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

static const uint64_t S = 1000ULL; // one second in ms
static const uint64_t MIN = 60 * S;

static DeviceSnapshot dev(uint32_t id = 7)
{
    DeviceSnapshot d;
    d.id = id;
    d.location = "1.OG links Flur";
    d.detectorSn = 2191690440u;
    d.radioSn = 2208808031u;
    d.lineId = 2736445707u;
    return d;
}

static std::map<std::string, std::string> asMap(const std::vector<Message> &m)
{
    std::map<std::string, std::string> r;
    for (const auto &x : m)
        r[x.topic] = x.payload;
    return r;
}

static bool has(const std::map<std::string, std::string> &m, const std::string &t, const std::string &v)
{
    auto it = m.find(t);
    return it != m.end() && it->second == v;
}

static void testRoot()
{
    CHECK(rootTopic("", "aabbcc") == "genius-gateway/aabbcc");
    CHECK(rootTopic("home/smoke/", "aabbcc") == "home/smoke");
    CHECK(rootTopic("custom", "aabbcc") == "custom");
}

static void testValues()
{
    DeviceSnapshot d = dev();
    auto v = deviceValues(d);
    CHECK(v[T_ALARM].present && v[T_ALARM].text == "false");
    CHECK(v[T_LOCATION].text == "1.OG links Flur");
    CHECK(v[T_SERIAL_DETECTOR].text == "2191690440");
    CHECK(v[T_SERIAL_RADIO].text == "2208808031");
    CHECK(v[T_LINE_ID].text == "2736445707");
    // unknown: no payload
    CHECK(!v[T_LAST_SEEN].present && !v[T_RSSI].present && !v[T_RSSI_SOURCE].present);
    CHECK(!v[T_BATTERY_LOW].present && !v[T_DEVICE_FAULT].present && !v[T_RADIO_FAULT].present && !v[T_READOUT_TIME].present);
    CHECK(v[T_LINK_FAULT].text == "false" && v[T_LINK_FAULT_STATE].text == "none" && v[T_LINK_FAULT_ACK].text == "false");
    CHECK(!v[T_LINK_FAULT_SINCE].present && !v[T_LINK_FAULT_CAUSE].present);

    // a device without location / serials / radio module
    DeviceSnapshot bare;
    bare.id = 9;
    auto b = deviceValues(bare);
    CHECK(!b[T_LOCATION].present && !b[T_SERIAL_DETECTOR].present && !b[T_SERIAL_RADIO].present && !b[T_LINE_ID].present);

    // last seen needs a valid wall clock
    d.hasSeen = true;
    d.seenWall = 0;
    CHECK(!deviceValues(d)[T_LAST_SEEN].present);
    d.seenWall = 1794628103;
    CHECK(deviceValues(d)[T_LAST_SEEN].text == "1794628103");

    // RSSI: live wins over the range test; a range test without an answer is unknown
    d.hasProbe = true;
    d.probeRssi = -80;
    v = deviceValues(d);
    CHECK(v[T_RSSI].text == "-80" && v[T_RSSI_SOURCE].text == "probe");
    d.hasLiveRssi = true;
    d.liveRssi = -78;
    v = deviceValues(d);
    CHECK(v[T_RSSI].text == "-78" && v[T_RSSI_SOURCE].text == "live");
    d.hasLiveRssi = false;
    d.probeRssi = 0;
    v = deviceValues(d);
    CHECK(!v[T_RSSI].present && !v[T_RSSI_SOURCE].present);

    // readout flags only after an acoustic readout
    d.hasReadout = true;
    d.batteryLow = true;
    d.readoutWall = 1700000000;
    v = deviceValues(d);
    CHECK(v[T_BATTERY_LOW].text == "true" && v[T_DEVICE_FAULT].text == "false" && v[T_RADIO_FAULT].text == "false");
    CHECK(v[T_READOUT_TIME].text == "1700000000");
}

static void testFaultValues()
{
    DeviceSnapshot d = dev();
    d.faultState = FaultState::Warning;
    d.faultSinceWall = 100;
    d.causeSn = 2208811275u;
    auto v = deviceValues(d);
    CHECK(v[T_LINK_FAULT].text == "true" && v[T_LINK_FAULT_STATE].text == "warning" && v[T_LINK_FAULT_ACK].text == "false");
    CHECK(v[T_LINK_FAULT_SINCE].text == "100" && v[T_LINK_FAULT_CAUSE].text == "2208811275");

    d.faultState = FaultState::Active;
    CHECK(deviceValues(d)[T_LINK_FAULT_STATE].text == "active");

    d.faultState = FaultState::Acked;
    v = deviceValues(d);
    CHECK(v[T_LINK_FAULT].text == "true" && v[T_LINK_FAULT_STATE].text == "acked" && v[T_LINK_FAULT_ACK].text == "true");

    // acknowledgement without a known start: no "since"
    d.faultStartUnknown = true;
    CHECK(!deviceValues(d)[T_LINK_FAULT_SINCE].present);

    // after the fault expired everything is back to "none" and the cause is gone
    d.faultState = FaultState::None;
    d.faultStartUnknown = false;
    v = deviceValues(d);
    CHECK(v[T_LINK_FAULT].text == "false" && v[T_LINK_FAULT_STATE].text == "none");
    CHECK(!v[T_LINK_FAULT_SINCE].present && !v[T_LINK_FAULT_CAUSE].present);
}

static void testFirstPublishAndChangeOnly()
{
    Tree t;
    Globals g;
    bool deferred = false;
    uint64_t now = 100 * S;

    auto m = asMap(t.plan({dev()}, g, now, false, &deferred));
    // unknown values are not announced, known ones are
    CHECK(has(m, "devices/7/alarm", "false"));
    CHECK(has(m, "devices/7/location", "1.OG links Flur"));
    CHECK(has(m, "devices/7/serial/detector", "2191690440"));
    CHECK(has(m, "devices/7/link_fault", "false"));
    CHECK(m.find("devices/7/rssi") == m.end() && m.find("devices/7/last_seen") == m.end());
    CHECK(has(m, "alarm", "false") && has(m, "alarming_count", "0") && has(m, "fault_count", "0"));
    CHECK(!deferred);

    // nothing changed: nothing is sent
    CHECK(t.plan({dev()}, g, now + S, false, &deferred).empty());

    // an alarm goes out at once
    DeviceSnapshot d = dev();
    d.alarming = true;
    g.alarming = true;
    g.alarmingCount = 1;
    m = asMap(t.plan({d}, g, now + 2 * S, false));
    CHECK(m.size() == 5 && has(m, "devices/7/alarm", "true") && has(m, "alarm", "true") && has(m, "alarming_count", "1"));
    CHECK(has(m, "lines/2736445707/alarm", "true") && has(m, "lines/2736445707/alarming_count", "1"));
}

static void testThrottle()
{
    Tree t;
    Globals g;
    bool deferred = false;
    DeviceSnapshot d = dev();
    d.hasSeen = true;
    d.seenWall = 1000;
    d.hasLiveRssi = true;
    d.liveRssi = -80;
    uint64_t now = 0;
    auto m = asMap(t.plan({d}, g, now, false));
    CHECK(has(m, "devices/7/last_seen", "1000") && has(m, "devices/7/rssi", "-80") && has(m, "devices/7/rssi_source", "live"));

    // last seen moves on every frame, but is only sent once a minute
    d.seenWall = 1010;
    CHECK(t.plan({d}, g, now + 10 * S, false, &deferred).empty());
    CHECK(deferred);
    m = asMap(t.plan({d}, g, now + MIN, false, &deferred));
    CHECK(has(m, "devices/7/last_seen", "1010") && !deferred);

    // RSSI: 2 dB is noise, 3 dB is sent at once
    d.liveRssi = -82;
    CHECK(t.plan({d}, g, now + MIN + S, false, &deferred).empty());
    CHECK(deferred);
    d.liveRssi = -83;
    m = asMap(t.plan({d}, g, now + MIN + 2 * S, false));
    CHECK(has(m, "devices/7/rssi", "-83"));

    // a small change is sent after 10 minutes at the latest
    d.liveRssi = -84;
    CHECK(t.plan({d}, g, now + MIN + 3 * S, false, &deferred).empty() && deferred);
    m = asMap(t.plan({d}, g, now + MIN + 2 * S + 10 * MIN, false, &deferred));
    CHECK(has(m, "devices/7/rssi", "-84") && !deferred);
}

static void testClearsUnknownValues()
{
    Tree t;
    Globals g;
    DeviceSnapshot d = dev();
    d.hasSeen = true;
    d.seenWall = 1000;
    d.hasLiveRssi = true;
    d.liveRssi = -80;
    t.plan({d}, g, 0, false);

    // the value disappears (e.g. state of a replaced module): the retained topic is cleared
    d.hasSeen = false;
    d.hasLiveRssi = false;
    auto m = asMap(t.plan({d}, g, 5 * MIN, false));
    CHECK(has(m, "devices/7/last_seen", "") && has(m, "devices/7/rssi", "") && has(m, "devices/7/rssi_source", ""));

    // a fault with cause, then expiry: cause is cleared
    d.faultState = FaultState::Active;
    d.causeSn = 2208811275u;
    d.faultSinceWall = 50;
    m = asMap(t.plan({d}, g, 6 * MIN, false));
    CHECK(has(m, "devices/7/link_fault", "true") && has(m, "devices/7/link_fault_cause", "2208811275"));
    d.faultState = FaultState::None;
    d.causeSn = 0;
    m = asMap(t.plan({d}, g, 7 * MIN, false));
    CHECK(has(m, "devices/7/link_fault", "false") && has(m, "devices/7/link_fault_cause", "") && has(m, "devices/7/link_fault_since", ""));
}

static void testForce()
{
    Tree t;
    Globals g;
    auto first = t.plan({dev(1), dev(2)}, g, 0, false);
    CHECK(!first.empty());

    // after (re)connect every topic is sent again, the unknown ones as clears, so no stale retained value survives
    auto m = t.plan({dev(1), dev(2)}, g, S, true);
    CHECK(m.size() == 2 * T_COUNT + Tree::GLOBAL_COUNT + L_COUNT); // both detectors share one alarm line
    auto mm = asMap(m);
    CHECK(has(mm, "devices/1/last_seen", "") && has(mm, "devices/2/rssi", ""));
    CHECK(has(mm, "devices/2/alarm", "false"));
}

static void testRemovedDevices()
{
    Tree t;
    Globals g;
    t.plan({dev(1), dev(2)}, g, 0, false);

    // device 2 is deleted: everything published for it is cleared (only the topics that had a value)
    auto m = asMap(t.plan({dev(1)}, g, S, false));
    CHECK(has(m, "devices/2/alarm", "") && has(m, "devices/2/location", "") && has(m, "devices/2/link_fault", ""));
    CHECK(m.find("devices/2/rssi") == m.end()); // never had a value
    CHECK(m.find("devices/1/alarm") == m.end());
    CHECK(has(m, "lines/2736445707/device_count", "1")); // the line summary follows
    CHECK(t.plan({dev(1)}, g, 2 * S, false).empty());
}

static void testClearAll()
{
    Tree t;
    Globals g;
    t.plan({dev(1)}, g, 0, false);
    auto m = asMap(t.clearAll());
    CHECK(has(m, "devices/1/alarm", "") && has(m, "alarm", "") && has(m, "fault_count", ""));
    CHECK(has(m, "lines/2736445707/device_count", "") && has(m, "lines/2736445707/alarm", ""));
    CHECK(t.empty());
    CHECK(t.clearAll().empty());
    // after clearing, publishing starts from scratch
    CHECK(!t.plan({dev(1)}, g, S, false).empty());
}

static void testLines()
{
    const uint32_t L1 = 2736445707u, L2 = 1234u;
    DeviceSnapshot a = dev(1), b = dev(2), c = dev(3), none_ = dev(4), bc = dev(5);
    a.lineId = L1;
    b.lineId = L1;
    c.lineId = L2;
    none_.lineId = 0;          // detector without a line
    bc.lineId = 0xFFFFFFFFu;   // broadcast is not a line
    b.alarming = true;
    b.faultState = FaultState::Active;
    c.faultState = FaultState::Warning;

    // aggregation
    auto sum = summarizeLines({a, b, c, none_, bc}, {{L1, "Erdgeschoss"}});
    CHECK(sum.size() == 2);
    CHECK(sum[0].id == L2 && sum[0].devices == 1 && sum[0].faults == 1 && sum[0].alarming == 0 && sum[0].name.empty());
    CHECK(sum[1].id == L1 && sum[1].devices == 2 && sum[1].alarming == 1 && sum[1].faults == 1 && sum[1].name == "Erdgeschoss");
    // a configured line without detectors still exists, the broadcast and the null line never do
    sum = summarizeLines({a}, {{L2, "Keller"}, {0, "x"}, {0xFFFFFFFFu, "Broadcast"}});
    CHECK(sum.size() == 2 && sum[0].id == L2 && sum[0].devices == 0 && sum[0].name == "Keller");

    Tree t;
    Globals g;
    auto m = asMap(t.plan({a, b, c, none_}, {{L1, "Erdgeschoss"}, {L2, "Keller"}}, g, 0, false));
    CHECK(has(m, "lines/2736445707/name", "Erdgeschoss") && has(m, "lines/1234/name", "Keller"));
    CHECK(has(m, "lines/2736445707/device_count", "2") && has(m, "lines/2736445707/alarm", "true"));
    CHECK(has(m, "lines/2736445707/alarming_count", "1") && has(m, "lines/2736445707/fault_count", "1"));
    CHECK(has(m, "lines/1234/alarm", "false") && has(m, "lines/1234/fault_count", "1") && has(m, "lines/1234/device_count", "1"));
    CHECK(m.find("lines/0/name") == m.end()); // the detector without a line belongs to no line

    // renaming a line: only the name goes out
    m = asMap(t.plan({a, b, c, none_}, {{L1, "Obergeschoss"}, {L2, "Keller"}}, g, S, false));
    CHECK(m.size() == 1 && has(m, "lines/2736445707/name", "Obergeschoss"));

    // the alarm of the line ends
    b.alarming = false;
    m = asMap(t.plan({a, b, c, none_}, {{L1, "Obergeschoss"}, {L2, "Keller"}}, g, 2 * S, false));
    CHECK(has(m, "lines/2736445707/alarm", "false") && has(m, "lines/2736445707/alarming_count", "0"));

    // a detector moves to another line: both summaries change, the detector keeps its topics
    c.lineId = L1;
    m = asMap(t.plan({a, b, c, none_}, {{L1, "Obergeschoss"}, {L2, "Keller"}}, g, 3 * S, false));
    CHECK(has(m, "lines/2736445707/device_count", "3") && has(m, "lines/1234/device_count", "0"));
    CHECK(has(m, "devices/3/line_id", "2736445707"));

    // a line without detectors that is no longer configured disappears: its topics are cleared
    m = asMap(t.plan({a, b, c, none_}, {{L1, "Obergeschoss"}}, g, 4 * S, false));
    CHECK(has(m, "lines/1234/name", "") && has(m, "lines/1234/device_count", "") && has(m, "lines/1234/alarm", ""));
    CHECK(t.plan({a, b, c, none_}, {{L1, "Obergeschoss"}}, g, 5 * S, false).empty());
}

int main()
{
    testRoot();
    testValues();
    testFaultValues();
    testFirstPublishAndChangeOnly();
    testThrottle();
    testClearsUnknownValues();
    testForce();
    testRemovedDevices();
    testClearAll();
    testLines();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
