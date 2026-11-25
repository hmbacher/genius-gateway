// Host-side unit tests for src/GeniusMqttTree.h (MQTT value tree: values, change detection, throttling).
// Build and run: tests/native/run.ps1, or with g++:
//   g++ -std=c++17 -Wall -Wextra -I src tests/native/test_tree.cpp -o test_tree && ./test_tree

#include <GeniusMqttTree.h>

#include <cstdio>
#include <map>

using namespace genius_tree;

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
static const uint32_t LINE = 2119672224u;
static const std::vector<LineName> NO_NAMES;

static DeviceSnapshot dev(uint32_t id = 7)
{
    DeviceSnapshot d;
    d.id = id;
    d.location = "Hallway";
    d.detectorSn = 80900201u;
    d.radioSn = 70900201u;
    d.lineId = LINE;
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
    CHECK(v[T_LOCATION].text == "Hallway");
    CHECK(v[T_SERIAL_DETECTOR].text == "80900201");
    CHECK(v[T_SERIAL_RADIO].text == "70900201");
    CHECK(v[T_LINE_ID].text == "2119672224");
    // not known: no payload
    CHECK(!v[T_LAST_SEEN].present && !v[T_RSSI].present);
    CHECK(!v[T_BATTERY_LOW].present && !v[T_DEVICE_FAULT].present && !v[T_RADIO_FAULT].present && !v[T_DISMOUNTED].present);
    // the gateway's own state is always known
    CHECK(v[T_REPORTED_MISSING].present && v[T_REPORTED_MISSING].text == "false");

    // a device without location / serials / radio module
    DeviceSnapshot bare;
    bare.id = 9;
    auto b = deviceValues(bare);
    CHECK(!b[T_LOCATION].present && !b[T_SERIAL_DETECTOR].present && !b[T_SERIAL_RADIO].present && !b[T_LINE_ID].present);

    // last seen and signal of a supervised radio module, once it was heard
    d.lastSeen = "2026-11-24T18:33:40.000Z";
    v = deviceValues(d);
    CHECK(v[T_LAST_SEEN].text == "2026-11-24T18:33:40.000Z" && !v[T_RSSI].present); // heard through another module: no signal value
    d.hasRssi = true;
    d.rssi = -78;
    CHECK(deviceValues(d)[T_RSSI].text == "-78");
}

static void testStatusValues()
{
    DeviceSnapshot d = dev();
    CHECK(!d.faulty());

    // a readout reports battery, device and radio link as OK: known, not active
    d.knownFlags = GENIUS_STATUS_BATT_LOW | GENIUS_STATUS_ERROR | GENIUS_STATUS_RADIO;
    auto v = deviceValues(d);
    CHECK(v[T_BATTERY_LOW].text == "false" && v[T_DEVICE_FAULT].text == "false" && v[T_RADIO_FAULT].text == "false");
    CHECK(!v[T_DISMOUNTED].present); // never reported: still not known
    CHECK(!d.faulty());

    // a battery warning over the air
    d.activeFlags = GENIUS_STATUS_BATT_LOW;
    v = deviceValues(d);
    CHECK(v[T_BATTERY_LOW].text == "true" && v[T_DEVICE_FAULT].text == "false" && d.faulty());

    // dismounted and radio link fault
    d.knownFlags |= GENIUS_STATUS_DISMOUNT;
    d.activeFlags = GENIUS_STATUS_DISMOUNT | GENIUS_STATUS_RADIO;
    v = deviceValues(d);
    CHECK(v[T_DISMOUNTED].text == "true" && v[T_RADIO_FAULT].text == "true" && v[T_BATTERY_LOW].text == "false");

    // reported missing is a fault of its own, not the radio link flag
    DeviceSnapshot m = dev();
    m.knownFlags = m.activeFlags = GENIUS_STATUS_REPORTED_MISSING;
    v = deviceValues(m);
    CHECK(v[T_REPORTED_MISSING].text == "true" && !v[T_RADIO_FAULT].present && m.faulty());

    // a muted fault is not a fault by itself
    DeviceSnapshot q = dev();
    q.knownFlags = q.activeFlags = GENIUS_STATUS_MUTE;
    CHECK(!q.faulty());
}

