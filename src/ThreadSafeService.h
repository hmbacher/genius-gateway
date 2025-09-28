/**
 * @file ThreadSafeService.h
 * @brief Base class providing thread-safe service functionality
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/// Base class providing thread-safe service functionality using recursive mutexes
class ThreadSafeService
{
public:
    ThreadSafeService() : _accessMutex(xSemaphoreCreateRecursiveMutex())
    {
    }

    ~ThreadSafeService()
    {
        if (_accessMutex != nullptr)
        {
            vSemaphoreDelete(_accessMutex);
            _accessMutex = nullptr;
        }
    }

protected:
    /// Begin a thread-safe transaction by acquiring the mutex
    inline void beginTransaction()
    {
        xSemaphoreTakeRecursive(_accessMutex, portMAX_DELAY);
    }

    /// End a thread-safe transaction by releasing the mutex
    inline void endTransaction()
    {
        xSemaphoreGiveRecursive(_accessMutex);
    }

private:
    SemaphoreHandle_t _accessMutex; ///< Recursive mutex for thread synchronization
};
