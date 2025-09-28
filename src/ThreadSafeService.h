#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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
    inline void beginTransaction()
    {
        xSemaphoreTakeRecursive(_accessMutex, portMAX_DELAY);
    }

    inline void endTransaction()
    {
        xSemaphoreGiveRecursive(_accessMutex);
    }

private:
    SemaphoreHandle_t _accessMutex;
};
