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
                                                                                         GATEWAY_DEVICES_FILE),
                                                                          _isAlarming(false),
                                                                          _numAlarming(0)
{
}

void GatewayDevicesService::begin()
{
    _httpEndpoint.begin();
    _fsPersistence.readFromFS();

    /* Update alarming state after every device update */
    this->addUpdateHandler([&](const String &originId)
                           { _updateAlarmingState(); },
                           false);
}

bool GatewayDevicesService::AddGeniusDevice(const uint32_t snRadioModule,
                                            const uint32_t snSmokeDetector)
{
    beginTransaction();

    // Create a new GeniusDevice with unknown models and production dates
    GeniusDevice newDevice = GeniusDevice(
        GeniusComponent<GeniusSmokeDetector>(static_cast<GeniusSmokeDetector>(GSD_UNKNOWN), snSmokeDetector, 0),
        GeniusComponent<GeniusRadioModule>(static_cast<GeniusRadioModule>(GRM_UNKNOWN), snRadioModule, 0),
        GENIUS_DEVICE_DEFAULT_LOCATION);
    // Set registration type
    newDevice.registration = GDR_GENIUS_PACKET;
    // Add the new device to the state
    _state.devices.push_back(newDevice);

    endTransaction();

    callUpdateHandlers(GENIUS_DEVICE_ADDED_FROM_PACKET);
    
    return true;
}

const GeniusDevice *GatewayDevicesService::setAlarm(uint32_t detectorSN)
{
    GeniusDevice *updatedDevice = nullptr;

    beginTransaction();
    for (GeniusDevice &device : _state.devices)
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
                _numAlarming++;
                updatedDevice = &device;

                ESP_LOGV(GeniusDevices::TAG, "Alarm started for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
        }
    }
    endTransaction();

    if (updatedDevice)
        callUpdateHandlers(ALARM_STATE_CHANGE);

    return updatedDevice;
}

const GeniusDevice *GatewayDevicesService::resetAlarm(uint32_t detectorSN, genius_alarm_ending_t endingReason)
{
    GeniusDevice *updatedDevice = nullptr;

    beginTransaction();
    for (GeniusDevice &device : _state.devices)
    {
        if (device.smokeDetector.sn == detectorSN)
        {
            if (device.isAlarming)
            {
                device.isAlarming = false;
                device.alarms.back().endTime = time(nullptr); // seconds precision
                device.alarms.back().endingReason = endingReason;

                updatedDevice = &device;
                _numAlarming--;

                ESP_LOGV(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", detectorSN);
            }
            break;
        }
    }
    
    if (_numAlarming == 0)
        _isAlarming = false;

    endTransaction();

    if (updatedDevice)
        callUpdateHandlers(ALARM_STATE_CHANGE);

    return updatedDevice;
}

bool GatewayDevicesService::resetAllAlarms()
{
    bool updated = false;

    beginTransaction();
    for (GeniusDevice &device : _state.devices)
    {
        if (device.isAlarming)
        {
            device.isAlarming = false;
            device.alarms.back().endTime = time(nullptr); // seconds precision
            device.alarms.back().endingReason = GAE_BY_MANUAL;

            updated = true;

            ESP_LOGV(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", detectorSN);
        }
    }
    _isAlarming = false;
    _numAlarming = 0;
    endTransaction();

    if (updated)
        callUpdateHandlers(ALARM_STATE_CHANGE);

    return updated;
}

void GatewayDevicesService::_updateAlarmingState()
{
    bool isAlarming = false;
    uint32_t numAlarming = 0;

    for (const GeniusDevice &device : _state.devices)
    {
        if (device.isAlarming)
        {
            isAlarming = true;
            numAlarming++;
        }
    }

    beginTransaction();
    _isAlarming = isAlarming;
    _numAlarming = numAlarming;
    endTransaction();
}