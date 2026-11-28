/// @file WSLogger.cpp
/// @brief Implementation of WebSocket logger for CC1101 packet streaming
/// 
/// @copyright Copyright (c) 2024-2025 Genius Gateway Project
/// @license AGPL-3.0 with Commons Clause
/// 
/// This file is part of Genius Gateway.
/// 
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Affero General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version, with the Commons Clause restriction.
/// 
/// "Commons Clause" License Condition v1.0
/// The Software is provided to you by the Licensor under the License,
/// as defined below, subject to the following condition:
/// Without limiting other conditions in the License, the grant of rights
/// under the License will not include, and the License does not grant to you,
/// the right to Sell the Software.
/// 
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
/// GNU Affero General Public License for more details.
/// 
/// See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.

#include <WSLogger.h>
#include <lwip/sockets.h>

WSLogger::WSLogger(ESP32SvelteKit *sveltekit) : _server(sveltekit->getServer()),
                                                _securityManager(sveltekit->getSecurityManager()),
                                                _settings(sveltekit)
{
    _clientsMutex = xSemaphoreCreateMutex();
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
        _rememberRequest(request);
        return true; });

    // Registring event handler for WebSocket event OnOpen
    _webSocket.onOpen([this](PsychicWebSocketClient *client)
                      {
        // Packets are pushed from the reception task - see logPacket()
        EventSocket::setSendTimeout(client->socket(), WEB_SOCKET_LOGGER_SEND_TIMEOUT_MS);
        transmitId(client);

        // Listed on the WebSocket Logger page until it closes
        const int socket = client->socket();
        WSLoggerClient entry;
        entry.address = client->remoteIP().toString();
        struct sockaddr_in6 peer;
        socklen_t peerSize = sizeof(peer);
        if (getpeername(socket, (struct sockaddr *)&peer, &peerSize) == 0)
            entry.port = ntohs(peer.sin6_port);
        entry.connectedAt = millis();
        if (xSemaphoreTake(_clientsMutex, portMAX_DELAY) == pdTRUE)
        {
            auto pending = _pendingClients.find(socket);
            if (pending != _pendingClients.end())
            {
                entry.name = pending->second.name;
                entry.userAgent = pending->second.userAgent;
                _pendingClients.erase(pending);
            }
            _clients[socket] = entry;
            xSemaphoreGive(_clientsMutex);
        }

        ESP_LOGI(WSLogger::TAG, "ws[%s][%u] connect", client->remoteIP().toString().c_str(), client->socket());
        ESP_LOGV(WSLogger::TAG, "Number of connected clients: %d", _webSocket.count()); });

    // Registering event handler for WebSocket event OnClose
    _webSocket.onClose([this](PsychicWebSocketClient *client)
                       {
        ESP_LOGI(WSLogger::TAG, "ws[%s][%u] disconnect", client->remoteIP().toString().c_str(), client->socket());
        if (xSemaphoreTake(_clientsMutex, portMAX_DELAY) == pdTRUE)
        {
            _clients.erase(client->socket());
            _pendingClients.erase(client->socket());
            xSemaphoreGive(_clientsMutex);
        } });

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

    // Connected clients for the WebSocket Logger page
    _server->on(WEB_SOCKET_LOGGER_CLIENTS_PATH,
                HTTP_GET,
                _securityManager->wrapRequest(
                    [this](PsychicRequest *request) -> esp_err_t
                    {
                        PsychicJsonResponse response(request, false);
                        JsonObject root = response.getRoot().to<JsonObject>();
                        clientsToJson(root);
                        return response.send();
                    },
                    AuthenticationPredicates::IS_ADMIN));

    // Switched off: nothing is sent any more, so the clients do not stay connected for nothing
    _settings.addUpdateHandler([this](const String &originId)
                               {
        if (!_settings.isEnabled())
            _closeAll(); },
                               false);

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

    // Not sendAll(): it stops at the first client that fails and leaves that client connected,
    // so every further packet would wait for it again - in the task that receives the radio
    // packets.
    std::vector<int> stillDropped;
    std::vector<int> sent;
    for (PsychicClient *client : _webSocket.getClientList())
    {
        if (client->_friend == nullptr)
            continue;

        const int socket = client->socket();
        if (std::find(_droppedSockets.begin(), _droppedSockets.end(), socket) != _droppedSockets.end())
        {
            stillDropped.push_back(socket); // disconnect is under way - do not wait for it again
            continue;
        }

        auto *wsClient = static_cast<PsychicWebSocketClient *>(client->_friend);
        if (wsClient->sendMessage(HTTPD_WS_TYPE_BINARY, packet, sizeof(cc1101_packet_t)) != ESP_OK)
        {
            ESP_LOGW(WSLogger::TAG, "ws[%d] did not take a packet - disconnecting the client", socket);
            stillDropped.push_back(socket);
            client->close();
        }
        else
        {
            sent.push_back(socket);
        }
    }
    // Forget sockets that are closed by now; their number may be handed out again.
    _droppedSockets = std::move(stillDropped);

    // Count after sending - the lock is never held while waiting for a client
    if (!sent.empty() && xSemaphoreTake(_clientsMutex, portMAX_DELAY) == pdTRUE)
    {
        for (int socket : sent)
        {
            auto entry = _clients.find(socket);
            if (entry != _clients.end())
            {
                entry->second.packets++;
                entry->second.bytes += sizeof(cc1101_packet_t);
            }
        }
        xSemaphoreGive(_clientsMutex);
    }
}

