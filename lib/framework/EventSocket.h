#ifndef Socket_h
#define Socket_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <PsychicHttp.h>
#include <SecurityManager.h>
#include <StatefulService.h>
#include <list>
#include <map>
#include <vector>

#define EVENT_SERVICE_PATH "/ws/events"

// How long emitEvent() may wait for one client to take a frame. The HTTP server's own send
// timeout is meant for slow HTTP responses and far too long here: events are sent from the
// task that emits them, and that task must not stand still because a browser tab is asleep.
#ifndef EVENT_SOCKET_SEND_TIMEOUT_MS
#define EVENT_SOCKET_SEND_TIMEOUT_MS 1000
#endif

typedef std::function<void(JsonObject &root, int originId)> EventCallback;
typedef std::function<void(const String &originId)> SubscribeCallback;

class EventSocket
{
public:
    EventSocket(PsychicHttpServer *server, SecurityManager *_securityManager, AuthenticationPredicate authenticationPredicate = AuthenticationPredicates::IS_AUTHENTICATED);

    void begin();

    void registerEvent(String event);

    void onEvent(String event, EventCallback callback);

    void onSubscribe(String event, SubscribeCallback callback);

    /**
     * @brief Send an event to its subscribers
     *
     * Sends from the calling task, one client after the other. A client that does not take
     * the frame within EVENT_SOCKET_SEND_TIMEOUT_MS is disconnected and loses its
     * subscriptions; a browser reconnects and subscribes again by itself. So a call can
     * take up to that time per unresponsive client, once.
     *
     * @param onlyToSameOrigin true: send to originId only; false: send to all subscribers
     *                         except originId
     */
    void emitEvent(String event, JsonObject &jsonObject, const char *originId = "", bool onlyToSameOrigin = false);

    /**
     * @brief Limit how long a send on this socket may block
     *
     * For WebSocket handlers that push data from a task of their own, like emitEvent() does.
     */
    static void setSendTimeout(int socket, uint32_t timeoutMs);

    bool isEventValid(String event);

    unsigned int getConnectedClients();

private:
    PsychicHttpServer *_server;
    PsychicWebSocketHandler _socket;
    SecurityManager *_securityManager;
    AuthenticationPredicate _authenticationPredicate;

    std::vector<String> events;
    std::map<String, std::list<int>> client_subscriptions;
    std::map<String, std::list<EventCallback>> event_callbacks;
    std::map<String, std::list<SubscribeCallback>> subscribe_callbacks;
    void handleEventCallbacks(String event, JsonObject &jsonObject, int originId);
    void handleSubscribeCallbacks(String event, const String &originId);

    /// Remove these sockets from every subscription list. Caller holds clientSubscriptionsMutex.
    void removeSubscriptions(const std::vector<int> &sockets);

    void onWSOpen(PsychicWebSocketClient *client);
    void onWSClose(PsychicWebSocketClient *client);
    esp_err_t onFrame(PsychicWebSocketRequest *request, httpd_ws_frame *frame);
};

#endif
