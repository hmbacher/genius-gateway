/// @file WSLogger.cpp
/// @brief Implementation of WebSocket logger for CC1101 packet streaming

#include <WSLogger.h>

WSLogger::WSLogger(ESP32SvelteKit *sveltekit) : _server(sveltekit->getServer()),
                                                _securityManager(sveltekit->getSecurityManager()),
                                                _settings(sveltekit)
{
}

void WSLogger::begin()
{
    // Initialize/Load settings
    _settings.begin();

    // Check if connection is allowed
    _webSocket.setFilter([this](PsychicRequest *request) -> bool
                         {
        // Check if WSLogger is enabled
        if (!_settings.isEnabled())
        {
            // If WebSocket Logger is disabled, reject the connection
            ESP_LOGV(WSLogger::TAG, "WebSocket Logger is disabled, rejecting connection.");
            return false;
        }
        
        // Check security
        auto securityFilter = _securityManager->filterRequest(AuthenticationPredicates::NONE_REQUIRED);
        if (!securityFilter(request))
            return false;

        // Finally accept the connection
        return true; });

    // Registring event handler for WebSocket event OnOpen
    _webSocket.onOpen([this](PsychicWebSocketClient *client)
                      {
        transmitId(client);

        ESP_LOGI(WSLogger::TAG, "ws[%s][%u] connect", client->remoteIP().toString().c_str(), client->socket());
        ESP_LOGV(WSLogger::TAG, "Number of connected clients: %d", _webSocket.count()); });

    // Registering event handler for WebSocket event OnClose
    _webSocket.onClose([this](PsychicWebSocketClient *client)
                       { ESP_LOGI(WSLogger::TAG, "ws[%s][%u] disconnect", client->remoteIP().toString().c_str(), client->socket()); });

    // Registering event handler for WebSocket event OnFrame
    _webSocket.onFrame([this](PsychicWebSocketRequest *request, httpd_ws_frame *frame) -> esp_err_t
                       {
        ESP_LOGV(WSLogger::TAG, "ws[%s][%u] opcode[%d]", request->client()->remoteIP().toString().c_str(), request->client()->socket(), frame->type);

        if (frame->type == HTTPD_WS_TYPE_BINARY)
        {
            ESP_LOGV(WSLogger::TAG, "ws[%s][%u] request: %s", request->client()->remoteIP().toString().c_str(), request->client()->socket(), (char *)frame->payload);
        }
        return ESP_OK; });

    // Register the WebSocket handler at the web server
    _server->on(WEB_SOCKET_LOGGER_PATH, &_webSocket);

    ESP_LOGV(WSLogger::TAG, "Registered WebSocket handler: %s", WEB_SOCKET_LOGGER_PATH);
}

String WSLogger::clientId(PsychicWebSocketClient *client)
{
    return WEB_SOCKET_LOGGER_ORIGIN_CLIENT_ID_PREFIX + String(client->socket());
}

void WSLogger::logPacket(cc1101_packet_t *packet)
{
    if (!_settings.isEnabled())
        return;

    _webSocket.sendAll(HTTPD_WS_TYPE_BINARY, packet, sizeof(cc1101_packet_t));
}

void WSLogger::transmitId(PsychicWebSocketClient *client)
{
    JsonDocument jsonDocument;
    JsonObject root = jsonDocument.to<JsonObject>();
    root["type"] = "id";
    root["id"] = clientId(client);

    // serialize the json to a string
    String buffer;
    serializeJson(jsonDocument, buffer);
    client->sendMessage(buffer.c_str());
}
