#ifndef AlarmStateService_h
#define AlarmStateService_h

#include <PsychicHttp.h>
#include <SecurityManager.h>
#include <EventSocket.h>
#include <JsonUtils.h>
#include <ESP32SvelteKit.h>

#define ALARMSTATE_SERVICE_PATH "/rest/alarm"
#define EVENT_ALARM "alarm"

class AlarmState
{
public:
    std::vector<uint32_t> _alarmingDevices;

    static void read(AlarmState &alarmState, JsonObject &root)
    {
        JsonArray devices = root["alarmingDevices"].to<JsonArray>();
        for (auto &device : alarmState._alarmingDevices)
        {
            devices.add(device);
        }
    }

private:
    static constexpr const char *TAG = "AlarmState";
};

class AlarmStateService : public StatefulService<AlarmState>
{
public:
    AlarmStateService(ESP32SvelteKit *sveltekit);

    void begin();

    bool isAlarming(uint32_t sn)
    {
        for (auto &alarming_dev_sn : _state._alarmingDevices)
        {
            if (alarming_dev_sn == sn)
                return true;
        }
        return false;
    }

private:
    static constexpr const char *TAG = "AlarmStateService";

    PsychicHttpServer *_server;
    SecurityManager *_securityManager;
    EventSocket *_socket;

    void _loop();                                                                            // For testing purposes
    static void _loopImpl(void *_this) { static_cast<AlarmStateService *>(_this)->_loop(); } // For testing purposes

    void _emitAlarm();
};

#endif // AlarmStateService_h