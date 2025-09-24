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
    
    // Generate unique ID with collision detection
    newDevice.id = _generateUniqueDeviceId();
    
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

                device.published = false; // Mark as not published for MQTT publishing

                updatedDevice = &device;
                _isAlarming = true;
                _numAlarming++;

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

                device.published = false; // Mark as not published for MQTT publishing

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
            device.published = false; // Mark as not published for MQTT publishing

            updated = true;

            ESP_LOGV(GeniusDevices::TAG, "Alarm ended for smoke detector with SN '%lu'.", device.smokeDetector.sn);
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

uint32_t GatewayDevicesService::_generateUniqueDeviceId() const
{
    uint32_t candidateId = (uint32_t)time(nullptr);
    
    // Simple linear scan for collision detection - efficient for small device counts
    while (std::any_of(_state.devices.begin(), _state.devices.end(),
           [candidateId](const GeniusDevice& device) { 
               return device.id == candidateId; 
           })) {
        candidateId++;
    }
    
    return candidateId;
}

bool GatewayDevicesService::isAlarming()
{
    bool isAlarming = false;

    beginTransaction();
    isAlarming = _isAlarming;
    endTransaction();

    return isAlarming;
}

uint32_t GatewayDevicesService::numAlarmingDevices()
{
    uint32_t numAlarming = 0;

    beginTransaction();
    numAlarming = _numAlarming;
    endTransaction();

    return numAlarming;
}

bool GatewayDevicesService::isSmokeDetectorKnown(uint32_t detectorSN)
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

std::vector<DeviceMqttData> GatewayDevicesService::getDevicesMqttData()
{
    std::vector<DeviceMqttData> mqttData;
    beginTransaction();

    // Return data for all devices, that haven't been published since last change
    for (GeniusDevice &dev : _state.devices)
    {
        if (!dev.published)
        {
            mqttData.emplace_back(
                dev.smokeDetector.sn,
                dev.location,
                dev.isAlarming);
        }
    }

    endTransaction();
    return mqttData;
}

void GatewayDevicesService::setPublished(uint32_t smokeDetectorSN)
{
    beginTransaction();
    for (GeniusDevice &device : _state.devices)
    {
        if (device.smokeDetector.sn == smokeDetectorSN)
        {
            device.published = true;
            break;
        }
    }
    endTransaction();
}

