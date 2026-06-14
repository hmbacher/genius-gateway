#ifndef PsramAllocator_h
#define PsramAllocator_h

#include <cstddef>
#include <cstdlib>
#include <new>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

/// std::allocator-compatible allocator that prefers PSRAM (SPIRAM) when available,
/// falling back to the regular heap otherwise. Intended for transient buffers
/// large enough that internal RAM pressure matters (e.g. import staging).
template <class T>
struct PsramAllocator
{
    using value_type = T;

    PsramAllocator() noexcept = default;
    template <class U> PsramAllocator(const PsramAllocator<U> &) noexcept {}

    T *allocate(std::size_t n)
    {
        const std::size_t bytes = n * sizeof(T);
        void *p = nullptr;
        if (psramFound())
            p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!p)
            p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
        if (!p)
        {
            // ESP-IDF builds may have C++ exceptions disabled; abort matches the OOM
            // behaviour of normal malloc-based allocators in that configuration.
            abort();
        }
        return static_cast<T *>(p);
    }

    void deallocate(T *p, std::size_t) noexcept
    {
        heap_caps_free(p);
    }
};

template <class T, class U>
bool operator==(const PsramAllocator<T> &, const PsramAllocator<U> &) noexcept { return true; }
template <class T, class U>
bool operator!=(const PsramAllocator<T> &, const PsramAllocator<U> &) noexcept { return false; }

#endif // PsramAllocator_h
