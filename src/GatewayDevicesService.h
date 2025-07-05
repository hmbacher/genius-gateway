#ifndef GatewayDevicesService_h
#define GatewayDevicesService_h

#include <EventSocket.h>
#include <FSPersistence.h>
#include <HttpEndpoint.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <ESP32SvelteKit.h>
#include <Utils.hpp>

#define GATEWAY_DEVICES_FILE "/config/gateway-devices.json"
#define GATEWAY_DEVICES_SERVICE_PATH "/rest/gateway-devices"

#define GATEWAY_MAX_DEVICES 50
#define GATEWAY_MAX_ALARMS 100

#define ALARM_STATE_CHANGE "alarm-state-change"

#define GENIUS_DEVICE_ADDED_FROM_PACKET "genius-device-added-from-packet"
#define GENIUS_DEVICE_DEFAULT_LOCATION "Unknown location"

// Lightweight structure for MQTT publishing - contains only the minimal properties needed
struct DeviceMqttData
{
    uint32_t smokeDetectorSN;
    String location;
    bool isAlarming;

    DeviceMqttData(uint32_t sn, const String &loc, bool alarming)
        : smokeDetectorSN(sn), location(loc), isAlarming(alarming) {}
};

typedef enum genius_alarm_ending
{
    HAE_MIN = -2,              // Minimum value (for enum range checks)
    HAE_ALARM_ACTIVE = -1,     // Alarm is currently active
    HAE_BY_SMOKE_DETECTOR = 0, // Alarm was ended by smoke detector
    HAE_BY_MANUAL_RESET,       // Alarm was ended by manual reset
    HAE_MAX                    // Maximum value (for enum range checks)
} genius_alarm_ending_t;

typedef struct genius_device_alarm
{
    time_t startTime;
    time_t endTime;
    genius_alarm_ending_t endingReason;
} genius_device_alarm_t;

typedef enum genius_smoke_detector
{
    HSD_UNKNOWN = -1, // Unknown smoke detector type
    HSD_GENIUS_PLUS_X = 0
} GeniusSmokeDetector;

typedef enum genius_radio_module
{
    HRM_UNKNOWN = -1, // Unknown radio module type
    HRM_FM_BASIS_X = 0
} GeniusRadioModule;

/**
 * @brief Template class for Genius components
 * @tparam T The type of the model attribute
 */
template <typename T>
class GeniusComponent
{
public:
    GeniusComponent(const T &model,
                    uint32_t sn,
                    time_t productionDate) : model(model),
                                             sn(sn),
                                             productionDate(productionDate)
    {
    }

    void toJson(JsonObject &root)
    {
        // Serial number
        root["sn"] = sn;
        // Production date (if any set)
        if (productionDate > 0)
            root["productionDate"] = Utils::time_t_to_iso8601(productionDate);
        // Model (if any set)
        if (static_cast<int>(model) != -1)
            root["model"] = static_cast<int>(model);
    }

    T model;
    uint32_t sn;
    time_t productionDate; // Production date in seconds since Unix Epoch (UTC)
};

typedef enum genius_device_registration
{
    GDR_MIN = -1,      // Just for boundary checks
    GDR_BUILT_IN = 0,  // Device is built-in
    GDR_GENIUS_PACKET, // Device was added via received genius packet
    GDR_MANUAL,        // Device registered manually (via web interface)
    GDR_MAX            // Just for boundary checks
} genius_device_registration_t;

/**
 * @brief Class for Genius devices
 */
class GeniusDevice
{
public:
    GeniusDevice(const GeniusComponent<GeniusSmokeDetector> &smokeDetector,
                 const GeniusComponent<GeniusRadioModule> &radioModule,
                 const String &location) : smokeDetector(smokeDetector),
                                           radioModule(radioModule),
                                           location(location),
                                           isAlarming(false),
                                           registration(GDR_MANUAL)

    {
    }

    void toJson(JsonObject &root)
    {
        // Smoke detector
        JsonObject smokeDetector = root["smokeDetector"].to<JsonObject>();
        this->smokeDetector.toJson(smokeDetector);
        // Radio module
        JsonObject radioModule = root["radioModule"].to<JsonObject>();
        this->radioModule.toJson(radioModule);
        // Location
        root["location"] = this->location;
        // Registration
        root["registration"] = this->registration;
        // Alarms
        JsonArray alarms = root["alarms"].to<JsonArray>();
        for (auto &alarm : this->alarms)
        {
            JsonObject alarm_as_json = alarms.add<JsonObject>();

            alarm_as_json["startTime"] = Utils::time_t_to_iso8601(alarm.startTime);
            alarm_as_json["endTime"] = Utils::time_t_to_iso8601(alarm.endTime);
            alarm_as_json["endingReason"] = alarm.endingReason;
        }
    }

    GeniusComponent<GeniusSmokeDetector> smokeDetector;
    GeniusComponent<GeniusRadioModule> radioModule;
    String location;
    std::vector<genius_device_alarm_t> alarms;
    genius_device_registration_t registration;
    bool isAlarming;
};

class GeniusDevices
{

public:
    static constexpr const char *TAG = "GeniusDevices";

    std::vector<GeniusDevice> devices;

