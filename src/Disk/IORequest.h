#pragma once

#include <cstdint>
#include <vector>
#include "Core/Version.h"

namespace recoverysuite {
namespace disk {

/**
 * @brief Enumeration of I/O request types
 */
enum class IORequestType {
    READ,
    WRITE,
    FLUSH,
    SHUTDOWN
};

/**
 * @brief Structure representing an I/O request to a disk
 *
 * This structure encapsulates a single I/O operation to be performed
 * on a disk device. It contains all the information needed to execute
 * the request.
 */
struct IORequest {
    IORequestType type;          // Type of I/O operation
    uint64_t startSector;        // Starting sector (0-based)
    uint32_t sectorCount;        // Number of sectors to process
    void* buffer;                // Pointer to data buffer (for READ/WRITE)
    uint64_t bufferSize;         // Size of buffer in bytes
    uint32_t flags;              // Optional flags (e.g., for write-through)
    uint64_t timeoutMilliseconds; // Timeout for the operation (0 = default)

    /**
     * @brief Constructor for a read request
     */
    static IORequest read(uint64_t startSector, uint32_t sectorCount,
                         void* buffer, uint64_t bufferSize) {
        IORequest req{};
        req.type = IORequestType::READ;
        req.startSector = startSector;
        req.sectorCount = sectorCount;
        req.buffer = buffer;
        req.bufferSize = bufferSize;
        req.flags = 0;
        req.timeoutMilliseconds = 0;
        return req;
    }

    /**
     * @brief Constructor for a write request
     */
    static IORequest write(uint64_t startSector, uint32_t sectorCount,
                          const void* buffer, uint64_t bufferSize) {
        IORequest req{};
        req.type = IORequestType::WRITE;
        req.startSector = startSector;
        req.sectorCount = sectorCount;
        req.buffer = const_cast<void*>(buffer); // Cast away const for buffer
        req.bufferSize = bufferSize;
        req.flags = 0;
        req.timeoutMilliseconds = 0;
        return req;
    }

    /**
     * @brief Constructor for a flush request
     */
    static IORequest flush() {
        IORequest req{};
        req.type = IORequestType::FLUSH;
        req.startSector = 0;
        req.sectorCount = 0;
        req.buffer = nullptr;
        req.bufferSize = 0;
        req.flags = 0;
        req.timeoutMilliseconds = 0;
        return req;
    }

    /**
     * @brief Constructor for a shutdown request
     */
    static IORequest shutdown() {
        IORequest req{};
        req.type = IORequestType::SHUTDOWN;
        req.startSector = 0;
        req.sectorCount = 0;
        req.buffer = nullptr;
        req.bufferSize = 0;
        req.flags = 0;
        req.timeoutMilliseconds = 0;
        return req;
    }
};

} // namespace disk
} // namespace recoverysuite