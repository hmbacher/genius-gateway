#include <EventSocket.h>

#include <lwip/sockets.h>

SemaphoreHandle_t clientSubscriptionsMutex = xSemaphoreCreateMutex();

EventSocket::EventSocket(PsychicHttpServer *server,
                         SecurityManager *securityManager,
                         AuthenticationPredicate authenticationPredicate) : _server(server),
                                                                            _securityManager(securityManager),
                                                                            _authenticationPredicate(authenticationPredicate)
{
}

void EventSocket::begin()
{
    _socket.setFilter(_securityManager->filterRequest(_authenticationPredicate));
    _socket.onOpen((std::bind(&EventSocket::onWSOpen, this, std::placeholders::_1)));
    _socket.onClose(std::bind(&EventSocket::onWSClose, this, std::placeholders::_1));
    _socket.onFrame(std::bind(&EventSocket::onFrame, this, std::placeholders::_1, std::placeholders::_2));
    _server->on(EVENT_SERVICE_PATH, &_socket);

    ESP_LOGV(SVK_TAG, "Registered event socket endpoint: %s", EVENT_SERVICE_PATH);
}

void EventSocket::registerEvent(String event)
{
    if (!isEventValid(event))
    {
        ESP_LOGD(SVK_TAG, "Registering event: %s", event.c_str());
        events.push_back(event);
    }
    else
    {
        ESP_LOGW(SVK_TAG, "Event already registered: %s", event.c_str());
    }
}

void EventSocket::setSendTimeout(int socket, uint32_t timeoutMs)
{
    struct timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0)
    {
        ESP_LOGW(SVK_TAG, "ws[%d] could not set the send timeout (errno %d)", socket, errno);
    }
}

void EventSocket::removeSubscriptions(const std::vector<int> &sockets)
{
    for (auto &event_subscriptions : client_subscriptions)
    {
        for (int socket : sockets)
        {
            event_subscriptions.second.remove(socket);
        }
    }
}

void EventSocket::onWSOpen(PsychicWebSocketClient *client)
{
    setSendTimeout(client->socket(), EVENT_SOCKET_SEND_TIMEOUT_MS);
    ESP_LOGI(SVK_TAG, "ws[%s][%u] connect", client->remoteIP().toString().c_str(), client->socket());
}

void EventSocket::onWSClose(PsychicWebSocketClient *client)
{
    xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
    for (auto &event_subscriptions : client_subscriptions)
    {
        event_subscriptions.second.remove(client->socket());
    }
    xSemaphoreGive(clientSubscriptionsMutex);
    ESP_LOGI(SVK_TAG, "ws[%s][%u] disconnect", client->remoteIP().toString().c_str(), client->socket());
}