    static void read(GeniusDevices &geniusDevices, JsonObject &root)
    {
        JsonArray jsonDevices = root["devices"].to<JsonArray>();
        for (auto &device : geniusDevices.devices)
        {
            JsonObject jsonDevice = jsonDevices.add<JsonObject>();
            device.toJson(jsonDevice);
        }

        ESP_LOGV(GeniusDevices::TAG, "Smoke detector devices configurations read.");
    }

    static StateUpdateResult update(JsonObject &root, GeniusDevices &geniusDevices)
    {
        if (root["devices"].is<JsonArray>())
        {
            geniusDevices.devices.clear();

            // iterate over devices
            int i = 0;
            for (JsonVariant jsonDeviceArrItem : root["devices"].as<JsonArray>())
            {
                if (i++ >= GATEWAY_MAX_DEVICES)
                {
                    ESP_LOGE(GeniusDevices::TAG, "Too many smoke detector devices. Maximum allowed is %d.", GATEWAY_MAX_DEVICES);
                    break;
                }

                JsonObject smokeDetector = jsonDeviceArrItem["smokeDetector"].as<JsonObject>();
                JsonObject radioModule = jsonDeviceArrItem["radioModule"].as<JsonObject>();

                GeniusDevice newDevice = GeniusDevice(
                    GeniusComponent<GeniusSmokeDetector>(
                        static_cast<GeniusSmokeDetector>(smokeDetector["model"].as<int>()),
                        smokeDetector["sn"].as<uint32_t>(),
                        Utils::iso8601_to_time_t(smokeDetector["productionDate"].as<String>())),
                    GeniusComponent<GeniusRadioModule>(
                        static_cast<GeniusRadioModule>(radioModule["model"].as<int>()),
                        radioModule["sn"].as<uint32_t>(),
                        Utils::iso8601_to_time_t(radioModule["productionDate"].as<String>())),
                    jsonDeviceArrItem["location"].as<String>());

                // Registration type
                if (jsonDeviceArrItem["registration"].is<int>())
                    newDevice.registration = static_cast<genius_device_registration_t>(jsonDeviceArrItem["registration"].as<int>());
                else
                    newDevice.registration = GDR_MANUAL; // Default to manual registration if not specified

                // Logged Alarms
                if (jsonDeviceArrItem["alarms"].is<JsonArray>())
                {
                    // iterate over alarms
                    int alarms_count = 0;
                    for (JsonVariant jsonAlarm : jsonDeviceArrItem["alarms"].as<JsonArray>())
                    {
                        if (alarms_count++ >= GATEWAY_MAX_ALARMS)
                        {
                            ESP_LOGE(GeniusDevices::TAG, "Too many alarms for a smoke detector devices. Maximum allowed is %d.", GATEWAY_MAX_ALARMS);
                            break;
                        }

                        newDevice.alarms.push_back(genius_device_alarm_t{
                            .startTime = Utils::iso8601_to_time_t(jsonAlarm["startTime"].as<String>()),
                            .endTime = Utils::iso8601_to_time_t(jsonAlarm["endTime"].as<String>()),
                            .endingReason = static_cast<genius_alarm_ending_t>(jsonAlarm["endingReason"].as<int>())});

                        alarms_count++;
                    }
                }

                geniusDevices.devices.push_back(newDevice);

                ESP_LOGV(GeniusDevices::TAG, "Added smoke detector with SN '%lu'.", geniusDevices.devices.back().smokeDetector.sn);

                i++;
            }
        }

        ESP_LOGV(GeniusDevices::TAG, "Smoke detector devices configurations updated.");

        return StateUpdateResult::CHANGED;
    }
};

class GatewayDevicesService : public StatefulService<GeniusDevices>
{
public:
    GatewayDevicesService(ESP32SvelteKit *sveltekit);

    void begin();

    void AddGeniusDevice(const uint32_t snRadioModule,
                         const uint32_t snSmokeDetector);

    void setAlarm(uint32_t detectorSN);

    void resetAlarm(uint32_t detectorSN, genius_alarm_ending_t endingReason);

    bool isAlarming()
    {
        bool isAlarming = false;

        beginTransaction();
        isAlarming = _isAlarming;
        endTransaction();

        return isAlarming;
    }

    bool isSmokeDetectorKnown(uint32_t detectorSN)
    {
        bool found = false;
        beginTransaction();
        for (auto &device : _state.devices)
        {
            if (device.smokeDetector.sn == detectorSN)
            {
                found = true;
                break;
            }
        }
        endTransaction();
        return found;
    }

    // Optimized method for MQTT publishing - returns only minimal data needed
    std::vector<DeviceMqttData> getDevicesMqttData()
    {
        std::vector<DeviceMqttData> mqttData;

        beginTransaction();
        mqttData.reserve(_state.devices.size()); // Pre-allocate for efficiency
        for (const auto &device : _state.devices)
        {
            mqttData.emplace_back(
                device.smokeDetector.sn,
                device.location,
                device.isAlarming);
        }
        endTransaction();

        return mqttData;
    }

private:
    HttpEndpoint<GeniusDevices> _httpEndpoint;
    FSPersistence<GeniusDevices> _fsPersistence;
    bool _isAlarming = false;

    void updateAlarmState();
};

#endif // GatewayDevicesService_h