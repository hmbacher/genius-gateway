/**
 * @file GatewaySettingsService.h
 * @brief Gateway settings service for managing gateway configuration options
 */

#ifndef GatewaySettingsService_h
#define GatewaySettingsService_h

#include <FSPersistence.h>
#include <HttpEndpoint.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <ESP32SvelteKit.h>
#include <Utils.hpp>

#define GATEWAY_SETTINGS_FILE "/config/gateway-settings.json"  ///< Configuration file path
#define GATEWAY_SETTINGS_SERVICE_PATH "/rest/gateway-settings" ///< REST API service endpoint path

#define GATEWAY_SETTINGS_STR_ALERT_ON_UNKNOWN_DETECTORS "alert_on_unknown_detectors"                             ///< JSON key for unknown detector alerts
#define GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_COMMISSIONING_PACKET "add_alarm_line_from_commissioning_packet" ///< JSON key for commissioning packet line addition
#define GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_ALARM_PACKET "add_alarm_line_from_alarm_packet"                 ///< JSON key for alarm packet line addition
#define GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_LINE_TEST_PACKET "add_alarm_line_from_line_test_packet"         ///< JSON key for line test packet line addition
#define GATEWAY_SETTINGS_DEFAULT_ALERT_ON_UNKNOWN_DETECTORS true                                                 ///< Default setting for unknown detector alerts
#define GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_COMMISSIONING_PACKET true                                   ///< Default setting for commissioning packet line addition
#define GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_ALARM_PACKET true                                           ///< Default setting for alarm packet line addition
#define GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_LINE_TEST_PACKET true                                       ///< Default setting for line test packet line addition

/// Gateway settings data model class
class GatewaySettings
{

public:
    bool alertOnUnknownDetectors = GATEWAY_SETTINGS_DEFAULT_ALERT_ON_UNKNOWN_DETECTORS;                           ///< Alert when unknown detectors are discovered
    bool addALarmLineFromCommissioningPacket = GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_COMMISSIONING_PACKET; ///< Add alarm lines from commissioning packets
    bool addAlarmLineFromAlarmPacket = GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_ALARM_PACKET;                 ///< Add alarm lines from alarm packets
    bool addAlarmLineFromLineTestPacket = GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_LINE_TEST_PACKET;          ///< Add alarm lines from line test packets

    static void read(GatewaySettings &gwSettings, JsonObject &root)
    {
        root[GATEWAY_SETTINGS_STR_ALERT_ON_UNKNOWN_DETECTORS] = gwSettings.alertOnUnknownDetectors;
        root[GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_COMMISSIONING_PACKET] = gwSettings.addALarmLineFromCommissioningPacket;
        root[GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_ALARM_PACKET] = gwSettings.addAlarmLineFromAlarmPacket;
        root[GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_LINE_TEST_PACKET] = gwSettings.addAlarmLineFromLineTestPacket;

        ESP_LOGV(GatewaySettings::TAG, "Gateway settings read.");
    }

    static StateUpdateResult update(JsonObject &root, GatewaySettings &gwSettings)
    {
        bool updated = false;
        bool newBoolValue;

        /* alert_on_unknown_detectors */
        newBoolValue = root[GATEWAY_SETTINGS_STR_ALERT_ON_UNKNOWN_DETECTORS] | GATEWAY_SETTINGS_DEFAULT_ALERT_ON_UNKNOWN_DETECTORS;
        if (gwSettings.alertOnUnknownDetectors != newBoolValue)
        {
            gwSettings.alertOnUnknownDetectors = newBoolValue;
            updated |= true;
        }

        /* add_alarm_line_from_commissioning_packet */
        newBoolValue = root[GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_COMMISSIONING_PACKET] | GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_COMMISSIONING_PACKET;
        if (gwSettings.addALarmLineFromCommissioningPacket != newBoolValue)
        {
            gwSettings.addALarmLineFromCommissioningPacket = newBoolValue;
            updated |= true;
        }

        /* add_alarm_line_from_alarm_packet */
        newBoolValue = root[GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_ALARM_PACKET] | GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_ALARM_PACKET;
        if (gwSettings.addAlarmLineFromAlarmPacket != newBoolValue)
        {
            gwSettings.addAlarmLineFromAlarmPacket = newBoolValue;
            updated |= true;
        }

        /* add_alarm_line_from_line_test_packet */
        newBoolValue = root[GATEWAY_SETTINGS_STR_ADD_ALARM_LINE_FROM_LINE_TEST_PACKET] | GATEWAY_SETTINGS_DEFAULT_ADD_ALARM_LINE_FROM_LINE_TEST_PACKET;
        if (gwSettings.addAlarmLineFromLineTestPacket != newBoolValue)
        {
            gwSettings.addAlarmLineFromLineTestPacket = newBoolValue;
            updated |= true;
        }

        if (updated)
        {
            ESP_LOGV(GatewaySettings::TAG, "Gateway settings updated.");
            return StateUpdateResult::CHANGED;
        }

        return StateUpdateResult::UNCHANGED;
    }

private:
    static constexpr const char *TAG = "GatewaySettings"; ///< Logging tag
};

/// Service for managing gateway configuration settings
class GatewaySettingsService : public StatefulService<GatewaySettings>
{
public:
    GatewaySettingsService(ESP32SvelteKit *sveltekit);

    /// Initialize the gateway settings service
    void begin();

    /// Check if alerts on unknown detectors are enabled
    bool isAlertOnUnknownDetectorsEnabled()
    {
        beginTransaction();
        bool val = _state.alertOnUnknownDetectors;
        endTransaction();
        return val;
    }

    /// Check if alarm line addition from commissioning packets is enabled
    bool isAddAlarmLineFromCommissioningPacketEnabled()
    {
        beginTransaction();
        bool val = _state.addALarmLineFromCommissioningPacket;
        endTransaction();
        return val;
    }

    /// Check if alarm line addition from alarm packets is enabled
    bool isAddAlarmLineFromAlarmPacketEnabled()
    {
        beginTransaction();
        bool val = _state.addAlarmLineFromAlarmPacket;
        endTransaction();
        return val;
    }

    /// Check if alarm line addition from line test packets is enabled
    bool isAddAlarmLineFromLineTestPacketEnabled()
    {
        beginTransaction();
        bool val = _state.addAlarmLineFromLineTestPacket;
        endTransaction();
        return val;
    }

private:
    HttpEndpoint<GatewaySettings> _httpEndpoint;   ///< REST API endpoint handler
    FSPersistence<GatewaySettings> _fsPersistence; ///< File system persistence handler
};

#endif // GatewaySettingsService_h