StateUpdateResult GeniusDevices::update(JsonObject &root, GeniusDevices &geniusDevices)
{
    bool hasChanges = false;
    
    if (!root["devices"].is<JsonArray>())
    {
        ESP_LOGV(GeniusDevices::TAG, "No devices array in JSON, no changes made.");
        return StateUpdateResult::UNCHANGED;
    }

    JsonArray jsonDevices = root["devices"].as<JsonArray>();
    std::vector<uint32_t> processedDeviceIds; // Track which device IDs we've seen in JSON
    
    // Process each device from JSON - add new or update existing
    int deviceCount = 0;
    for (JsonVariant jsonDeviceArrItem : jsonDevices)
    {
        if (deviceCount++ >= GATEWAY_MAX_DEVICES)
        {
            ESP_LOGE(GeniusDevices::TAG, "Too many smoke detector devices. Maximum allowed is %d.", GATEWAY_MAX_DEVICES);
            break;
        }

        JsonObject smokeDetectorJson = jsonDeviceArrItem["smokeDetector"].as<JsonObject>();
        JsonObject radioModuleJson = jsonDeviceArrItem["radioModule"].as<JsonObject>();
        
        uint32_t deviceId = jsonDeviceArrItem["id"].as<uint32_t>();
        processedDeviceIds.push_back(deviceId);
        
        // Check if device exists by ID
        auto existingDevice = std::find_if(geniusDevices.devices.begin(), geniusDevices.devices.end(),
            [deviceId](const GeniusDevice& device) {
                return device.id == deviceId;
            });
        
        if (existingDevice == geniusDevices.devices.end())
        {
            // New device - add it (use ID from JSON)
            GeniusDevice newDevice = GeniusDevice(
                GeniusComponent<GeniusSmokeDetector>(
                    static_cast<GeniusSmokeDetector>(smokeDetectorJson["model"].as<int>()),
                    smokeDetectorJson["sn"].as<uint32_t>(),
                    Utils::iso8601_to_time_t(smokeDetectorJson["productionDate"].as<String>())),
                GeniusComponent<GeniusRadioModule>(
                    static_cast<GeniusRadioModule>(radioModuleJson["model"].as<int>()),
                    radioModuleJson["sn"].as<uint32_t>(),
                    Utils::iso8601_to_time_t(radioModuleJson["productionDate"].as<String>())),
                jsonDeviceArrItem["location"].as<String>(),
                deviceId); // Use the ID from JSON

            // Set optional properties with defaults
            newDevice.isAlarming = jsonDeviceArrItem["isAlarming"].is<bool>() ? 
                jsonDeviceArrItem["isAlarming"].as<bool>() : false;
            newDevice.registration = jsonDeviceArrItem["registration"].is<int>() ? 
                static_cast<genius_device_registration_t>(jsonDeviceArrItem["registration"].as<int>()) : GDR_MANUAL;
            
            // Process alarms
            if (jsonDeviceArrItem["alarms"].is<JsonArray>())
            {
                int alarms_count = 0;
                for (JsonVariant jsonAlarm : jsonDeviceArrItem["alarms"].as<JsonArray>())
                {
                    if (alarms_count++ >= GATEWAY_MAX_ALARMS)
                    {
                        ESP_LOGE(GeniusDevices::TAG, "Too many alarms for smoke detector device. Maximum allowed is %d.", GATEWAY_MAX_ALARMS);
                        break;
                    }

                    newDevice.alarms.push_back(genius_device_alarm_t{
                        .startTime = Utils::iso8601_to_time_t(jsonAlarm["startTime"].as<String>()),
                        .endTime = Utils::iso8601_to_time_t(jsonAlarm["endTime"].as<String>()),
                        .endingReason = static_cast<genius_alarm_ending_t>(jsonAlarm["endingReason"].as<int>())});
                }
            }
            
            // Mark for republishing
            newDevice.published = false;
            
            // Finally add the new device
            geniusDevices.devices.push_back(newDevice);
            
            hasChanges = true;
        }
        else
        {
            // Update existing device attribute by attribute
            bool deviceChanged = false;
            
            // Update smoke detector component...
            // ...model
            GeniusSmokeDetector newSmokeDetectorModel = static_cast<GeniusSmokeDetector>(smokeDetectorJson["model"].as<int>());
            if (existingDevice->smokeDetector.model != newSmokeDetectorModel)
            {
                existingDevice->smokeDetector.model = newSmokeDetectorModel;
                deviceChanged = true;
            }
            // ...serial number
            uint32_t newSmokeDetectorSN = smokeDetectorJson["sn"].as<uint32_t>();
            if (existingDevice->smokeDetector.sn != newSmokeDetectorSN)
            {
                existingDevice->smokeDetector.sn = newSmokeDetectorSN;
                deviceChanged = true;
            }
            // ...production date
            time_t newSmokeDetectorProdDate = Utils::iso8601_to_time_t(smokeDetectorJson["productionDate"].as<String>());
            if (existingDevice->smokeDetector.productionDate != newSmokeDetectorProdDate)
            {
                existingDevice->smokeDetector.productionDate = newSmokeDetectorProdDate;
                deviceChanged = true;
            }
            
            // Update radio module component...
            // ...model
            GeniusRadioModule newRadioModuleModel = static_cast<GeniusRadioModule>(radioModuleJson["model"].as<int>());
            if (existingDevice->radioModule.model != newRadioModuleModel)
            {
                existingDevice->radioModule.model = newRadioModuleModel;
                deviceChanged = true;
            }
            // ...serial number
            uint32_t newRadioModuleSN = radioModuleJson["sn"].as<uint32_t>();
            if (existingDevice->radioModule.sn != newRadioModuleSN)
            {
                existingDevice->radioModule.sn = newRadioModuleSN;
                deviceChanged = true;
            }
            // ...production date
            time_t newRadioModuleProdDate = Utils::iso8601_to_time_t(radioModuleJson["productionDate"].as<String>());
            if (existingDevice->radioModule.productionDate != newRadioModuleProdDate)
            {
                existingDevice->radioModule.productionDate = newRadioModuleProdDate;
                deviceChanged = true;
            }
            
            // Update location
            String newLocation = jsonDeviceArrItem["location"].as<String>();
            if (existingDevice->location != newLocation)
            {
                existingDevice->location = newLocation;
                deviceChanged = true;
            }
            
            // NOTE: The following attributes are managed internally and should not be updated from JSON:
            // - isAlarming: Controlled by alarm detection system
            // - registration: Set when device is first added/detected  
            // - alarms: Managed by alarm start/stop events
            
            /*
            // Update isAlarming
            bool newIsAlarming = jsonDeviceArrItem["isAlarming"].is<bool>() ? 
                jsonDeviceArrItem["isAlarming"].as<bool>() : false;
            if (existingDevice->isAlarming != newIsAlarming)
            {
                existingDevice->isAlarming = newIsAlarming;
                deviceChanged = true;
            }
            
            // Update registration
            genius_device_registration_t newRegistration = jsonDeviceArrItem["registration"].is<int>() ? 
                static_cast<genius_device_registration_t>(jsonDeviceArrItem["registration"].as<int>()) : GDR_MANUAL;
            if (existingDevice->registration != newRegistration)
            {
                existingDevice->registration = newRegistration;
                deviceChanged = true;
            }
            
            // Update alarms (for simplicity, we replace the entire alarms vector if it has changed)
            // A more sophisticated approach would compare individual alarms
            std::vector<genius_device_alarm_t> newAlarms;
            if (jsonDeviceArrItem["alarms"].is<JsonArray>())
            {
                int alarms_count = 0;
                for (JsonVariant jsonAlarm : jsonDeviceArrItem["alarms"].as<JsonArray>())
                {
                    if (alarms_count++ >= GATEWAY_MAX_ALARMS)
                    {
                        ESP_LOGE(GeniusDevices::TAG, "Too many alarms for smoke detector device. Maximum allowed is %d.", GATEWAY_MAX_ALARMS);
                        break;
                    }

                    newAlarms.push_back(genius_device_alarm_t{
                        .startTime = Utils::iso8601_to_time_t(jsonAlarm["startTime"].as<String>()),
                        .endTime = Utils::iso8601_to_time_t(jsonAlarm["endTime"].as<String>()),
                        .endingReason = static_cast<genius_alarm_ending_t>(jsonAlarm["endingReason"].as<int>())});
                }
            }
            
            // Compare alarms (simple size comparison - could be more sophisticated)
            if (existingDevice->alarms.size() != newAlarms.size())
            {
                existingDevice->alarms = newAlarms;
                deviceChanged = true;
            }
            */
            
            // Mark for republishing if device changed
            if (deviceChanged)
            {
                existingDevice->published = false;
                hasChanges = true;
            }
        }
    }
    
    // Remove devices that are not in the JSON (deleted devices)
    auto it = geniusDevices.devices.begin();
    while (it != geniusDevices.devices.end())
    {
        uint32_t deviceId = it->id;
        bool foundInJson = std::find(processedDeviceIds.begin(), processedDeviceIds.end(), deviceId) != processedDeviceIds.end();
        
        if (!foundInJson)
        {
            ESP_LOGV(GeniusDevices::TAG, "Removed device with ID '%lu' (not found in JSON).", deviceId);
            it = geniusDevices.devices.erase(it);
            hasChanges = true;
        }
        else
        {
            ++it;
        }
    }
    
    ESP_LOGV(GeniusDevices::TAG, "Smoke detector devices configurations updated.");
    
    return hasChanges ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
}