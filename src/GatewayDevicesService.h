/**
 * @file GatewayDevicesService.h
 * @brief Gateway devices service for managing genius smoke detector devices
 */

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

#define GATEWAY_DEVICES_FILE "/config/gateway-devices.json"  ///< Configuration file path for device data
#define GATEWAY_DEVICES_SERVICE_PATH "/rest/gateway-devices"  ///< REST API service endpoint path

#define GATEWAY_MAX_DEVICES 50   ///< Maximum number of devices supported
#define GATEWAY_MAX_ALARMS 100   ///< Maximum number of alarms supported

#define ALARM_STATE_CHANGE "alarm-state-change"  ///< WebSocket event for alarm state changes

#define GENIUS_DEVICE_ADDED_FROM_PACKET "genius-device-added-from-packet"  ///< Event for device discovery
#define GENIUS_DEVICE_DEFAULT_LOCATION "Unknown location"  ///< Default location for new devices

/// Lightweight structure for MQTT publishing - contains only the minimal properties needed
struct DeviceMqttData
{
    uint32_t smokeDetectorSN;  ///< Smoke detector serial number
    String location;           ///< Device location string
    bool isAlarming;          ///< Current alarm state

    DeviceMqttData(uint32_t sn, const String &loc, bool alarming)
        : smokeDetectorSN(sn), location(loc), isAlarming(alarming) {}
};

typedef enum genius_alarm_ending
{
    GAE_MIN = -2,              ///< Minimum value (for enum range checks)
    GAE_ALARM_ACTIVE = -1,     ///< Alarm is currently active
    GAE_BY_SMOKE_DETECTOR = 0, ///< Alarm was ended by smoke detector
    GAE_BY_MANUAL,             ///< Alarm was ended manually via web interface
    GAE_MAX                    ///< Maximum value (for enum range checks)
} genius_alarm_ending_t;

typedef struct genius_device_alarm
{
    time_t startTime;                   ///< Alarm start timestamp
    time_t endTime;                     ///< Alarm end timestamp
    genius_alarm_ending_t endingReason; ///< How the alarm was ended
} genius_device_alarm_t;

typedef enum genius_smoke_detector
{
    GSD_UNKNOWN = -1,     ///< Unknown smoke detector type
    GSD_GENIUS_PLUS_X = 0 ///< Genius Plus X smoke detector model
} GeniusSmokeDetector;

typedef enum genius_radio_module
{
    GRM_UNKNOWN = -1,     ///< Unknown radio module type
    GRM_FM_BASIS_X = 0    ///< FM Basis X radio module model
} GeniusRadioModule;

/// Template class for Genius components
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

    T model;                ///< Component model type
    uint32_t sn;           ///< Component serial number
    time_t productionDate; ///< Production date (Unix timestamp)
};

typedef enum genius_device_registration
{
    GDR_MIN = -1,      ///< Boundary check minimum value
    GDR_BUILT_IN = 0,  ///< Device is built-in
    GDR_GENIUS_PACKET, ///< Device was added via received genius packet
    GDR_MANUAL,        ///< Device registered manually (via web interface)
    GDR_MAX            ///< Boundary check maximum value
} genius_device_registration_t;

/// Class for Genius devices
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

    /// Update genius devices from JSON object
    static StateUpdateResult update(JsonObject &root, GeniusDevices &geniusDevices);
};

/// Service for managing gateway devices and smoke detector communication
class GatewayDevicesService : public StatefulService<GeniusDevices>
{
public:
    GatewayDevicesService(ESP32SvelteKit *sveltekit);

    /// Initialize the gateway devices service
    void begin();

    /// Add a genius device based on radio module and smoke detector serial numbers
    bool AddGeniusDevice(const uint32_t snRadioModule,
                         const uint32_t snSmokeDetector);

    /// Set alarm state for a device by detector serial number
    const GeniusDevice *setAlarm(uint32_t detectorSN);

    /// Reset alarm state for a device with specified ending reason
    const GeniusDevice *resetAlarm(uint32_t detectorSN, genius_alarm_ending_t endingReason);

    /// Reset all active alarms
    bool resetAllAlarms();

    /// Check if any device is currently alarming
    bool isAlarming();

    /// Get the number of devices currently alarming
    uint32_t numAlarmingDevices();

    /// Check if a smoke detector is known/registered
    bool isSmokeDetectorKnown(uint32_t detectorSN);

    /// Get minimal device data optimized for MQTT publishing
    std::vector<DeviceMqttData> getDevicesMqttData();

    /// Mark device as published to MQTT
    void setPublished(uint32_t smokeDetectorSN);

private:
    HttpEndpoint<GeniusDevices> _httpEndpoint;   ///< REST API endpoint handler
    FSPersistence<GeniusDevices> _fsPersistence; ///< File system persistence handler
    bool _isAlarming;                            ///< Current global alarming state
    uint32_t _numAlarming;                       ///< Number of devices currently alarming

    /// Update internal alarming state counters
    void _updateAlarmingState();

    /// Generate a unique device ID for new devices
    uint32_t _generateUniqueDeviceId() const;
};

#endif // GatewayDevicesService_h