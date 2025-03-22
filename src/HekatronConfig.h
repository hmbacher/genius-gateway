#ifndef HekatronSettings_h
#define HekatronSettings_h

#include <StatefulService.h>

class HekatronSettings
{

public:
    static constexpr const char *TAG = "HekatronSettings";

    bool allowUnknowDevices;

    static void read(HekatronSettings &hekatronSettings, JsonObject &root)
    {
        root["allow_unknow_devices"] = hekatronSettings.allowUnknowDevices;

        ESP_LOGV(HekatronSettings::TAG, "Hekatron settings read.");
    }

    static StateUpdateResult update(JsonObject &root, HekatronSettings &hekatronSettings)
    {
        bool newSettingValue = root["allow_unknow_devices"];
        if (newSettingValue != hekatronSettings.allowUnknowDevices)
        {
            hekatronSettings.allowUnknowDevices = newSettingValue;
            ESP_LOGV(HekatronSettings::TAG, "Hekatron settings updated.");
            return StateUpdateResult::CHANGED;
        }

        ESP_LOGV(HekatronSettings::TAG, "No changes in settings.");

        return StateUpdateResult::UNCHANGED;
    }    
};

#endif // HekatronSettings_h