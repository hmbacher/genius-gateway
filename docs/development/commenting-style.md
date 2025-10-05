# Commenting Style Guide

This document defines modern, lightweight commenting standards for the Genius Gateway project, optimized for IDE integration and code readability.

!!! info "Purpose"
    This guide serves both human developers and AI assistants, providing clear standards for code documentation that enhance maintainability and collaboration.

## Philosophy

- **Clarity over verbosity**: Comments should enhance understanding, not clutter code
- **IDE integration**: Leverage modern IDE hover tooltips and IntelliSense
- **Self-documenting code**: Prefer meaningful names over extensive comments
- **Strategic documentation**: Focus on WHY and complex HOW, not obvious WHAT

## Modern Approach

### Balance Documentation Needs

=== "Modern (Recommended)"

    ```cpp
    // Clean and IDE-friendly
    class NetworkManager {
        void reconnectWithBackoff(); // Implements exponential backoff for network recovery
        
    private:
        int connectionAttempts;      ///< Current retry count (IDE shows in hover)
        std::string serverAddress;  ///< Target server URL
    };
    ```

=== "Traditional (Avoid)"

    ```cpp
    /**
     * @brief Class for managing network connections
     * @details This class handles network connection management with automatic
     * retry logic and exponential backoff algorithms for improved reliability.
     */
    class NetworkManager {
        /**
         * @brief Reconnect to server with exponential backoff
         * @details Implements exponential backoff algorithm starting with 1s delay
         */
        void reconnectWithBackoff();
    };
    ```

## General Principles

1. **Consistency**: Use the same style throughout each file type
2. **Clarity**: Comments should explain WHY, not just WHAT  
3. **Modern tooling**: Optimize for IDE hover tooltips and IntelliSense
4. **Strategic documentation**: Document public APIs and complex algorithms
5. **Maintenance**: Keep comments up-to-date with code changes

## C++ Files (.cpp, .hpp, .h)

### Function Documentation - Strategic Approach

=== "Public API"

    ```cpp
    /**
     * @brief Processes user input with validation
     * @param input User input string to process  
     * @param maxLength Maximum allowed length [1-255]
     * @return 0 on success, -1 on invalid input
     * @note Thread-safe operation
     */
    int processInput(const std::string& input, size_t maxLength);
    ```

=== "Private/Internal"

    ```cpp
    void validateUserPermissions(); // Checks current user against ACL database
    ```

=== "Complex Algorithms"

    ```cpp
    void optimizedSort() {
        // Using quicksort with median-of-three pivot selection
        // Provides O(n log n) average case with good cache locality
        // Implementation details...
    }
    ```

### Member Variables - IDE Integration

```cpp
class PacketManager {
private:
    // Static members and constants
    static constexpr const char *TAG = "PacketManager"; ///< Logging tag for ESP_LOG functions
    
    // Instance members - IDE shows these in hover tooltips
    int connectionTimeout;           ///< Connection timeout in milliseconds
    std::string serverEndpoint;     ///< Target server URL and port
    bool isConnected;               ///< Current connection status
    std::vector<Packet> sendQueue;  ///< Pending packets for transmission
};
```

### Implementation Comments

```cpp
// In .cpp files: Focus on implementation details and algorithms
void PacketManager::reconnect() {
    // Exponential backoff: 1s, 2s, 4s, 8s, max 30s
    // This prevents thundering herd problems during server outages
    int delay = std::min(1000 * (1 << retryAttempts), 30000);
    
    if (lastConnectionTime + delay > getCurrentTime()) {
        return; // Still in backoff period
    }
    
    // Connection logic here...
}
```

### Single Line Comments

```cpp
// Use double-slash for all single line comments
int count = 0;  // Initialize counter
```

### Multi-Line Comments

```cpp
// Use multiple single-line comments for multi-line explanations
// This approach is preferred in modern C++ as it's safer
// and easier to comment/uncomment blocks
void processData() {
    // Implementation
}
```

### Class Documentation

```cpp
/**
 * @brief Brief description of the class purpose
 * @details Longer description explaining the class design,
 * usage patterns, and important behavioral notes.
 * 
 * This class manages packet transmission with automatic
 * retry logic and error recovery.
 */
class PacketManager {
public:
    // Public interface comments as needed
    
private:
    int _retryCount;  ///< Number of retry attempts remaining
};
```

### File Headers

```cpp
/**
 * @file filename.hpp
 * @brief Brief description of file purpose
 * @details Detailed description of what this file contains,
 * main classes/functions, and overall architecture.
 * 
 * @author Your Name
 * @date 2025-10-05
 * @copyright Copyright (C) 2025 Your Company
 * @license MIT License
 */

#pragma once

// System includes
#include <system_header>

// Project includes  
#include "project_header.h"

// === Constants ===
#define MAX_BUFFER_SIZE 1024  // Maximum buffer size in bytes
```