static void testFirstPublishAndChangeOnly()
{
    Tree t;
    Globals g;
    bool deferred = false;
    uint64_t now = 100 * S;

    auto m = asMap(t.plan({dev()}, NO_NAMES, g, now, false, &deferred));
    // values that are not known are not announced, known ones are
    CHECK(has(m, "devices/7/alarm", "false"));
    CHECK(has(m, "devices/7/location", "Hallway"));
    CHECK(has(m, "devices/7/serial/detector", "80900201"));
    CHECK(has(m, "devices/7/reported_missing", "false"));
    CHECK(m.find("devices/7/rssi") == m.end() && m.find("devices/7/last_seen") == m.end() && m.find("devices/7/battery_low") == m.end());
    CHECK(has(m, "alarm", "false") && has(m, "alarming_count", "0") && has(m, "fault_count", "0"));
    CHECK(!deferred);

    // nothing changed: nothing is sent
    CHECK(t.plan({dev()}, NO_NAMES, g, now + S, false, &deferred).empty());

    // an alarm goes out at once
    DeviceSnapshot d = dev();
    d.alarming = true;
    g.alarming = true;
    g.alarmingCount = 1;
    m = asMap(t.plan({d}, NO_NAMES, g, now + 2 * S, false));
    CHECK(m.size() == 5 && has(m, "devices/7/alarm", "true") && has(m, "alarm", "true") && has(m, "alarming_count", "1"));
    CHECK(has(m, "lines/2119672224/alarm", "true") && has(m, "lines/2119672224/alarming_count", "1"));

    // a status flag becomes known and active: one topic of the device, the fault counters
    d.knownFlags = d.activeFlags = GENIUS_STATUS_BATT_LOW;
    g.faultCount = 1;
    m = asMap(t.plan({d}, NO_NAMES, g, now + 3 * S, false));
    CHECK(m.size() == 3 && has(m, "devices/7/battery_low", "true") && has(m, "fault_count", "1") && has(m, "lines/2119672224/fault_count", "1"));
}

static void testThrottle()
{
    Tree t;
    Globals g;
    bool deferred = false;
    DeviceSnapshot d = dev();
    d.lastSeen = "2026-11-24T18:33:40.000Z";
    d.hasRssi = true;
    d.rssi = -80;
    uint64_t now = 0;
    auto m = asMap(t.plan({d}, NO_NAMES, g, now, false));
    CHECK(has(m, "devices/7/last_seen", "2026-11-24T18:33:40.000Z") && has(m, "devices/7/rssi", "-80"));

    // last seen moves on every frame, but is only sent once a minute
    d.lastSeen = "2026-11-24T18:33:50.000Z";
    CHECK(t.plan({d}, NO_NAMES, g, now + 10 * S, false, &deferred).empty());
    CHECK(deferred);
    m = asMap(t.plan({d}, NO_NAMES, g, now + MIN, false, &deferred));
    CHECK(has(m, "devices/7/last_seen", "2026-11-24T18:33:50.000Z") && !deferred);

    // RSSI: 2 dB is noise, 3 dB is sent at once
    d.rssi = -82;
    CHECK(t.plan({d}, NO_NAMES, g, now + MIN + S, false, &deferred).empty());
    CHECK(deferred);
    d.rssi = -83;
    m = asMap(t.plan({d}, NO_NAMES, g, now + MIN + 2 * S, false));
    CHECK(has(m, "devices/7/rssi", "-83"));

    // a small change is sent after 10 minutes at the latest
    d.rssi = -84;
    CHECK(t.plan({d}, NO_NAMES, g, now + MIN + 3 * S, false, &deferred).empty() && deferred);
    m = asMap(t.plan({d}, NO_NAMES, g, now + MIN + 2 * S + 10 * MIN, false, &deferred));
    CHECK(has(m, "devices/7/rssi", "-84") && !deferred);
}

static void testClearsUnknownValues()
{
    Tree t;
    Globals g;
    DeviceSnapshot d = dev();
    d.lastSeen = "2026-11-24T18:33:40.000Z";
    d.hasRssi = true;
    d.rssi = -80;
    d.knownFlags = GENIUS_STATUS_BATT_LOW | GENIUS_STATUS_RADIO;
    d.activeFlags = GENIUS_STATUS_BATT_LOW;
    t.plan({d}, NO_NAMES, g, 0, false);

    // the values disappear (gateway restarted: nothing heard yet): the retained topics are cleared
    d.lastSeen.clear();
    d.hasRssi = false;
    auto m = asMap(t.plan({d}, NO_NAMES, g, 5 * MIN, false));
    CHECK(has(m, "devices/7/last_seen", "") && has(m, "devices/7/rssi", ""));

    // the status is reset: an active flag becomes unknown again, a flag reported OK stays
    d.knownFlags = GENIUS_STATUS_RADIO;
    d.activeFlags = 0;
    m = asMap(t.plan({d}, NO_NAMES, g, 6 * MIN, false));
    // (the line no longer counts the detector as faulty)
    CHECK(m.size() == 2 && has(m, "devices/7/battery_low", "") && has(m, "lines/2119672224/fault_count", "0"));
}

static void testForce()
{
    Tree t;
    Globals g;
    auto first = t.plan({dev(1), dev(2)}, NO_NAMES, g, 0, false);
    CHECK(!first.empty());

    // after (re)connect every topic is sent again, the unknown ones as clears, so no stale retained value survives
    auto m = t.plan({dev(1), dev(2)}, NO_NAMES, g, S, true);
    CHECK(m.size() == 2 * T_COUNT + Tree::GLOBAL_COUNT + L_COUNT); // both detectors share one alarm line
    auto mm = asMap(m);
    CHECK(has(mm, "devices/1/last_seen", "") && has(mm, "devices/2/rssi", "") && has(mm, "devices/2/battery_low", ""));
    CHECK(has(mm, "devices/2/alarm", "false"));
}