esp_err_t EventSocket::onFrame(PsychicWebSocketRequest *request, httpd_ws_frame *frame)
{
    ESP_LOGV(SVK_TAG, "ws[%s][%u] opcode[%d]", request->client()->remoteIP().toString().c_str(),
             request->client()->socket(), frame->type);

    JsonDocument doc;
#if FT_ENABLED(EVENT_USE_JSON)
    if (frame->type == HTTPD_WS_TYPE_TEXT)
    {
        ESP_LOGV(SVK_TAG, "ws[%s][%u] request: %s", request->client()->remoteIP().toString().c_str(),
                 request->client()->socket(), (char *)frame->payload);

        DeserializationError error = deserializeJson(doc, (char *)frame->payload, frame->len);
#else
    if (frame->type == HTTPD_WS_TYPE_BINARY)
    {
        ESP_LOGV(SVK_TAG, "ws[%s][%u] request: %s", request->client()->remoteIP().toString().c_str(),
                 request->client()->socket(), (char *)frame->payload);

        DeserializationError error = deserializeMsgPack(doc, (char *)frame->payload, frame->len);
#endif

        if (!error && doc.is<JsonObject>())
        {
            String event = doc["event"];
            if (event == "subscribe")
            {
                // only subscribe to events that are registered
                if (isEventValid(doc["data"].as<String>()))
                {
                    // The lists are walked by emitEvent() in other tasks. The callbacks run
                    // without the mutex: they usually emit the current state themselves.
                    xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
                    client_subscriptions[doc["data"]].push_back(request->client()->socket());
                    xSemaphoreGive(clientSubscriptionsMutex);
                    handleSubscribeCallbacks(doc["data"], String(request->client()->socket()));
                }
                else
                {
                    ESP_LOGW(SVK_TAG, "Client tried to subscribe to unregistered event: %s", doc["data"].as<String>().c_str());
                }
            }
            else if (event == "unsubscribe")
            {
                xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
                client_subscriptions[doc["data"]].remove(request->client()->socket());
                xSemaphoreGive(clientSubscriptionsMutex);
            }
            else
            {
                JsonObject jsonObject = doc["data"].as<JsonObject>();
                handleEventCallbacks(event, jsonObject, request->client()->socket());
            }
            return ESP_OK;
        }
        ESP_LOGW(SVK_TAG, "Error[%d] parsing JSON: %s", error, (char *)frame->payload);
    }
    return ESP_OK;
}

void EventSocket::emitEvent(String event, JsonObject &jsonObject, const char *originId, bool onlyToSameOrigin)
{
    // Only process valid events
    if (!isEventValid(String(event)))
    {
        ESP_LOGW(SVK_TAG, "Method tried to emit unregistered event: %s", event);
        return;
    }

    int originSubscriptionId = originId[0] ? atoi(originId) : -1;
    xSemaphoreTake(clientSubscriptionsMutex, portMAX_DELAY);
    auto &subscriptions = client_subscriptions[event];
    if (subscriptions.empty())
    {
        xSemaphoreGive(clientSubscriptionsMutex);
        return;
    }

    JsonDocument doc;
    doc["event"] = event;
    doc["data"] = jsonObject;

#if FT_ENABLED(EVENT_USE_JSON)
    size_t len = measureJson(doc);
#else
    size_t len = measureMsgPack(doc);
#endif

    char *output = new char[len + 1];

#if FT_ENABLED(EVENT_USE_JSON)
    serializeJson(doc, output, len + 1);
#else
    serializeMsgPack(doc, output, len);
#endif

    // null terminate the string
    output[len] = '\0';

#if FT_ENABLED(EVENT_USE_JSON)
    const httpd_ws_type_t frameType = HTTPD_WS_TYPE_TEXT;
#else
    const httpd_ws_type_t frameType = HTTPD_WS_TYPE_BINARY;
#endif

    // Sockets to drop afterwards - the list must not change while it is walked.
    std::vector<int> gone;   // no client behind the subscription any more
    std::vector<int> failed; // client did not take the frame in time

    auto sendTo = [&](int subscription)
    {
        auto *client = _socket.getClient(subscription);
        if (!client)
        {
            gone.push_back(subscription);
            return;
        }
        ESP_LOGV(SVK_TAG, "Emitting event: %s to %s[%u], Message[%d]: %s", event, client->remoteIP().toString().c_str(), client->socket(), len, output);
        if (client->sendMessage(frameType, output, len) != ESP_OK)
        {
            // The send timed out or the connection is broken. A frame may have gone out
            // half-way, so the connection cannot be used any more either way.
            ESP_LOGW(SVK_TAG, "ws[%d] did not take event '%s' - disconnecting the client", subscription, event.c_str());
            failed.push_back(subscription);
            client->close();
        }
    };

    // if onlyToSameOrigin == true, send the message back to the origin
    if (onlyToSameOrigin && originSubscriptionId > 0)
    {
        sendTo(originSubscriptionId);
    }
    else
    { // else send the message to all other clients
        for (int subscription : subscriptions)
        {
            if (subscription == originSubscriptionId)
                continue;
            sendTo(subscription);
        }
    }

    // A client that failed once is not waited for again: it loses all its subscriptions now,
    // not only when the HTTP server gets round to closing the connection.
    removeSubscriptions(gone);
    removeSubscriptions(failed);

    delete[] output;
    xSemaphoreGive(clientSubscriptionsMutex);
}

void EventSocket::handleEventCallbacks(String event, JsonObject &jsonObject, int originId)
{
    for (auto &callback : event_callbacks[event])
    {
        callback(jsonObject, originId);
    }
}

void EventSocket::handleSubscribeCallbacks(String event, const String &originId)
{
    for (auto &callback : subscribe_callbacks[event])
    {
        callback(originId);
    }
}

void EventSocket::onEvent(String event, EventCallback callback)
{
    if (!isEventValid(event))
    {
        ESP_LOGW(SVK_TAG, "Method tried to register unregistered event: %s", event.c_str());
        return;
    }
    event_callbacks[event].push_back(callback);
}

void EventSocket::onSubscribe(String event, SubscribeCallback callback)
{
    if (!isEventValid(event))
    {
        ESP_LOGW(SVK_TAG, "Method tried to subscribe to unregistered event: %s", event.c_str());
        return;
    }
    subscribe_callbacks[event].push_back(callback);
    ESP_LOGI(SVK_TAG, "onSubscribe for event: %s", event.c_str());
}

bool EventSocket::isEventValid(String event)
{
    return std::find(events.begin(), events.end(), event) != events.end();
}

unsigned int EventSocket::getConnectedClients()
{
    return (unsigned int)_socket.getClientList().size();
}