### Macro Definitions

=== "Simple Macros"

    ```cpp
    #define MAX_RETRIES 3  // Maximum number of retry attempts
    ```

=== "Complex Macros"

    ```cpp
    /**
     * @brief Extracts 32-bit value from buffer with network byte order conversion
     * @param buffer Pointer to buffer containing data
     * @param pos Byte offset in buffer (must be <= buffer_size - 4)
     * @warning No bounds checking performed - caller must validate pos
     */
    #define EXTRACT32(buffer, pos) (__ntohl(*(uint32_t *)&buffer[pos]))
    ```

## C Files (.c, .h)

### Single Line Comments

```c
/* Use C-style comments for all single line comments */
int count = 0;  /* Initialize counter */
```

### Multi-Line Comments

```c
/*
 * Use traditional C-style block comments
 * for multi-line explanations and documentation.
 * This ensures compatibility with older C standards.
 */
void processData(void) {
    /* Implementation */
}
```

### Function Documentation

```c
/**
 * @brief Brief description of the function
 * @param param_name Description of parameter
 * @param another_param Description with constraints [range: 0-255]
 * @return Description of return value and error codes
 * @retval 0 Success
 * @retval -1 Invalid parameter
 * @retval -2 Memory allocation failed
 * @note Use snake_case for C function parameters
 * @warning Critical warnings about usage
 */
int my_function(int param_name, const char* another_param);
```

### File Headers

```c
/**
 * @file filename.c
 * @brief Brief description of file purpose
 * @details Detailed description of what this file contains,
 * main functions, and overall purpose.
 * 
 * @author Your Name
 * @date 2025-10-05
 * @copyright Copyright (C) 2025 Your Company
 * @license MIT License
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>

/* Project includes */
#include "project_header.h"

/* === Constants === */
#define MAX_BUFFER_SIZE 1024  /* Maximum buffer size in bytes */

/* === Static Variables === */
static int s_retry_count = 0;  /* Global retry counter */
```

### Struct Documentation

```c
/**
 * @brief Brief description of struct purpose
 * @details Longer description of struct usage and constraints
 */
typedef struct {
    int retry_count;     /* Number of retries remaining */
    char* buffer;        /* Data buffer pointer */
    size_t buffer_size;  /* Size of allocated buffer */
} packet_context_t;
```

## Enforcement Rules

!!! warning "Required Documentation"
    1. **File Headers**: REQUIRED for all .c, .cpp, .h, .hpp files
    2. **Public API Documentation**: REQUIRED for all public functions/methods
    3. **Complex Logic**: REQUIRED comments for algorithms > 10 lines
    4. **Magic Numbers**: REQUIRED explanation for all numeric constants
    5. **TODO/FIXME**: Must include date and author name

### TODO Format

```cpp
// TODO(username, 2025-10-05): Implement error recovery logic
// FIXME(username, 2025-10-05): Memory leak in cleanup function
// HACK(username, 2025-10-05): Temporary workaround for issue #123
```

## Modern Lightweight Summary

### ✅ Recommended Approach

```cpp
class NetworkManager {
public:
    void reconnect();                    // Simple function - minimal comment
    esp_err_t initialize(config_t cfg); // Complex API - document parameters elsewhere
    
private:
    static constexpr const char *TAG = "NetworkManager"; ///< Logging tag for ESP_LOG
    int connectionTimeout;               ///< Connection timeout in milliseconds
    bool isConnected;                   ///< Current connection status
    std::vector<Packet> sendQueue;     ///< Pending packets for transmission
};
```

### ❌ Avoid Over-Documentation

```cpp
// Too verbose - avoid this approach
class NetworkManager {
private:
    /** @brief Connection timeout value in milliseconds */
    int connectionTimeout;
    /** @brief Boolean flag indicating current connection status */
    bool isConnected;  
    /** @brief Vector container holding pending packet objects for transmission */
    std::vector<Packet> sendQueue;
};
```

### Key Benefits

- **IDE Integration**: `///<` provides hover tooltips in VS Code, CLion, Visual Studio
- **Minimal Overhead**: Only 4 characters vs 13+ for full Doxygen blocks
- **Professional**: Follows industry best practices for embedded C++
- **Future-Proof**: Compatible with documentation generation if needed later
- **Readable**: Clean code that doesn't sacrifice clarity for brevity

!!! tip "IDE Support"
    This commenting style is optimized for modern IDEs and provides excellent IntelliSense/hover tooltip integration while keeping the code clean and readable.

---

*This modern style guide reflects 2025 best practices for embedded C++ development with optimal IDE integration.*