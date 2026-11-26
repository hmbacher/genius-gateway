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

static void testFirstValueIsNotThrottled()
{
    Tree t;
    Globals g;
    bool deferred = false;
    DeviceSnapshot d = dev();
    t.plan({d}, NO_NAMES, g, 0, false); // the detector is known, its module was not heard yet

    // heard for the first time a few seconds later: both values appear at once
    d.lastSeen = "2026-11-24T18:33:40.000Z";
    d.hasRssi = true;
    d.rssi = -2; // closer to 0 than the change threshold
    auto m = asMap(t.plan({d}, NO_NAMES, g, 5 * S, false, &deferred));
    CHECK(m.size() == 2 && has(m, "devices/7/last_seen", "2026-11-24T18:33:40.000Z") && has(m, "devices/7/rssi", "-2"));
    CHECK(!deferred);

    // the value after it is spaced out as usual
    d.lastSeen = "2026-11-24T18:33:50.000Z";
    CHECK(t.plan({d}, NO_NAMES, g, 15 * S, false, &deferred).empty() && deferred);

    // values that disappear are cleared at once
    d.lastSeen.clear();
    d.hasRssi = false;
    m = asMap(t.plan({d}, NO_NAMES, g, 20 * S, false, &deferred));
    CHECK(m.size() == 2 && has(m, "devices/7/last_seen", "") && has(m, "devices/7/rssi", "") && !deferred);

    // the same after a forced publish (MQTT connect), which sends the empty topics as clears
    Tree f;
    DeviceSnapshot e = dev();
    f.plan({e}, NO_NAMES, g, 0, true);
    e.lastSeen = "2026-11-24T18:33:40.000Z";
    m = asMap(f.plan({e}, NO_NAMES, g, 5 * S, false, &deferred));
    CHECK(m.size() == 1 && has(m, "devices/7/last_seen", "2026-11-24T18:33:40.000Z") && !deferred);
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

static void testSweep()
{
    bool isLine = true;
    uint32_t id = 0;
    // the tree's own topics
    CHECK(ownTopic("devices/7/alarm", isLine, id) && !isLine && id == 7);
    CHECK(ownTopic("devices/900201/serial/detector", isLine, id) && !isLine && id == 900201);
    CHECK(ownTopic("devices/4294967295/rssi", isLine, id) && id == 4294967295u);
    CHECK(ownTopic("lines/2119672224/device_count", isLine, id) && isLine && id == 2119672224u);
    CHECK(ownTopic("devices/0/alarm", isLine, id) && id == 0);
    // everything else is somebody else's
    CHECK(!ownTopic("alarm", isLine, id) && !ownTopic("alarming_count", isLine, id));
    CHECK(!ownTopic("devices/7/custom", isLine, id) && !ownTopic("devices/7/serial", isLine, id));
    CHECK(!ownTopic("devices/7/alarm/extra", isLine, id) && !ownTopic("devices/7", isLine, id) && !ownTopic("devices//alarm", isLine, id));
    CHECK(!ownTopic("devices/abc/alarm", isLine, id) && !ownTopic("devices/7a/alarm", isLine, id) && !ownTopic("devices/-7/alarm", isLine, id));
    CHECK(!ownTopic("devices/007/alarm", isLine, id)); // not written by the tree
    CHECK(!ownTopic("devices/4294967296/alarm", isLine, id) && !ownTopic("devices/12345678901/alarm", isLine, id));
    CHECK(!ownTopic("lines/7/location", isLine, id) && !ownTopic("devices/7/name", isLine, id)); // suffix of the other kind
    CHECK(!ownTopic("status", isLine, id) && !ownTopic("device/7/alarm", isLine, id) && !ownTopic("", isLine, id));

    // the tree knows detectors 1 and 2 on one line; the broker also has topics of detector 3 and of another line
    Tree t;
    Globals g;
    t.plan({dev(1), dev(2)}, NO_NAMES, g, 0, true);
    auto m = asMap(t.sweep({1, 2, 3}, {2119672224u, 1234}));
    CHECK(m.size() == (size_t)T_COUNT + L_COUNT);
    CHECK(has(m, "devices/3/alarm", "") && has(m, "devices/3/serial/radio", "") && has(m, "devices/3/rssi", ""));
    CHECK(has(m, "lines/1234/name", "") && has(m, "lines/1234/device_count", ""));
    CHECK(m.find("devices/1/alarm") == m.end() && m.find("lines/2119672224/name") == m.end());
    // nothing left over: nothing to do; the sweep does not change what the tree knows
    CHECK(t.sweep({1, 2}, {2119672224u}).empty() && t.sweep({}, {}).empty());
    CHECK(t.plan({dev(1), dev(2)}, NO_NAMES, g, S, false).empty());
    // a tree that published nothing yet knows nobody: everything found is a leftover
    Tree fresh;
    CHECK(fresh.sweep({1}, {}).size() == (size_t)T_COUNT);
}

static void testAlarmsFirst()
{
    CHECK(alarmTopic("alarm") && alarmTopic("alarming_count") && alarmTopic("devices/7/alarm"));
    CHECK(alarmTopic("lines/2119672224/alarm") && alarmTopic("lines/2119672224/alarming_count"));
    CHECK(!alarmTopic("fault_count") && !alarmTopic("devices/7/location") && !alarmTopic("devices/7/battery_low"));
    CHECK(!alarmTopic("lines/7/name") && !alarmTopic("lines/7/device_count") && !alarmTopic("devices/7/xalarm") && !alarmTopic(""));

    // a full publish of two detectors on one line: the alarm topics lead, in their old order
    Tree t;
    Globals g;
    g.alarming = true;
    g.alarmingCount = 1;
    DeviceSnapshot a = dev(1), b = dev(2);
    b.alarming = true;
    std::vector<Message> m = t.plan({a, b}, NO_NAMES, g, 0, true);
    const size_t total = m.size();
    alarmsFirst(m);
    CHECK(m.size() == total && total == 2 * T_COUNT + L_COUNT + Tree::GLOBAL_COUNT);
    CHECK(m[0].topic == "devices/1/alarm" && m[0].payload == "false");
    CHECK(m[1].topic == "devices/2/alarm" && m[1].payload == "true");
    CHECK(m[2].topic == "lines/2119672224/alarm" && m[3].topic == "lines/2119672224/alarming_count");
    CHECK(m[4].topic == "alarm" && m[4].payload == "true" && m[5].topic == "alarming_count" && m[5].payload == "1");
    CHECK(m[6].topic == "devices/1/location"); // the rest follows as planned
    for (size_t i = 6; i < m.size(); i++)
        CHECK(!alarmTopic(m[i].topic));
}

static void testEnqueueFirst()
{
    // a full publish is queued; an alarm comes in while it drains
    std::deque<Message> q = {{"r/devices/1/alarm", "false"}, {"r/devices/1/location", "Hallway"}, {"r/alarm", "false"}, {"r/fault_count", "0"}};
    enqueueFirst(q, {{"r/devices/1/alarm", "true"}, {"r/alarm", "true"}, {"r/alarming_count", "1"}});
    CHECK(q.size() == 5);
    CHECK(q[0].topic == "r/devices/1/alarm" && q[0].payload == "true");
    CHECK(q[1].topic == "r/alarm" && q[1].payload == "true");
    CHECK(q[2].topic == "r/alarming_count" && q[2].payload == "1");
    // the older values of these topics are gone, the rest keeps its order
    CHECK(q[3].topic == "r/devices/1/location" && q[4].topic == "r/fault_count");

    // the alarm ends before the first one was sent: only the newest value stays
    enqueueFirst(q, {{"r/devices/1/alarm", "false"}});
    CHECK(q.size() == 5 && q[0].topic == "r/devices/1/alarm" && q[0].payload == "false" && q[1].topic == "r/alarm");

    // topics of another root (still being cleared) are not touched
    std::deque<Message> old = {{"old/devices/1/alarm", ""}};
    enqueueFirst(old, {{"new/devices/1/alarm", "true"}});
    CHECK(old.size() == 2 && old[0].topic == "new/devices/1/alarm" && old[1].topic == "old/devices/1/alarm");

    // an empty queue, and nothing to add
    std::deque<Message> e;
    enqueueFirst(e, {{"r/a", "1"}, {"r/b", "2"}});
    CHECK(e.size() == 2 && e[0].topic == "r/a" && e[1].topic == "r/b");
    enqueueFirst(e, {});
    CHECK(e.size() == 2);
}

int main()
{
    testRoot();
    testValues();
    testStatusValues();
    testFirstPublishAndChangeOnly();
    testThrottle();
    testFirstValueIsNotThrottled();
    testClearsUnknownValues();
    testForce();
    testRemovedDevices();
    testClearAll();
    testLines();
    testSweep();
    testAlarmsFirst();
    testEnqueueFirst();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