void WSLogger::clientsToJson(JsonObject &root)
{
    root["enabled"] = _settings.isEnabled();
    JsonArray list = root["clients"].to<JsonArray>();
    const uint32_t now = millis();
    if (xSemaphoreTake(_clientsMutex, portMAX_DELAY) == pdTRUE)
    {
        for (const auto &[socket, c] : _clients)
        {
            JsonObject item = list.add<JsonObject>();
            item["id"] = socket;
            item["address"] = c.address;
            item["port"] = c.port;
            if (!c.name.isEmpty())
                item["name"] = c.name;
            if (!c.userAgent.isEmpty())
                item["userAgent"] = c.userAgent;
            item["connectedMs"] = now - c.connectedAt;
            item["packets"] = c.packets;
            item["bytes"] = c.bytes;
        }
        xSemaphoreGive(_clientsMutex);
    }
}

void WSLogger::_rememberRequest(PsychicRequest *request)
{
    WSLoggerClient entry;
    entry.userAgent = request->header("User-Agent");
    if (entry.userAgent.length() > WEB_SOCKET_LOGGER_USER_AGENT_MAX)
        entry.userAgent = entry.userAgent.substring(0, WEB_SOCKET_LOGGER_USER_AGENT_MAX);

    httpd_req_t *req = request->request();
    const size_t queryLength = httpd_req_get_url_query_len(req);
    if (queryLength > 0 && queryLength < 256)
    {
        char query[queryLength + 1];
        char value[WEB_SOCKET_LOGGER_CLIENT_NAME_MAX * 3 + 1];
        if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
        {
            const esp_err_t found = httpd_query_key_value(query, WEB_SOCKET_LOGGER_CLIENT_PARAM, value, sizeof(value));
            if (found == ESP_OK || found == ESP_ERR_HTTPD_RESULT_TRUNC)
                entry.name = _cleanName(String(value));
        }
    }

    if (xSemaphoreTake(_clientsMutex, portMAX_DELAY) == pdTRUE)
    {
        _pendingClients[request->client()->socket()] = entry;
        xSemaphoreGive(_clientsMutex);
    }
}

void WSLogger::_closeAll()
{
    for (PsychicClient *client : _webSocket.getClientList())
    {
        ESP_LOGI(WSLogger::TAG, "ws[%d] logger switched off - disconnecting the client", client->socket());
        client->close();
    }
}

String WSLogger::_cleanName(const String &raw)
{
    String name;
    for (size_t i = 0; i < raw.length() && name.length() < WEB_SOCKET_LOGGER_CLIENT_NAME_MAX; i++)
    {
        char c = raw[i];
        if (c == '+')
        {
            c = ' ';
        }
        else if (c == '%' && i + 2 < raw.length())
        {
            c = (char)strtol(raw.substring(i + 1, i + 3).c_str(), nullptr, 16);
            i += 2;
        }
        if (c >= 0x20 && c < 0x7F)
            name += c;
    }
    name.trim();
    return name;
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
