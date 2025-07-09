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

void GatewayDevicesService::AddGeniusDevice(const uint32_t snRadioModule,
                                            const uint32_t snSmokeDetector)
{
    beginTransaction();

    // Create a new GeniusDevice with unknown models and production dates
    GeniusDevice newDevice = GeniusDevice(
        GeniusComponent<GeniusSmokeDetector>(static_cast<GeniusSmokeDetector>(GSD_UNKNOWN), snSmokeDetector, 0),
        GeniusComponent<GeniusRadioModule>(static_cast<GeniusRadioModule>(GRM_UNKNOWN), snRadioModule, 0),
        GENIUS_DEVICE_ADDED_FROM_PACKET);
    // Set registration type
    newDevice.registration = GDR_GENIUS_PACKET;
    // Add the new device to the state
    _state.devices.push_back(newDevice);

    endTransaction();
    callUpdateHandlers(GENIUS_DEVICE_ADDED_FROM_PACKET);
}

bool GatewayDevicesService::setAlarm(uint32_t detectorSN)
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
                genius_device_alarm_t alarm = {.startTime = time(nullptr), // seconds precision
                                               .endTime = 0,
                                               .endingReason = GAE_ALARM_ACTIVE};
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

    return updated;
}

bool GatewayDevicesService::resetAlarm(uint32_t detectorSN, genius_alarm_ending_t endingReason)
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
                device.alarms.back().endTime = time(nullptr); // seconds precision
                device.alarms.back().endingReason = endingReason;

                updated = true;

                ESP_LOGV(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
        }
    }
    endTransaction();

    if (updated)
    {
        updateAlarmState();
        callUpdateHandlers(ALARM_STATE_CHANGE);
    }

    return updated;
}

bool GatewayDevicesService::resetAllAlarms()
{
    bool updated = false;

    beginTransaction();
    for (auto &device : _state.devices)
    {
            if (device.isAlarming)
            {
                device.isAlarming = false;
                device.alarms.back().endTime = time(nullptr); // seconds precision
                device.alarms.back().endingReason = GAE_BY_MANUAL;

                updated = true;

                ESP_LOGV(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
    }
    endTransaction();

    if (updated)
    {
        updateAlarmState();
        callUpdateHandlers(ALARM_STATE_CHANGE);
    }

    return updated;
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