static void testRemovedDevices()
{
    Tree t;
    Globals g;
    t.plan({dev(1), dev(2)}, NO_NAMES, g, 0, false);

    // device 2 is deleted: everything published for it is cleared (only the topics that had a value)
    auto m = asMap(t.plan({dev(1)}, NO_NAMES, g, S, false));
    CHECK(has(m, "devices/2/alarm", "") && has(m, "devices/2/location", "") && has(m, "devices/2/reported_missing", ""));
    CHECK(m.find("devices/2/rssi") == m.end()); // never had a value
    CHECK(m.find("devices/1/alarm") == m.end());
    CHECK(has(m, "lines/2119672224/device_count", "1")); // the line summary follows
    CHECK(t.plan({dev(1)}, NO_NAMES, g, 2 * S, false).empty());
}

static void testClearAll()
{
    Tree t;
    Globals g;
    t.plan({dev(1)}, NO_NAMES, g, 0, false);
    auto m = asMap(t.clearAll());
    CHECK(has(m, "devices/1/alarm", "") && has(m, "alarm", "") && has(m, "fault_count", ""));
    CHECK(has(m, "lines/2119672224/device_count", "") && has(m, "lines/2119672224/alarm", ""));
    CHECK(t.empty());
    CHECK(t.clearAll().empty());
    // after clearing, publishing starts from scratch
    CHECK(!t.plan({dev(1)}, NO_NAMES, g, S, false).empty());
}

static void testLines()
{
    const uint32_t L1 = LINE, L2 = 1234u;
    DeviceSnapshot a = dev(1), b = dev(2), c = dev(3), none_ = dev(4), bc = dev(5);
    a.lineId = L1;
    b.lineId = L1;
    c.lineId = L2;
    none_.lineId = 0;        // detector without a line
    bc.lineId = 0xFFFFFFFFu; // broadcast is not a line
    b.alarming = true;
    b.knownFlags = b.activeFlags = GENIUS_STATUS_RADIO;
    c.knownFlags = c.activeFlags = GENIUS_STATUS_REPORTED_MISSING;

    // aggregation
    auto sum = summarizeLines({a, b, c, none_, bc}, {{L1, "Ground floor"}});
    CHECK(sum.size() == 2);
    CHECK(sum[0].id == L2 && sum[0].devices == 1 && sum[0].faults == 1 && sum[0].alarming == 0 && sum[0].name.empty());
    CHECK(sum[1].id == L1 && sum[1].devices == 2 && sum[1].alarming == 1 && sum[1].faults == 1 && sum[1].name == "Ground floor");
    // a configured line without detectors still exists, the broadcast and the null line never do
    sum = summarizeLines({a}, {{L2, "Basement"}, {0, "x"}, {0xFFFFFFFFu, "Broadcast"}});
    CHECK(sum.size() == 2 && sum[0].id == L2 && sum[0].devices == 0 && sum[0].name == "Basement");

    Tree t;
    Globals g;
    auto m = asMap(t.plan({a, b, c, none_}, {{L1, "Ground floor"}, {L2, "Basement"}}, g, 0, false));
    CHECK(has(m, "lines/2119672224/name", "Ground floor") && has(m, "lines/1234/name", "Basement"));
    CHECK(has(m, "lines/2119672224/device_count", "2") && has(m, "lines/2119672224/alarm", "true"));
    CHECK(has(m, "lines/2119672224/alarming_count", "1") && has(m, "lines/2119672224/fault_count", "1"));
    CHECK(has(m, "lines/1234/alarm", "false") && has(m, "lines/1234/fault_count", "1") && has(m, "lines/1234/device_count", "1"));
    CHECK(m.find("lines/0/name") == m.end()); // the detector without a line belongs to no line

    // renaming a line: only the name goes out
    m = asMap(t.plan({a, b, c, none_}, {{L1, "First floor"}, {L2, "Basement"}}, g, S, false));
    CHECK(m.size() == 1 && has(m, "lines/2119672224/name", "First floor"));

    // the alarm of the line ends
    b.alarming = false;
    m = asMap(t.plan({a, b, c, none_}, {{L1, "First floor"}, {L2, "Basement"}}, g, 2 * S, false));
    CHECK(has(m, "lines/2119672224/alarm", "false") && has(m, "lines/2119672224/alarming_count", "0"));

    // a detector moves to another line: both summaries change, the detector keeps its topics
    c.lineId = L1;
    m = asMap(t.plan({a, b, c, none_}, {{L1, "First floor"}, {L2, "Basement"}}, g, 3 * S, false));
    CHECK(has(m, "lines/2119672224/device_count", "3") && has(m, "lines/1234/device_count", "0"));
    CHECK(has(m, "devices/3/line_id", "2119672224"));

    // a line without detectors that is no longer configured disappears: its topics are cleared
    m = asMap(t.plan({a, b, c, none_}, {{L1, "First floor"}}, g, 4 * S, false));
    CHECK(has(m, "lines/1234/name", "") && has(m, "lines/1234/device_count", "") && has(m, "lines/1234/alarm", ""));
    CHECK(t.plan({a, b, c, none_}, {{L1, "First floor"}}, g, 5 * S, false).empty());
}

int main()
{
    testRoot();
    testValues();
    testStatusValues();
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
