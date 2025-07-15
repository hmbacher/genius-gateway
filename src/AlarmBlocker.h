/**
 * @file AlarmBlocker.h
 * @brief AlarmBlocker service for temporarily blocking alarm notifications
 * 
 * This service provides functionality to temporarily block alarm notifications
 * for a specified duration. It maintains thread-safe state management and
 * provides WebSocket events for real-time status updates.
 */

#pragma once

#include <ESP32SvelteKit.h>
#include <EventSocket.h>
#include <JsonUtils.h>
#include <SecurityManager.h>
#include <PsychicHttp.h>
#include <ThreadSafeService.h>

/** @brief WebSocket event name for remaining block time updates */
#define ALARMBLOCKER_EVENT_REMAINING_BLOCK_TIME "rem-alarm-block-time"  // Remaining block time in seconds

/** @brief Loop processing period in milliseconds */
#define ALARMBLOCKER_LOOP_PERIOD_MS 1000 // 1 second

/**
 * @class AlarmBlocker
 * @brief Service for managing temporary alarm blocking functionality
 * 
 * The AlarmBlocker class provides a thread-safe service for temporarily blocking
 * alarm notifications. It supports:
 * - Time-based blocking with automatic expiration
 * - Manual blocking control (start/stop)
 * - Real-time status updates via WebSocket events
 * - Thread-safe state management
 * 
 * @extends ThreadSafeService
 */
class AlarmBlocker : public ThreadSafeService
{
public:
    /**
     * @brief Construct a new AlarmBlocker object
     * 
     * Initializes the AlarmBlocker service with references to the ESP32SvelteKit
     * framework components and sets initial state to unblocked.
     * 
     * @param sveltekit Pointer to the ESP32SvelteKit framework instance
     */
    AlarmBlocker(ESP32SvelteKit *sveltekit);

    /**
     * @brief Initialize the AlarmBlocker service
     * 
     * Registers WebSocket events and adds the loop function to the framework.
     * This method must be called during application initialization.
     */
    void begin();

    /**
     * @brief Main service loop function
     * 
     * Processes the blocking timer, automatically unblocks when time expires,
     * and emits status updates. Called periodically by the framework.
     */
    void loop();

    /**
     * @brief Start blocking alarms for a specified duration
     * 
     * Activates alarm blocking for the specified number of seconds.
     * Thread-safe operation that can be called from any context.
     * 
     * @param seconds Duration to block alarms (in seconds)
     */
    void startBlocking(uint32_t seconds)
    {
        beginTransaction();
        _isBlocked = true;
        _remainingBlockingTimeMS = seconds * 1000; // Convert seconds to milliseconds
        endTransaction();
    }

    /**
     * @brief Immediately end alarm blocking
     * 
     * Stops alarm blocking immediately, regardless of remaining time.
     * Thread-safe operation that can be called from any context.
     * 
     * @return esp_err_t Always returns ESP_OK
     */
    esp_err_t endBlocking()
    {
        beginTransaction();
        _isBlocked = false;
        _remainingBlockingTimeMS = 0;
        endTransaction();

        return ESP_OK;
    }

    /**
     * @brief Check if alarms are currently blocked
     * 
     * Thread-safe method to query the current blocking status.
     * 
     * @return true if alarms are currently blocked
     * @return false if alarms are not blocked
     */
    bool isBlocked()
    {
        bool isBlocked;
        beginTransaction();
        isBlocked = _isBlocked;
        endTransaction();
        return isBlocked;
    }

private:
    /** @brief Logging tag for ESP_LOG functions */
    static constexpr const char *TAG = "AlarmBlocker";

    /** @brief Pointer to the ESP32SvelteKit framework instance */
    ESP32SvelteKit *_sveltekit;
    
    /** @brief Pointer to the HTTP server instance */
    PsychicHttpServer *_server;
    
    /** @brief Pointer to the security manager instance */
    SecurityManager *_securityManager;
    
    /** @brief Pointer to the WebSocket event manager */
    EventSocket *_eventSocket;

    /** @brief Last time (in milliseconds) the loop() function was processed */
    volatile uint32_t _lastLooped;

    /** @brief Current blocking state flag */
    bool _isBlocked;
    
    /** @brief Remaining blocking time in milliseconds */
    uint32_t _remainingBlockingTimeMS;

    /**
     * @brief Emit remaining blocking time via WebSocket
     * 
     * Sends a WebSocket event with the current blocking status and remaining time.
     * 
     * @note This method is thread-safe and uses proper transaction locking
     *       to protect shared state access.
     */
    void _emitRemainingBlockingTime();
};