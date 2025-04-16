#ifndef GatewaySettingsService_h
#define GatewaySettingsService_h

#include <FSPersistence.h>
#include <HttpEndpoint.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <ESP32SvelteKit.h>
#include <Utils.hpp>

#define GATEWAY_SETTINGS_FILE "/config/gateway-settings.json"
#define GATEWAY_SETTINGS_SERVICE_PATH "/rest/gateway-settings"

#define GATEWAY_SETTINGS_STR_ALERT_ON_UNKNOWN_DETECTORS "alertOnUnknownDetectors"
#define GATEWAY_SETTINGS_DEFAULT_ALERT_ON_UNKNOWN_DETECTORS true

class GatewaySettings
{

public:
    bool alertOnUnknownDetectors = GATEWAY_SETTINGS_DEFAULT_ALERT_ON_UNKNOWN_DETECTORS;

    static void read(GatewaySettings &gwSettings, JsonObject &root)
    {
        root[GATEWAY_SETTINGS_STR_ALERT_ON_UNKNOWN_DETECTORS] = gwSettings.alertOnUnknownDetectors;

        ESP_LOGV(GatewaySettings::TAG, "Gateway settings read.");
    }

    static StateUpdateResult update(JsonObject &root, GatewaySettings &gwSettings)
    {
        boolean newState = root[GATEWAY_SETTINGS_STR_ALERT_ON_UNKNOWN_DETECTORS] | GATEWAY_SETTINGS_DEFAULT_ALERT_ON_UNKNOWN_DETECTORS;
        if (gwSettings.alertOnUnknownDetectors != newState)
        {
            gwSettings.alertOnUnknownDetectors = newState;

            ESP_LOGV(GatewaySettings::TAG, "Gateway settings updated.");
            return StateUpdateResult::CHANGED;
        }
        return StateUpdateResult::UNCHANGED;
    }

private:
    static constexpr const char *TAG = "GatewaySettings";
};

class GatewaySettingsService : public StatefulService<GatewaySettings>
{
public:
    GatewaySettingsService(ESP32SvelteKit *sveltekit);

    void begin();

private:
    HttpEndpoint<GatewaySettings> _httpEndpoint;
    FSPersistence<GatewaySettings> _fsPersistence;
};

#endif // GatewaySettingsService_h