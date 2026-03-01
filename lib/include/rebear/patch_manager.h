#pragma once

#include "patch.h"
#include "spi_protocol.h"
#include <map>
#include <string>
#include <vector>

namespace rebear {

/**
 * @brief High-level patch management class
 * 
 * PatchManager provides convenient methods for managing multiple patches,
 * including validation, persistence (JSON file I/O), and batch operations
 * with the FPGA via SPIProtocol.
 * 
 * Features:
 * - Validates patch ID uniqueness (0-15)
 * - Stores patches in memory
 * - Applies all patches to FPGA in one operation
 * - Saves/loads patch configurations to/from JSON files
 * - Clears patches both locally and on FPGA
 */
class PatchManager {
public:
    PatchManager() = default;
    ~PatchManager() = default;

    /**
     * @brief Add a patch to the manager
     * 
     * Validates the patch and adds it to the internal collection.
     * If a patch with the same ID already exists, it will be replaced.
     * 
     * @param patch The patch to add
     * @return true if patch was added successfully, false if validation failed
     */
    bool addPatch(const Patch& patch);

    /**
     * @brief Remove a patch by ID
     * 
     * @param id Patch ID (0-15)
     * @return true if patch was removed, false if ID not found
     */
    bool removePatch(uint8_t id);

    /**
     * @brief Get a patch by ID
     * 
     * @param id Patch ID (0-15)
     * @return Pointer to patch if found, nullptr otherwise
     */
    const Patch* getPatch(uint8_t id) const;

    /**
     * @brief Get all patches
     * 
     * @return Vector of all patches in the manager
     */
    std::vector<Patch> getPatches() const;

    /**
     * @brief Get number of patches
     * 
     * @return Number of patches currently managed
     */
    size_t count() const { return patches_.size(); }

    /**
     * @brief Check if manager has any patches
     * 
     * @return true if no patches are stored
     */
    bool empty() const { return patches_.empty(); }

    /**
     * @brief Apply all patches to FPGA
     * 
     * Sends all patches to the FPGA via the provided SPIProtocol instance.
     * If any patch fails to apply, the operation continues with remaining patches.
     * 
     * @param spi Connected SPIProtocol instance
     * @return true if all patches applied successfully, false if any failed
     */
    bool applyAll(SPIProtocol& spi);

    /**
     * @brief Clear all patches (local and FPGA)
     * 
     * Removes all patches from local storage and clears them on the FPGA.
     * 
     * @param spi Connected SPIProtocol instance
     * @return true if FPGA clear succeeded, false otherwise
     */
    bool clearAll(SPIProtocol& spi);

    /**
     * @brief Clear local patches only (don't touch FPGA)
     * 
     * Removes all patches from local storage without affecting FPGA state.
     */
    void clearLocal();

    /**
     * @brief Save patches to JSON file
     * 
     * File format:
     * {
     *   "patches": [
     *     {
     *       "id": 0,
     *       "address": "0x001000",
     *       "data": "0102030405060708",
     *       "enabled": true
     *     }
     *   ]
     * }
     * 
     * @param filename Path to output file
     * @return true if save succeeded, false on error
     */
    bool saveToFile(const std::string& filename) const;

    /**
     * @brief Load patches from JSON file
     * 
     * Replaces current patches with those loaded from file.
     * If loading fails, current patches are preserved.
     * 
     * @param filename Path to input file
     * @return true if load succeeded, false on error
     */
    bool loadFromFile(const std::string& filename);

    /**
     * @brief Get last error message
     * 
     * @return Description of last error that occurred
     */
    std::string getLastError() const { return lastError_; }

private:
    std::map<uint8_t, Patch> patches_;
    mutable std::string lastError_;

    void setError(const std::string& error) const {
        lastError_ = error;
    }
};

} // namespace rebear
