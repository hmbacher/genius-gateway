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
    GAE_MIN = -2,              // Minimum value (for enum range checks)
    GAE_ALARM_ACTIVE = -1,     // Alarm is currently active
    GAE_BY_SMOKE_DETECTOR = 0, // Alarm was ended by smoke detector
    GAE_BY_MANUAL,             // Alarm was ended manually via web interface
    GAE_MAX                    // Maximum value (for enum range checks)
} genius_alarm_ending_t;

typedef struct genius_device_alarm
{
    time_t startTime;
    time_t endTime;
    genius_alarm_ending_t endingReason;
} genius_device_alarm_t;

typedef enum genius_smoke_detector
{
    GSD_UNKNOWN = -1, // Unknown smoke detector type
    GSD_GENIUS_PLUS_X = 0
} GeniusSmokeDetector;

typedef enum genius_radio_module
{
    GRM_UNKNOWN = -1, // Unknown radio module type
    GRM_FM_BASIS_X = 0
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
                 const String &location,
                 uint32_t id = 0) : smokeDetector(smokeDetector),
                                   radioModule(radioModule),
                                   location(location),
                                   id(id), // Use provided ID (from JSON) or will be set by service
                                   registration(GDR_MANUAL),
                                   isAlarming(false),
                                   published(false)
    {
    }

    void toJson(JsonObject &root)
    {
        // Device ID (for internal tracking, not user-editable)
        root["id"] = this->id;
        // Smoke detector
        JsonObject smokeDetector = root["smokeDetector"].to<JsonObject>();
        this->smokeDetector.toJson(smokeDetector);
        // Radio module
        JsonObject radioModule = root["radioModule"].to<JsonObject>();
        this->radioModule.toJson(radioModule);
        // Location
        root["location"] = this->location;
        // Is alarming?
        root["isAlarming"] = this->isAlarming;
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

    uint32_t id; // Unique identifier for device (auto-generated, immutable)
    GeniusComponent<GeniusSmokeDetector> smokeDetector;
    GeniusComponent<GeniusRadioModule> radioModule;
    String location;
    std::vector<genius_device_alarm_t> alarms;
    genius_device_registration_t registration;
    bool isAlarming;
    bool published; // Whether the current device configuration has been published via MQTT
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

    static StateUpdateResult update(JsonObject &root, GeniusDevices &geniusDevices);
};

class GatewayDevicesService : public StatefulService<GeniusDevices>
{
public:
    GatewayDevicesService(ESP32SvelteKit *sveltekit);

    void begin();

    bool AddGeniusDevice(const uint32_t snRadioModule,
                         const uint32_t snSmokeDetector);

    const GeniusDevice *setAlarm(uint32_t detectorSN);

    const GeniusDevice *resetAlarm(uint32_t detectorSN, genius_alarm_ending_t endingReason);

    bool resetAllAlarms();

    bool isAlarming();

    uint32_t numAlarmingDevices();

    bool isSmokeDetectorKnown(uint32_t detectorSN);

    // Optimized method for MQTT publishing - returns only minimal data needed
    std::vector<DeviceMqttData> getDevicesMqttData();

    void setPublished(uint32_t smokeDetectorSN);

private:
    HttpEndpoint<GeniusDevices> _httpEndpoint;
    FSPersistence<GeniusDevices> _fsPersistence;
    bool _isAlarming;
    uint32_t _numAlarming;

    void _updateAlarmingState();
    uint32_t _generateUniqueDeviceId() const;
};

#endif // GatewayDevicesService_h