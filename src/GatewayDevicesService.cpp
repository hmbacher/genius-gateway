#include <GatewayDevicesService.h>

GatewayDevicesService::GatewayDevicesService(ESP32SvelteKit *sveltekit) : _httpEndpoint(GeniusDevices::read,
                                                                                        GeniusDevices::update,
                                                                                        this,
                                                                                        sveltekit->getServer(),
                                                                                        GATEWAY_DEVICES_SERVICE_PATH,
                                                                                        sveltekit->getSecurityManager(),
                                                                                        AuthenticationPredicates::IS_ADMIN),
                                                                          _fsPersistence(GeniusDevices::read,
                                                                                         GeniusDevices::update,
                                                                                         this,
                                                                                         sveltekit->getFS(),
                                                                                         GATEWAY_DEVICES_FILE)
{
}

void GatewayDevicesService::begin()
{
    _httpEndpoint.begin();
    _fsPersistence.readFromFS();
}

void GatewayDevicesService::setAlarm(uint32_t detectorSN)
{
    bool updated = false;

    beginTransaction();
    for (auto &device : _state.devices)
    {
        if (device.smokeDetector.sn == detectorSN)
        {
            if (!device.isAlarming)
            {
                device.isAlarming = true;
                genius_device_alarm_t alarm = {.startTime = time(nullptr),    // seconds precision
                                                 .endTime = 0,
                                                 .endingReason = HAE_ALARM_ACTIVE};
                device.alarms.push_back(alarm);

                _isAlarming = true;
                updated = true;

                ESP_LOGV(GeniusDevices::TAG, "Alarm started for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
        }
    }
    endTransaction();

    if (updated)
        callUpdateHandlers(ALARM_STATE_CHANGE);
}

void GatewayDevicesService::resetAlarm(uint32_t detectorSN, genius_alarm_ending_t endingReason)
{
    bool updated = false;

    beginTransaction();
    for (auto &device : _state.devices)
    {
        if (device.smokeDetector.sn == detectorSN)
        {
            if (device.isAlarming)
            {
                device.isAlarming = false;
                device.alarms.back().endTime = time(nullptr);   // seconds precision
                device.alarms.back().endingReason = endingReason;

                updated = true;

                ESP_LOGV(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
        }
    }
    endTransaction();

    if (updated) {
        updateAlarmState();
        callUpdateHandlers(ALARM_STATE_CHANGE);
    }
}

void GatewayDevicesService::updateAlarmState()
{
    bool alarming = false;

    beginTransaction(); 
    for (auto &device : _state.devices)
    {
        if (device.isAlarming)
        {
            alarming = true;
            break;
        }
    }
    _isAlarming = alarming;

    endTransaction();
}